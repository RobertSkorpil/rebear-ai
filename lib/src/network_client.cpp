#include "rebear/network_client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <chrono>

namespace rebear {

NetworkClient::NetworkClient(const std::string& host, uint16_t port)
    : socket_fd_(-1)
    , host_(host)
    , port_(port)
    , connected_(false)
    , running_(false)
{
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(int timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        return true;
    }
    
    // Create socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        setError("Failed to create socket");
        return false;
    }
    
    // Set non-blocking for connect timeout
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);
    
    // Resolve hostname
    struct hostent* server = gethostbyname(host_.c_str());
    if (server == nullptr) {
        setError("Failed to resolve hostname: " + host_);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // Setup server address
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port_);
    
    // Attempt connection
    int result = ::connect(socket_fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    
    if (result < 0) {
        if (errno != EINPROGRESS) {
            setError("Connection failed: " + std::string(strerror(errno)));
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // Wait for connection with timeout
        struct pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLOUT;
        
        result = poll(&pfd, 1, timeout_ms);
        if (result <= 0) {
            setError(result == 0 ? "Connection timeout" : "Connection failed");
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // Check if connection succeeded
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            setError("Connection failed: " + std::string(strerror(error)));
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
    }
    
    // Set back to blocking mode
    fcntl(socket_fd_, F_SETFL, flags);
    
    // Start receive thread
    connected_ = true;
    running_ = true;
    recv_thread_ = std::thread(&NetworkClient::receiveLoop, this);
    
    return true;
}

void NetworkClient::disconnect() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_) {
            return;
        }
        
        connected_ = false;
        running_ = false;
    }
    
    // Wake up any waiting threads
    response_cv_.notify_all();
    
    // Close socket
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
    // Wait for receive thread
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

bool NetworkClient::isConnected() const {
    return connected_;
}

bool NetworkClient::sendRequest(uint8_t type, 
                                const std::vector<uint8_t>& payload,
                                std::vector<uint8_t>& response, 
                                int timeout_ms) {
    if (!connected_) {
        setError("Not connected");
        return false;
    }
    
    // Create message
    protocol::Message msg(type, payload);
    
    // Send message
    if (!sendMessage(msg)) {
        return false;
    }
    
    // Determine expected response type
    uint8_t response_type = type + 1;  // Convention: response is command + 1
    
    // Wait for response
    return waitForResponse(response_type, response, timeout_ms);
}

std::string NetworkClient::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

void NetworkClient::receiveLoop() {
    while (running_) {
        protocol::Message msg;
        if (receiveMessage(msg, 100)) {  // 100ms timeout for checking running_ flag
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Store response
            auto& pending = pending_responses_[msg.type];
            pending.received = true;
            pending.data = msg.payload;
            
            // Notify waiting threads
            response_cv_.notify_all();
        }
    }
}

bool NetworkClient::sendMessage(const protocol::Message& msg) {
    std::vector<uint8_t> data = protocol::encodeMessage(msg);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (socket_fd_ < 0) {
        setError("Socket not open");
        return false;
    }
    
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t result = send(socket_fd_, data.data() + sent, data.size() - sent, 0);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            setError("Send failed: " + std::string(strerror(errno)));
            connected_ = false;
            return false;
        }
        sent += result;
    }
    
    return true;
}

bool NetworkClient::receiveMessage(protocol::Message& msg, int timeout_ms) {
    // Wait for data with timeout
    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;
    
    int result = poll(&pfd, 1, timeout_ms);
    if (result <= 0) {
        if (result < 0 && errno != EINTR) {
            setError("Poll failed: " + std::string(strerror(errno)));
            connected_ = false;
        }
        return false;
    }
    
    // Read header first
    std::vector<uint8_t> header(protocol::MIN_MESSAGE_SIZE);
    size_t received = 0;
    
    while (received < protocol::MIN_MESSAGE_SIZE) {
        ssize_t result = recv(socket_fd_, header.data() + received, 
                             protocol::MIN_MESSAGE_SIZE - received, 0);
        if (result <= 0) {
            if (result == 0 || errno != EINTR) {
                setError("Connection closed");
                connected_ = false;
                return false;
            }
            continue;
        }
        received += result;
    }
    
    // Decode header to get message length
    if (header[0] != protocol::MAGIC_BYTE_1 || header[1] != protocol::MAGIC_BYTE_2) {
        setError("Invalid magic bytes");
        connected_ = false;
        return false;
    }
    
    uint16_t length = (static_cast<uint16_t>(header[2]) << 8) | header[3];
    
    if (length < protocol::MIN_MESSAGE_SIZE || length > protocol::MAX_MESSAGE_SIZE) {
        setError("Invalid message length");
        connected_ = false;
        return false;
    }
    
    // Read remaining data
    std::vector<uint8_t> full_message = header;
    if (length > protocol::MIN_MESSAGE_SIZE) {
        size_t remaining = length - protocol::MIN_MESSAGE_SIZE;
        full_message.resize(length);
        received = 0;
        
        while (received < remaining) {
            ssize_t result = recv(socket_fd_, full_message.data() + protocol::MIN_MESSAGE_SIZE + received,
                                 remaining - received, 0);
            if (result <= 0) {
                if (result == 0 || errno != EINTR) {
                    setError("Connection closed");
                    connected_ = false;
                    return false;
                }
                continue;
            }
            received += result;
        }
    }
    
    // Decode message
    return protocol::decodeMessage(full_message, msg);
}

void NetworkClient::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
}

bool NetworkClient::waitForResponse(uint8_t response_type, 
                                    std::vector<uint8_t>& response, 
                                    int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Clear any previous response
    pending_responses_[response_type].received = false;
    
    // Wait for response with timeout
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    
    while (running_ && connected_) {
        auto& pending = pending_responses_[response_type];
        if (pending.received) {
            response = pending.data;
            pending.received = false;  // Clear for next request
            return true;
        }
        
        if (response_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            setError("Response timeout");
            return false;
        }
    }
    
    setError("Connection lost");
    return false;
}

} // namespace rebear
