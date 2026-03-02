#ifndef REBEAR_NETWORK_SERVER_H
#define REBEAR_NETWORK_SERVER_H

#include "command_handler.h"
#include <atomic>
#include <thread>
#include <vector>
#include <memory>

namespace rebear {
namespace server {

/**
 * @brief TCP server for rebear hardware access
 * 
 * Listens for client connections and handles commands.
 * Supports multiple concurrent clients.
 */
class NetworkServer {
public:
    /**
     * @brief Construct network server
     * @param port Port to listen on (default 9876)
     * @param max_clients Maximum concurrent clients (default 10)
     */
    NetworkServer(uint16_t port = 9876, int max_clients = 10);
    
    /**
     * @brief Destructor - stops server and closes connections
     */
    ~NetworkServer();
    
    /**
     * @brief Start the server
     * @return true if started successfully
     */
    bool start();
    
    /**
     * @brief Stop the server
     */
    void stop();
    
    /**
     * @brief Check if server is running
     * @return true if running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const { return last_error_; }

private:
    uint16_t port_;
    int max_clients_;
    int server_fd_;
    std::atomic<bool> running_;
    std::thread accept_thread_;
    std::vector<std::thread> client_threads_;
    std::string last_error_;
    
    CommandHandler command_handler_;
    
    void acceptLoop();
    void handleClient(int client_fd);
    void setError(const std::string& error);
};

} // namespace server
} // namespace rebear

#endif // REBEAR_NETWORK_SERVER_H
