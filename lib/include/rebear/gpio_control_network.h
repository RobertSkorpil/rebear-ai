#ifndef REBEAR_GPIO_CONTROL_NETWORK_H
#define REBEAR_GPIO_CONTROL_NETWORK_H

#include "gpio_control.h"
#include "network_client.h"
#include <memory>
#include <string>

namespace rebear {

/**
 * @brief Network-based GPIO control implementation
 * 
 * Provides the same interface as GPIOControl but communicates with
 * a remote rebear-server over TCP/IP instead of accessing hardware directly.
 */
class GPIOControlNetwork {
public:
    /**
     * @brief Construct GPIO control object
     * @param pin GPIO pin number (e.g., 3 or 4)
     * @param dir Direction (Input or Output)
     * @param host Hostname or IP address of rebear-server
     * @param port Port number (default 9876)
     */
    GPIOControlNetwork(int pin, GPIOControl::Direction dir, 
                      const std::string& host, uint16_t port = 9876);
    
    /**
     * @brief Destructor - automatically closes GPIO
     */
    ~GPIOControlNetwork();
    
    // Disable copy
    GPIOControlNetwork(const GPIOControlNetwork&) = delete;
    GPIOControlNetwork& operator=(const GPIOControlNetwork&) = delete;
    
    /**
     * @brief Initialize GPIO pin
     * @return true on success, false on error
     */
    bool init();
    
    /**
     * @brief Close GPIO pin
     */
    void close();
    
    /**
     * @brief Write value to output pin
     * @param value true for HIGH (1), false for LOW (0)
     * @return true on success, false on error
     */
    bool write(bool value);
    
    /**
     * @brief Read current output state (for output pins)
     * @return Current output value
     */
    bool read() const;
    
    /**
     * @brief Read input pin value (for input pins)
     * @return Current input value
     */
    bool readInput() const;
    
    /**
     * @brief Wait for edge event (blocking)
     * @param timeout_ms Timeout in milliseconds (0 = infinite)
     * @return true if edge detected, false on timeout or error
     */
    bool waitForEdge(int timeout_ms);
    
    /**
     * @brief Check if GPIO is open
     * @return true if open, false otherwise
     */
    bool isOpen() const { return is_open_; }
    
    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const { return lastError_; }

private:
    std::unique_ptr<NetworkClient> client_;
    int pin_;
    GPIOControl::Direction direction_;
    bool is_open_;
    mutable bool current_value_;  // For output pins
    std::string lastError_;
    
    void setError(const std::string& error);
};

} // namespace rebear

#endif // REBEAR_GPIO_CONTROL_NETWORK_H
