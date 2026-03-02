#ifndef REBEAR_NETWORK_CLIENT_H
#define REBEAR_NETWORK_CLIENT_H

#include "protocol.h"
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <map>

namespace rebear {

/**
 * @brief Network client for communicating with rebear-server
 * 
 * Provides TCP-based communication with automatic reconnection,
 * request/response correlation, and thread-safe operation.
 */
class NetworkClient {
public:
    /**
     * @brief Construct a network client
     * @param host Hostname or IP address of server
     * @param port Port number (default 9876)
     */
    NetworkClient(const std::string& host, uint16_t port = 9876);
    
    /**
     * @brief Destructor - closes connection and stops threads
     */
    ~NetworkClient();
    
    // Disable copy
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    
    /**
     * @brief Connect to the server
     * @param timeout_ms Connection timeout in milliseconds
     * @return true if connected successfully
     */
    bool connect(int timeout_ms = 5000);
    
    /**
     * @brief Disconnect from the server
     */
    void disconnect();
    
    /**
     * @brief Check if connected to server
     * @return true if connected
     */
    bool isConnected() const;
    
    /**
     * @brief Send a request and wait for response
     * @param type Command type
     * @param payload Request payload
     * @param response Response payload (output)
     * @param timeout_ms Response timeout in milliseconds
     * @return true if request succeeded
     */
    bool sendRequest(uint8_t type, 
                     const std::vector<uint8_t>& payload,
                     std::vector<uint8_t>& response, 
                     int timeout_ms = 1000);
    
    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;

private:
    // Connection state
    int socket_fd_;
    std::string host_;
    uint16_t port_;
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    
    // Threading
    std::thread recv_thread_;
    std::mutex mutex_;
    std::condition_variable response_cv_;
    
    // Response tracking
    struct PendingResponse {
        bool received;
        std::vector<uint8_t> data;
    };
    std::map<uint8_t, PendingResponse> pending_responses_;
    
    // Error tracking
    mutable std::mutex error_mutex_;
    std::string last_error_;
    
    // Internal methods
    void receiveLoop();
    bool sendMessage(const protocol::Message& msg);
    bool receiveMessage(protocol::Message& msg, int timeout_ms);
    void setError(const std::string& error);
    bool waitForResponse(uint8_t response_type, std::vector<uint8_t>& response, int timeout_ms);
};

} // namespace rebear

#endif // REBEAR_NETWORK_CLIENT_H
