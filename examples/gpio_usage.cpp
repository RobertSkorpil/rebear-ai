/**
 * @file gpio_usage.cpp
 * @brief Example demonstrating GPIO control for button and buffer monitoring
 * 
 * This example shows how to:
 * 1. Control the teddy bear button via GPIO 3
 * 2. Monitor FPGA buffer ready signal via GPIO 4
 * 3. Coordinate button presses with transaction monitoring
 */

#include "rebear/gpio_control.h"
#include "rebear/spi_protocol.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace rebear;

void printTransaction(const Transaction& trans) {
    std::cout << "  Address: 0x" << std::hex << trans.address 
              << " (" << std::dec << trans.address << ")" << std::endl;
    std::cout << "  Count:   " << trans.count << " bytes" << std::endl;
    std::cout << "  Time:    " << trans.timestamp << " ms" << std::endl;
}

int main() {
    std::cout << "GPIO Control Example" << std::endl;
    std::cout << "====================" << std::endl;
    std::cout << std::endl;
    
    // Initialize button control (GPIO 3)
    std::cout << "Initializing button control (GPIO 3)..." << std::endl;
    ButtonControl button(3);
    if (!button.init()) {
        std::cerr << "Failed to initialize button: " << button.getLastError() << std::endl;
        std::cerr << "Make sure you're running on Raspberry Pi with GPIO access" << std::endl;
        return 1;
    }
    std::cout << "✓ Button control initialized" << std::endl;
    
    // Initialize buffer ready monitor (GPIO 4)
    std::cout << "Initializing buffer ready monitor (GPIO 4)..." << std::endl;
    BufferReadyMonitor monitor(4);
    if (!monitor.init()) {
        std::cerr << "Failed to initialize monitor: " << monitor.getLastError() << std::endl;
        return 1;
    }
    std::cout << "✓ Buffer ready monitor initialized" << std::endl;
    
    // Connect to FPGA via SPI
    std::cout << "\nConnecting to FPGA..." << std::endl;
    SPIProtocol spi;
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return 1;
    }
    std::cout << "✓ Connected to FPGA" << std::endl;
    
    // Clear old transactions
    std::cout << "\nClearing old transactions..." << std::endl;
    if (!spi.clearTransactions()) {
        std::cerr << "Failed to clear transactions: " << spi.getLastError() << std::endl;
        return 1;
    }
    std::cout << "✓ Transaction buffer cleared" << std::endl;
    
    // Example 1: Simple button press
    std::cout << "\n--- Example 1: Simple Button Press ---" << std::endl;
    std::cout << "Pressing button for 100ms..." << std::endl;
    if (button.click(100)) {
        std::cout << "✓ Button pressed and released" << std::endl;
    } else {
        std::cerr << "✗ Button click failed: " << button.getLastError() << std::endl;
    }
    
    // Wait for MCU to respond
    std::cout << "Waiting for MCU activity..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Example 2: Efficient monitoring with buffer ready signal
    std::cout << "\n--- Example 2: Efficient Monitoring ---" << std::endl;
    std::cout << "Monitoring transactions using buffer ready signal..." << std::endl;
    std::cout << "(Will monitor for 10 seconds)" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    int transaction_count = 0;
    
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();
        
        if (elapsed >= 10) {
            break;
        }
        
        // Check if buffer has data (non-blocking)
        if (monitor.isReady()) {
            std::cout << "\nBuffer ready signal detected!" << std::endl;
            
            // Read transaction
            auto trans = spi.readTransaction();
            if (trans && trans->address != 0xFFFFFF) {
                transaction_count++;
                std::cout << "Transaction #" << transaction_count << ":" << std::endl;
                printTransaction(*trans);
            }
        } else {
            // No data available, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    std::cout << "\nTotal transactions captured: " << transaction_count << std::endl;
    
    // Example 3: Blocking wait for buffer ready
    std::cout << "\n--- Example 3: Blocking Wait ---" << std::endl;
    std::cout << "Pressing button and waiting for buffer ready..." << std::endl;
    
    button.click(100);
    
    std::cout << "Waiting for buffer ready (timeout: 2 seconds)..." << std::endl;
    if (monitor.waitReady(2000)) {
        std::cout << "✓ Buffer became ready!" << std::endl;
        
        auto trans = spi.readTransaction();
        if (trans && trans->address != 0xFFFFFF) {
            std::cout << "Transaction captured:" << std::endl;
            printTransaction(*trans);
        }
    } else {
        std::cout << "✗ Timeout waiting for buffer ready" << std::endl;
    }
    
    // Example 4: Automated testing sequence
    std::cout << "\n--- Example 4: Automated Testing ---" << std::endl;
    std::cout << "Performing automated button press sequence..." << std::endl;
    
    for (int i = 1; i <= 3; i++) {
        std::cout << "\nPress #" << i << ":" << std::endl;
        
        // Clear buffer
        spi.clearTransactions();
        
        // Press button
        std::cout << "  Pressing button..." << std::endl;
        button.click(100);
        
        // Wait for activity
        std::cout << "  Waiting for transactions..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Read all available transactions
        int count = 0;
        while (monitor.isReady()) {
            auto trans = spi.readTransaction();
            if (trans && trans->address != 0xFFFFFF) {
                count++;
            } else {
                break;
            }
        }
        
        std::cout << "  Captured " << count << " transactions" << std::endl;
        
        // Wait between presses
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Cleanup
    std::cout << "\n--- Cleanup ---" << std::endl;
    std::cout << "Ensuring button is released..." << std::endl;
    button.release();
    
    std::cout << "Closing SPI connection..." << std::endl;
    spi.close();
    
    std::cout << "\n✓ Example completed successfully!" << std::endl;
    std::cout << "\nKey Takeaways:" << std::endl;
    std::cout << "1. Use ButtonControl for programmatic button presses" << std::endl;
    std::cout << "2. Use BufferReadyMonitor to avoid unnecessary SPI reads" << std::endl;
    std::cout << "3. Combine both for efficient automated testing" << std::endl;
    std::cout << "4. Buffer ready signal eliminates dummy reads (0xFF...)" << std::endl;
    
    return 0;
}
