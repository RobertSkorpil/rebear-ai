/**
 * @file spi_basic_usage.cpp
 * @brief Basic usage example for SPIProtocol
 * 
 * This example demonstrates how to use the SPIProtocol class to:
 * - Connect to the FPGA
 * - Monitor Flash transactions
 * - Apply patches
 * - Clear patches
 */

#include "rebear/spi_protocol.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace rebear;

void printTransaction(const Transaction& trans) {
    std::cout << "  Address: 0x" << std::hex << std::setw(6) << std::setfill('0') 
              << trans.address << std::dec
              << " | Count: " << std::setw(5) << trans.count << " bytes"
              << " | Time: " << std::setw(5) << trans.timestamp << " ms"
              << std::endl;
}

int main() {
    std::cout << "SPIProtocol Basic Usage Example" << std::endl;
    std::cout << "================================" << std::endl;
    
    // Create SPIProtocol instance
    SPIProtocol spi;
    
    // Open connection to FPGA
    std::cout << "\n1. Connecting to FPGA..." << std::endl;
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Error: " << spi.getLastError() << std::endl;
        return 1;
    }
    std::cout << "   ✓ Connected successfully" << std::endl;
    
    // Clear any old transactions
    std::cout << "\n2. Clearing transaction buffer..." << std::endl;
    if (!spi.clearTransactions()) {
        std::cerr << "Error: " << spi.getLastError() << std::endl;
        return 1;
    }
    std::cout << "   ✓ Buffer cleared" << std::endl;
    
    // Monitor transactions for a few seconds
    std::cout << "\n3. Monitoring transactions (5 seconds)..." << std::endl;
    std::cout << "   (Trigger teddy bear activity to see transactions)" << std::endl;
    
    auto startTime = std::chrono::steady_clock::now();
    int transactionCount = 0;
    
    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(5)) {
            break;
        }
        
        // Try to read a transaction
        auto trans = spi.readTransaction();
        if (trans) {
            transactionCount++;
            printTransaction(*trans);
        }
        
        // Small delay to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "   Total transactions captured: " << transactionCount << std::endl;
    
    // Apply a test patch
    std::cout << "\n4. Applying test patch..." << std::endl;
    Patch testPatch;
    testPatch.id = 0;
    testPatch.address = 0x001000;  // Example address
    testPatch.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    testPatch.enabled = true;
    
    if (!spi.setPatch(testPatch)) {
        std::cerr << "Error: " << spi.getLastError() << std::endl;
        return 1;
    }
    std::cout << "   ✓ Patch applied: " << testPatch.toString() << std::endl;
    std::cout << "   (This patch will replace 8 bytes at address 0x001000 with 0xFF)" << std::endl;
    
    // Monitor for a bit more to see if patch triggers
    std::cout << "\n5. Monitoring with patch active (5 seconds)..." << std::endl;
    std::cout << "   (Watch for reads from address 0x001000)" << std::endl;
    
    startTime = std::chrono::steady_clock::now();
    transactionCount = 0;
    
    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(5)) {
            break;
        }
        
        auto trans = spi.readTransaction();
        if (trans) {
            transactionCount++;
            printTransaction(*trans);
            
            // Highlight if patch address is accessed
            if (trans->address == 0x001000) {
                std::cout << "   ⚠ PATCH TRIGGERED! MCU read from patched address" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "   Total transactions captured: " << transactionCount << std::endl;
    
    // Clear patches
    std::cout << "\n6. Clearing all patches..." << std::endl;
    if (!spi.clearPatches()) {
        std::cerr << "Error: " << spi.getLastError() << std::endl;
        return 1;
    }
    std::cout << "   ✓ All patches cleared" << std::endl;
    
    // Close connection
    std::cout << "\n7. Disconnecting..." << std::endl;
    spi.close();
    std::cout << "   ✓ Disconnected" << std::endl;
    
    std::cout << "\n=== Example completed successfully ===" << std::endl;
    
    return 0;
}
