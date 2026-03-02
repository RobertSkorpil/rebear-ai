#include "network_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <poll.h>

namespace rebear {
namespace server {

NetworkServer::NetworkServer(uint16_t port, int max_clients)
    : port_(port)
    , max_clients_(max_clients)
    , server_fd_(-1)
    , running_(false)
{
}

NetworkServer::~NetworkServer() {
    stop();
}

bool NetworkServer::start() {
    if (running_) {
        return true;
    }
    
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        setError("Failed to create socket");
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        setError("Failed to set socket options");
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // Bind socket
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        setError("Failed to bind socket to port " + std::to_string(port_));
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // Listen
    if (listen(server_fd_, max_clients_) < 0) {
        setError("Failed to listen on socket");
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // Start accept thread
    running_ = true;
    accept_thread_ = std::thread(&NetworkServer::acceptLoop, this);
    
    std::cout << "Server started on port " << port_ << std::endl;
    return true;
}

void NetworkServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Close server socket to unblock accept()
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    // Wait for accept thread
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    // Wait for all client threads
    for (auto& thread : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads_.clear();
    
    std::cout << "Server stopped" << std::endl;
}

void NetworkServer::acceptLoop() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Use poll to check for incoming connections with timeout
        struct pollfd pfd;
        pfd.fd = server_fd_;
        pfd.events = POLLIN;
        
        int result = poll(&pfd, 1, 1000);  // 1 second timeout
        if (result < 0) {
            if (errno != EINTR) {
                std::cerr << "Poll error: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        if (result == 0) {
            // Timeout - check running_ flag
            continue;
        }
        
        // Accept connection
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "Accept error: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        std::cout << "Client connected" << std::endl;
        
        // Handle client in new thread
        client_threads_.emplace_back(&NetworkServer::handleClient, this, client_fd);
    }
}

void NetworkServer::handleClient(int client_fd) {
    while (running_) {
        // Read message header
        std::vector<uint8_t> header(protocol::MIN_MESSAGE_SIZE);
        size_t received = 0;
        
        while (received < protocol::MIN_MESSAGE_SIZE) {
            // Use poll to check for data with timeout
            struct pollfd pfd;
            pfd.fd = client_fd;
            pfd.events = POLLIN;
            
            int result = poll(&pfd, 1, 1000);  // 1 second timeout
            if (result < 0) {
                if (errno != EINTR) {
                    std::cerr << "Poll error: " << strerror(errno) << std::endl;
                    break;
                }
                continue;
            }
            
            if (result == 0) {
                // Timeout - check running_ flag
                if (!running_) {
                    break;
                }
                continue;
            }
            
            ssize_t n = recv(client_fd, header.data() + received, 
                           protocol::MIN_MESSAGE_SIZE - received, 0);
            if (n <= 0) {
                if (n == 0) {
                    std::cout << "Client disconnected" << std::endl;
                } else {
                    std::cerr << "Receive error: " << strerror(errno) << std::endl;
                }
                close(client_fd);
                return;
            }
            received += n;
        }
        
        if (!running_) {
            break;
        }
        
        // Validate header
        if (header[0] != protocol::MAGIC_BYTE_1 || header[1] != protocol::MAGIC_BYTE_2) {
            std::cerr << "Invalid magic bytes" << std::endl;
            close(client_fd);
            return;
        }
        
        uint16_t length = (static_cast<uint16_t>(header[2]) << 8) | header[3];
        
        if (length < protocol::MIN_MESSAGE_SIZE || length > protocol::MAX_MESSAGE_SIZE) {
            std::cerr << "Invalid message length: " << length << std::endl;
            close(client_fd);
            return;
        }
        
        // Read remaining data
        std::vector<uint8_t> full_message = header;
        if (length > protocol::MIN_MESSAGE_SIZE) {
            size_t remaining = length - protocol::MIN_MESSAGE_SIZE;
            full_message.resize(length);
            received = 0;
            
            while (received < remaining) {
                ssize_t n = recv(client_fd, full_message.data() + protocol::MIN_MESSAGE_SIZE + received,
                               remaining - received, 0);
                if (n <= 0) {
                    if (n == 0) {
                        std::cout << "Client disconnected" << std::endl;
                    } else {
                        std::cerr << "Receive error: " << strerror(errno) << std::endl;
                    }
                    close(client_fd);
                    return;
                }
                received += n;
            }
        }
        
        // Decode message
        protocol::Message request;
        if (!protocol::decodeMessage(full_message, request)) {
            std::cerr << "Failed to decode message" << std::endl;
            close(client_fd);
            return;
        }
        
        // Handle command
        protocol::Message response;
        command_handler_.handleCommand(request, response);
        
        // Send response
        std::vector<uint8_t> response_data = protocol::encodeMessage(response);
        size_t sent = 0;
        
        while (sent < response_data.size()) {
            ssize_t n = send(client_fd, response_data.data() + sent, 
                           response_data.size() - sent, 0);
            if (n < 0) {
                if (errno != EINTR) {
                    std::cerr << "Send error: " << strerror(errno) << std::endl;
                    close(client_fd);
                    return;
                }
                continue;
            }
            sent += n;
        }
    }
    
    close(client_fd);
}

void NetworkServer::setError(const std::string& error) {
    last_error_ = error;
    std::cerr << "Error: " << error << std::endl;
}

} // namespace server
} // namespace rebear
