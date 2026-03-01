#pragma once

#include <string>
#include <memory>
#include <functional>

namespace rebear {

/**
 * @brief Low-level GPIO control class
 * 
 * Provides access to Raspberry Pi GPIO pins for input/output operations.
 * Uses Linux GPIO character device interface (/dev/gpiochip0) with fallback
 * to sysfs interface (/sys/class/gpio) if character device is unavailable.
 * 
 * GPIO Pin Assignments:
 * - GPIO 3: Button control (output) - Controls teddy bear button
 * - GPIO 4: Buffer ready signal (input) - FPGA buffer status
 */
class GPIOControl {
public:
    enum class Direction {
        Input,
        Output
    };
    
    enum class Edge {
        None,
        Rising,
        Falling,
        Both
    };
    
    /**
     * @brief Construct GPIO control object
     * @param pin GPIO pin number (e.g., 3 or 4)
     * @param dir Direction (Input or Output)
     */
    GPIOControl(int pin, Direction dir);
    
    /**
     * @brief Destructor - automatically closes GPIO
     */
    ~GPIOControl();
    
    // Disable copy
    GPIOControl(const GPIOControl&) = delete;
    GPIOControl& operator=(const GPIOControl&) = delete;
    
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
     * @brief Set edge detection for input pins
     * @param edge Edge type to detect
     * @return true on success, false on error
     */
    bool setEdge(Edge edge);
    
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
    int pin_;
    Direction direction_;
    int fd_;
    bool is_open_;
    bool current_value_;  // For output pins
    std::string lastError_;
    bool using_sysfs_;    // True if using sysfs fallback
    
    // Helper methods
    bool initCharDevice();
    bool initSysfs();
    void closeSysfs();
};

/**
 * @brief High-level button control helper
 * 
 * Provides convenient interface for controlling the teddy bear button
 * via GPIO 3. Handles timing and state management.
 */
class ButtonControl {
public:
    /**
     * @brief Construct button control
     * @param gpio_pin GPIO pin number (default: 3)
     */
    explicit ButtonControl(int gpio_pin = 3);
    
    /**
     * @brief Destructor - ensures button is released
     */
    ~ButtonControl();
    
    // Disable copy
    ButtonControl(const ButtonControl&) = delete;
    ButtonControl& operator=(const ButtonControl&) = delete;
    
    /**
     * @brief Initialize button control
     * @return true on success, false on error
     */
    bool init();
    
    /**
     * @brief Press button (set GPIO high)
     * @return true on success, false on error
     */
    bool press();
    
    /**
     * @brief Release button (set GPIO low)
     * @return true on success, false on error
     */
    bool release();
    
    /**
     * @brief Complete button click (press, wait, release)
     * @param duration_ms Press duration in milliseconds (default: 100ms)
     * @return true on success, false on error
     */
    bool click(int duration_ms = 100);
    
    /**
     * @brief Get current button state
     * @return true if pressed, false if released
     */
    bool isPressed() const { return pressed_; }
    
    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;
    
private:
    std::unique_ptr<GPIOControl> gpio_;
    bool pressed_;
};

/**
 * @brief Buffer ready monitor helper
 * 
 * Monitors GPIO 4 for FPGA buffer ready signal. Provides efficient
 * polling and interrupt-driven monitoring to avoid unnecessary SPI reads.
 */
class BufferReadyMonitor {
public:
    /**
     * @brief Construct buffer ready monitor
     * @param gpio_pin GPIO pin number (default: 4)
     */
    explicit BufferReadyMonitor(int gpio_pin = 4);
    
    /**
     * @brief Destructor
     */
    ~BufferReadyMonitor();
    
    // Disable copy
    BufferReadyMonitor(const BufferReadyMonitor&) = delete;
    BufferReadyMonitor& operator=(const BufferReadyMonitor&) = delete;
    
    /**
     * @brief Initialize buffer ready monitoring
     * @return true on success, false on error
     */
    bool init();
    
    /**
     * @brief Check if buffer has data available
     * @return true if buffer ready (GPIO high), false otherwise
     */
    bool isReady() const;
    
    /**
     * @brief Wait for buffer ready signal (blocking)
     * @param timeout_ms Timeout in milliseconds (0 = infinite)
     * @return true if buffer ready, false on timeout or error
     */
    bool waitReady(int timeout_ms = 1000);
    
    /**
     * @brief Set callback for buffer ready events
     * @param callback Function to call when buffer becomes ready
     * @return true on success, false on error
     * @note Callback runs in separate thread
     */
    bool setCallback(std::function<void()> callback);
    
    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;
    
private:
    std::unique_ptr<GPIOControl> gpio_;
    std::function<void()> callback_;
};

} // namespace rebear
