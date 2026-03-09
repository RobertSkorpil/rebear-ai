#include "rebear/network_client.h"
#include <cstring>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    
    using ssize_t = SSIZE_T;
    using socklen_t = int;
    #define close closesocket
    #define SHUT_RDWR SD_BOTH
    #ifndef EINPROGRESS
        #define EINPROGRESS WSAEWOULDBLOCK
    #endif
    #ifndef EINTR
        #define EINTR WSAEINTR
    #endif
    
    static int get_last_error() {
        return WSAGetLastError();
    }
    
    static std::string get_error_string(int error) {
        char* msg = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, nullptr);
        std::string result = msg ? msg : "Unknown error";
        LocalFree(msg);
        return result;
    }
    
    class WinsockInit {
    public:
        WinsockInit() {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        }
        ~WinsockInit() {
            WSACleanup();
        }
    };
    static WinsockInit winsock_init;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <errno.h>
    
    static int get_last_error() {
        return errno;
    }
    
    static std::string get_error_string(int error) {
        return std::strerror(error);
    }
#endif

namespace rebear {

NetworkClient::NetworkClient(const std::string& host, uint16_t port)
#ifdef _WIN32
    : socket_fd_(INVALID_SOCKET)
#else
    : socket_fd_(-1)
#endif
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
#ifdef _WIN32
    if (socket_fd_ == INVALID_SOCKET) {
#else
    if (socket_fd_ < 0) {
#endif
        setError("Failed to create socket: " + get_error_string(get_last_error()));
        return false;
    }
    
    // Set non-blocking for connect timeout
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket_fd_, FIONBIO, &mode);
#else
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);
#endif
    
    // Resolve hostname
    struct addrinfo hints, *result_addrinfo = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &result_addrinfo) != 0) {
        setError("Failed to resolve hostname: " + host_);
        close(socket_fd_);
#ifdef _WIN32
        socket_fd_ = INVALID_SOCKET;
#else
        socket_fd_ = -1;
#endif
        return false;
    }
    
    // Attempt connection
    int connect_result = ::connect(socket_fd_, result_addrinfo->ai_addr, (socklen_t)result_addrinfo->ai_addrlen);
    freeaddrinfo(result_addrinfo);
    
    if (connect_result < 0) {
        int error = get_last_error();
#ifdef _WIN32
        if (error != WSAEWOULDBLOCK) {
#else
        if (error != EINPROGRESS) {
#endif
            setError("Connection failed: " + get_error_string(error));
            close(socket_fd_);
#ifdef _WIN32
            socket_fd_ = INVALID_SOCKET;
#else
            socket_fd_ = -1;
#endif
            return false;
        }
        
        // Wait for connection with timeout
#ifdef _WIN32
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd_, &write_fds);
        
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        int select_result = select(0, nullptr, &write_fds, nullptr, &tv);
        if (select_result <= 0) {
#else
        struct pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLOUT;
        
        int select_result = poll(&pfd, 1, timeout_ms);
        if (select_result <= 0) {
#endif
            setError(select_result == 0 ? "Connection timeout" : "Connection failed");
            close(socket_fd_);
#ifdef _WIN32
            socket_fd_ = INVALID_SOCKET;
#else
            socket_fd_ = -1;
#endif
            return false;
        }
        
        // Check if connection succeeded
        int sock_error = 0;
        socklen_t len = sizeof(sock_error);
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, (char*)&sock_error, &len) < 0 || sock_error != 0) {
            setError("Connection failed: " + get_error_string(sock_error));
            close(socket_fd_);
#ifdef _WIN32
            socket_fd_ = INVALID_SOCKET;
#else
            socket_fd_ = -1;
#endif
            return false;
        }
    }
    
    // Set back to blocking mode
#ifdef _WIN32
    u_long mode_blocking = 0;
    ioctlsocket(socket_fd_, FIONBIO, &mode_blocking);
#else
    fcntl(socket_fd_, F_SETFL, flags);
#endif
    
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
#ifdef _WIN32
    if (socket_fd_ != INVALID_SOCKET) {
        close(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
    }
#else
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
#endif
    
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
    
#ifdef _WIN32
    if (socket_fd_ == INVALID_SOCKET) {
#else
    if (socket_fd_ < 0) {
#endif
        setError("Socket not open");
        return false;
    }
    
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t result = send(socket_fd_, (const char*)(data.data() + sent), (int)(data.size() - sent), 0);
        if (result < 0) {
            int error = get_last_error();
            if (error == EINTR) {
                continue;
            }
            setError("Send failed: " + get_error_string(error));
            connected_ = false;
            return false;
        }
        sent += result;
    }
    
    return true;
}

bool NetworkClient::receiveMessage(protocol::Message& msg, int timeout_ms) {
    // Wait for data with timeout
#ifdef _WIN32
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd_, &read_fds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(0, &read_fds, nullptr, nullptr, &tv);
    if (result <= 0) {
        if (result < 0) {
            int error = get_last_error();
            if (error != EINTR) {
                setError("Select failed: " + get_error_string(error));
                connected_ = false;
            }
        }
        return false;
    }
#else
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
#endif
    
    // Read header first
    std::vector<uint8_t> header(protocol::MIN_MESSAGE_SIZE);
    size_t received = 0;
    
    while (received < protocol::MIN_MESSAGE_SIZE) {
        ssize_t result = recv(socket_fd_, (char*)(header.data() + received), 
                             (int)(protocol::MIN_MESSAGE_SIZE - received), 0);
        if (result <= 0) {
            int error = get_last_error();
            if (result == 0 || error != EINTR) {
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
            ssize_t result = recv(socket_fd_, (char*)(full_message.data() + protocol::MIN_MESSAGE_SIZE + received),
                                 (int)(remaining - received), 0);
            if (result <= 0) {
                int error = get_last_error();
                if (result == 0 || error != EINTR) {
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
