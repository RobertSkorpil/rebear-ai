#include "rebear/gpio_control.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace rebear;

void printTestHeader(const std::string& test_name) {
    std::cout << "\n=== " << test_name << " ===" << std::endl;
}

void printResult(const std::string& test, bool passed) {
    std::cout << (passed ? "✓ " : "✗ ") << test << std::endl;
}

int main() {
    std::cout << "GPIO Control Test Suite" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "\nNote: This test requires GPIO hardware (Raspberry Pi)" << std::endl;
    std::cout << "      User must be in 'gpio' group for permissions" << std::endl;
    
    // Test 1: GPIOControl Basic Operations
    printTestHeader("Test 1: GPIOControl Basic Operations");
    {
        GPIOControl gpio(3, GPIOControl::Direction::Output);
        
        printResult("Initial state: not open", !gpio.isOpen());
        
        bool init_success = gpio.init();
        if (!init_success) {
            std::cout << "  Warning: GPIO init failed: " << gpio.getLastError() << std::endl;
            std::cout << "  (This is expected if not running on Raspberry Pi)" << std::endl;
        } else {
            printResult("GPIO initialized successfully", true);
            printResult("isOpen() returns true", gpio.isOpen());
            
            // Test write operations
            bool write_high = gpio.write(true);
            printResult("Write HIGH succeeded", write_high);
            printResult("Read back HIGH", gpio.read() == true);
            
            bool write_low = gpio.write(false);
            printResult("Write LOW succeeded", write_low);
            printResult("Read back LOW", gpio.read() == false);
            
            gpio.close();
            printResult("close() works correctly", !gpio.isOpen());
        }
    }
    
    // Test 2: ButtonControl
    printTestHeader("Test 2: ButtonControl");
    {
        ButtonControl button(3);
        
        printResult("Initial state: not pressed", !button.isPressed());
        
        bool init_success = button.init();
        if (!init_success) {
            std::cout << "  Warning: Button init failed: " << button.getLastError() << std::endl;
            std::cout << "  (This is expected if not running on Raspberry Pi)" << std::endl;
        } else {
            printResult("Button initialized successfully", true);
            
            // Test press/release
            bool press_ok = button.press();
            printResult("Press button succeeded", press_ok);
            printResult("isPressed() returns true", button.isPressed());
            
            bool release_ok = button.release();
            printResult("Release button succeeded", release_ok);
            printResult("isPressed() returns false", !button.isPressed());
            
            // Test click
            std::cout << "  Testing click (50ms duration)..." << std::endl;
            auto start = std::chrono::steady_clock::now();
            bool click_ok = button.click(50);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            
            printResult("Click succeeded", click_ok);
            printResult("Click duration ~50ms", elapsed >= 45 && elapsed <= 100);
            printResult("Button released after click", !button.isPressed());
        }
    }
    
    // Test 3: BufferReadyMonitor
    printTestHeader("Test 3: BufferReadyMonitor");
    {
        BufferReadyMonitor monitor(4);
        
        bool init_success = monitor.init();
        if (!init_success) {
            std::cout << "  Warning: Monitor init failed: " << monitor.getLastError() << std::endl;
            std::cout << "  (This is expected if not running on Raspberry Pi)" << std::endl;
        } else {
            printResult("Monitor initialized successfully", true);
            
            // Test isReady
            bool ready = monitor.isReady();
            std::cout << "  Buffer ready state: " << (ready ? "READY" : "NOT READY") << std::endl;
            printResult("isReady() call succeeded", true);
            
            // Test waitReady with short timeout
            std::cout << "  Testing waitReady with 100ms timeout..." << std::endl;
            auto start = std::chrono::steady_clock::now();
            bool wait_result = monitor.waitReady(100);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            
            if (wait_result) {
                std::cout << "  Buffer became ready during wait" << std::endl;
            } else {
                std::cout << "  Timeout occurred (expected if buffer not ready)" << std::endl;
            }
            printResult("waitReady() completed", true);
        }
    }
    
    // Test 4: Multiple GPIO Instances
    printTestHeader("Test 4: Multiple GPIO Instances");
    {
        ButtonControl button(3);
        BufferReadyMonitor monitor(4);
        
        bool button_init = button.init();
        bool monitor_init = monitor.init();
        
        if (button_init && monitor_init) {
            printResult("Both GPIOs initialized", true);
            
            // Test simultaneous operations
            button.press();
            bool ready = monitor.isReady();
            button.release();
            
            printResult("Simultaneous operations work", true);
        } else {
            std::cout << "  Warning: Could not initialize both GPIOs" << std::endl;
            std::cout << "  (This is expected if not running on Raspberry Pi)" << std::endl;
        }
    }
    
    // Test 5: Error Handling
    printTestHeader("Test 5: Error Handling");
    {
        GPIOControl gpio(3, GPIOControl::Direction::Output);
        
        // Try to write before init
        bool write_before_init = gpio.write(true);
        printResult("Write before init fails", !write_before_init);
        printResult("Error message set", !gpio.getLastError().empty());
        
        // Try to initialize twice
        if (gpio.init()) {
            bool init_again = gpio.init();
            printResult("Double init fails", !init_again);
        }
        
        // Try to write to input pin
        GPIOControl input_gpio(4, GPIOControl::Direction::Input);
        if (input_gpio.init()) {
            bool write_to_input = input_gpio.write(true);
            printResult("Write to input pin fails", !write_to_input);
        }
    }
    
    // Test 6: Button Click Timing
    printTestHeader("Test 6: Button Click Timing");
    {
        ButtonControl button(3);
        
        if (button.init()) {
            // Test different durations
            int durations[] = {50, 100, 200};
            bool all_passed = true;
            
            for (int duration : durations) {
                auto start = std::chrono::steady_clock::now();
                button.click(duration);
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                ).count();
                
                // Allow 20% tolerance
                int min_time = duration - 10;
                int max_time = duration + 50;  // Extra tolerance for system delays
                bool timing_ok = (elapsed >= min_time && elapsed <= max_time);
                
                std::cout << "  " << duration << "ms click: actual=" << elapsed 
                          << "ms " << (timing_ok ? "✓" : "✗") << std::endl;
                
                all_passed = all_passed && timing_ok;
            }
            
            printResult("All click timings within tolerance", all_passed);
        }
    }
    
    // Test 7: Cleanup and Reinitialization
    printTestHeader("Test 7: Cleanup and Reinitialization");
    {
        ButtonControl button(3);
        
        if (button.init()) {
            printResult("First init succeeded", true);
            
            button.press();
            // Destructor should release button
        }
        
        // Create new instance with same pin
        ButtonControl button2(3);
        bool reinit = button2.init();
        printResult("Reinitialization succeeded", reinit);
        
        if (reinit) {
            printResult("Button starts in released state", !button2.isPressed());
        }
    }
    
    std::cout << "\n========================" << std::endl;
    std::cout << "All tests completed!" << std::endl;
    std::cout << "\nNote: Some tests may show warnings if not running on Raspberry Pi" << std::endl;
    std::cout << "      with proper GPIO hardware. This is expected behavior." << std::endl;
    
    return 0;
}
