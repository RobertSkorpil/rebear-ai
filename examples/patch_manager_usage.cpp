/**
 * @file patch_manager_usage.cpp
 * @brief Example demonstrating PatchManager usage
 *
 * This example shows how to:
 * - Create and manage patches
 * - Save/load patch configurations
 * - Apply patches to FPGA
 * - Monitor for patch triggers
 */

#include "rebear/patch_manager.h"
#include "rebear/spi_protocol.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

using namespace rebear;

// Global flag for graceful shutdown
std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "\nShutting down..." << std::endl;
    running.store(false, std::memory_order_release);
}

void printPatch(const Patch& patch) {
    std::cout << "  Patch " << static_cast<int>(patch.id) 
              << ": Address 0x" << std::hex << patch.address << std::dec
              << ", Data: ";
    for (size_t i = 0; i < patch.data.size(); ++i) {
        printf("%02X ", patch.data[i]);
    }
    std::cout << (patch.enabled ? "[ENABLED]" : "[DISABLED]") << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "PatchManager Usage Example" << std::endl;
    std::cout << "==========================" << std::endl;
    
    // Set up signal handler
    signal(SIGINT, signalHandler);
    
    // Create patch manager
    PatchManager patchMgr;
    
    // Example 1: Create patches programmatically
    std::cout << "\n1. Creating patches..." << std::endl;
    
    Patch patch0;
    patch0.id = 0;
    patch0.address = 0x001000;
    patch0.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    patch0.enabled = true;
    
    if (patchMgr.addPatch(patch0)) {
        std::cout << "✓ Added patch 0" << std::endl;
    } else {
        std::cerr << "✗ Failed to add patch 0: " << patchMgr.getLastError() << std::endl;
    }
    
    Patch patch1;
    patch1.id = 1;
    patch1.address = 0x002000;
    patch1.data = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    patch1.enabled = true;
    
    if (patchMgr.addPatch(patch1)) {
        std::cout << "✓ Added patch 1" << std::endl;
    }
    
    // Example 2: List all patches
    std::cout << "\n2. Current patches:" << std::endl;
    auto patches = patchMgr.getPatches();
    for (const auto& p : patches) {
        printPatch(p);
    }
    
    // Example 3: Save to file
    std::cout << "\n3. Saving patches to file..." << std::endl;
    const std::string filename = "my_patches.json";
    
    if (patchMgr.saveToFile(filename)) {
        std::cout << "✓ Saved to " << filename << std::endl;
    } else {
        std::cerr << "✗ Failed to save: " << patchMgr.getLastError() << std::endl;
    }
    
    // Example 4: Load from file
    std::cout << "\n4. Loading patches from file..." << std::endl;
    PatchManager loadedMgr;
    
    if (loadedMgr.loadFromFile(filename)) {
        std::cout << "✓ Loaded " << loadedMgr.count() << " patches" << std::endl;
        for (const auto& p : loadedMgr.getPatches()) {
            printPatch(p);
        }
    } else {
        std::cerr << "✗ Failed to load: " << loadedMgr.getLastError() << std::endl;
    }
    
    // Example 5: Connect to FPGA and apply patches
    std::cout << "\n5. Connecting to FPGA..." << std::endl;
    SPIProtocol spi;
    
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "✗ Failed to connect: " << spi.getLastError() << std::endl;
        std::cerr << "  (This is expected if FPGA is not connected)" << std::endl;
        std::cout << "\nExample completed (without hardware)." << std::endl;
        return 0;
    }
    
    std::cout << "✓ Connected to FPGA" << std::endl;
    
    // Clear old transactions
    std::cout << "\n6. Clearing old transactions..." << std::endl;
    spi.clearTransactions();
    
    // Apply all patches
    std::cout << "\n7. Applying patches to FPGA..." << std::endl;
    if (patchMgr.applyAll(spi)) {
        std::cout << "✓ All patches applied successfully" << std::endl;
    } else {
        std::cerr << "✗ Failed to apply patches: " << patchMgr.getLastError() << std::endl;
    }
    
    // Example 6: Monitor for patch triggers
    std::cout << "\n8. Monitoring for patch triggers..." << std::endl;
    std::cout << "   (Press Ctrl+C to stop)" << std::endl;
    
    int transactionCount = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    while (running) {
        auto trans = spi.readTransaction();
        
        if (trans) {
            transactionCount++;
            
            std::cout << "Transaction #" << transactionCount 
                      << ": Address 0x" << std::hex << trans->address << std::dec
                      << ", Count: " << trans->count << " bytes"
                      << ", Time: " << trans->timestamp << " ms" << std::endl;
            
            // Check if this transaction matches any patch
            for (const auto& p : patchMgr.getPatches()) {
                if (p.address == trans->address) {
                    std::cout << "  ⚡ PATCH " << static_cast<int>(p.id) 
                              << " TRIGGERED!" << std::endl;
                }
            }
        }
        
        // Poll at 10 Hz
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Stop after 30 seconds if no Ctrl+C
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(30)) {
            std::cout << "\n30 seconds elapsed, stopping..." << std::endl;
            break;
        }
    }
    
    std::cout << "\nCaptured " << transactionCount << " transactions" << std::endl;
    
    // Example 7: Clean up
    std::cout << "\n9. Cleaning up..." << std::endl;
    
    if (patchMgr.clearAll(spi)) {
        std::cout << "✓ Cleared all patches from FPGA" << std::endl;
    } else {
        std::cerr << "✗ Failed to clear patches: " << patchMgr.getLastError() << std::endl;
    }
    
    spi.close();
    std::cout << "✓ Disconnected from FPGA" << std::endl;
    
    std::cout << "\nExample completed successfully!" << std::endl;
    
    return 0;
}
