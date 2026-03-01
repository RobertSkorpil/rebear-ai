/**
 * @file test_patch_manager.cpp
 * @brief Test suite for PatchManager class
 * 
 * Tests patch management, JSON I/O, and FPGA integration.
 */

#include "rebear/patch_manager.h"
#include "rebear/patch.h"
#include "rebear/spi_protocol.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <fstream>

using namespace rebear;

// Test colors
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

void printTest(const std::string& name, bool passed) {
    std::cout << (passed ? GREEN "✓" : RED "✗") << RESET 
              << " " << name << std::endl;
}

// Test 1: Basic operations
void testBasicOperations() {
    std::cout << "\n=== Test 1: Basic Operations ===" << std::endl;
    
    PatchManager pm;
    
    // Initially empty
    printTest("Initially empty", pm.empty() && pm.count() == 0);
    
    // Add a patch
    Patch p1;
    p1.id = 0;
    p1.address = 0x001000;
    p1.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    p1.enabled = true;
    
    bool added = pm.addPatch(p1);
    printTest("Add patch succeeded", added);
    printTest("Count is 1", pm.count() == 1);
    printTest("Not empty", !pm.empty());
    
    // Get patch
    const Patch* retrieved = pm.getPatch(0);
    printTest("Get patch by ID", retrieved != nullptr);
    if (retrieved) {
        printTest("Retrieved patch matches", 
                  retrieved->id == 0 && 
                  retrieved->address == 0x001000 &&
                  retrieved->enabled == true);
    }
    
    // Add another patch
    Patch p2;
    p2.id = 1;
    p2.address = 0x002000;
    p2.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    p2.enabled = true;
    
    pm.addPatch(p2);
    printTest("Count is 2", pm.count() == 2);
    
    // Get all patches
    auto patches = pm.getPatches();
    printTest("Get all patches returns 2", patches.size() == 2);
    
    // Remove patch
    bool removed = pm.removePatch(0);
    printTest("Remove patch succeeded", removed);
    printTest("Count is 1 after removal", pm.count() == 1);
    
    // Try to get removed patch
    const Patch* notFound = pm.getPatch(0);
    printTest("Removed patch not found", notFound == nullptr);
    
    // Clear local
    pm.clearLocal();
    printTest("Clear local empties manager", pm.empty());
}

// Test 2: Validation
void testValidation() {
    std::cout << "\n=== Test 2: Validation ===" << std::endl;
    
    PatchManager pm;
    
    // Valid patch
    Patch valid;
    valid.id = 5;
    valid.address = 0x001000;
    valid.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    valid.enabled = true;
    
    printTest("Valid patch accepted", pm.addPatch(valid));
    
    // Invalid patch (ID > 15)
    Patch invalid;
    invalid.id = 16;
    invalid.address = 0x001000;
    invalid.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    invalid.enabled = true;
    
    printTest("Invalid patch rejected", !pm.addPatch(invalid));
    printTest("Count still 1", pm.count() == 1);
    
    // Replace existing patch
    Patch replacement;
    replacement.id = 5;
    replacement.address = 0x003000;
    replacement.data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    replacement.enabled = false;
    
    pm.addPatch(replacement);
    printTest("Replacement patch accepted", pm.count() == 1);
    
    const Patch* replaced = pm.getPatch(5);
    printTest("Patch was replaced", 
              replaced && replaced->address == 0x003000 && !replaced->enabled);
}

// Test 3: JSON save/load
void testJsonIO() {
    std::cout << "\n=== Test 3: JSON Save/Load ===" << std::endl;
    
    const std::string testFile = "/tmp/test_patches.json";
    
    // Create patches
    PatchManager pm1;
    
    Patch p1;
    p1.id = 0;
    p1.address = 0x001000;
    p1.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    p1.enabled = true;
    pm1.addPatch(p1);
    
    Patch p2;
    p2.id = 5;
    p2.address = 0x002000;
    p2.data = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88};
    p2.enabled = false;
    pm1.addPatch(p2);
    
    Patch p3;
    p3.id = 15;
    p3.address = 0x003000;
    p3.data = {0x4A, 0x4D, 0x00, 0xFF, 0x12, 0x34, 0x56, 0x78};
    p3.enabled = true;
    pm1.addPatch(p3);
    
    // Save to file
    bool saved = pm1.saveToFile(testFile);
    printTest("Save to file succeeded", saved);
    
    // Verify file exists
    std::ifstream check(testFile);
    printTest("File was created", check.good());
    check.close();
    
    // Load into new manager
    PatchManager pm2;
    bool loaded = pm2.loadFromFile(testFile);
    printTest("Load from file succeeded", loaded);
    printTest("Loaded patch count matches", pm2.count() == 3);
    
    // Verify patches match
    const Patch* loaded1 = pm2.getPatch(0);
    printTest("Patch 0 loaded correctly", 
              loaded1 && loaded1->address == 0x001000 && loaded1->enabled);
    
    const Patch* loaded2 = pm2.getPatch(5);
    printTest("Patch 5 loaded correctly", 
              loaded2 && loaded2->address == 0x002000 && !loaded2->enabled);
    
    const Patch* loaded3 = pm2.getPatch(15);
    printTest("Patch 15 loaded correctly", 
              loaded3 && loaded3->address == 0x003000 && loaded3->enabled);
    
    // Verify data arrays
    if (loaded1) {
        bool dataMatch = true;
        for (size_t i = 0; i < 8; ++i) {
            if (loaded1->data[i] != p1.data[i]) {
                dataMatch = false;
                break;
            }
        }
        printTest("Patch 0 data matches", dataMatch);
    }
    
    // Test loading non-existent file
    PatchManager pm3;
    bool loadFailed = !pm3.loadFromFile("/tmp/nonexistent_file.json");
    printTest("Loading non-existent file fails gracefully", loadFailed);
    
    // Clean up
    std::remove(testFile.c_str());
}

// Test 4: JSON format verification
void testJsonFormat() {
    std::cout << "\n=== Test 4: JSON Format Verification ===" << std::endl;
    
    const std::string testFile = "/tmp/test_format.json";
    
    PatchManager pm;
    
    Patch p;
    p.id = 3;
    p.address = 0xABCDEF;
    p.data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    p.enabled = true;
    pm.addPatch(p);
    
    pm.saveToFile(testFile);
    
    // Read and verify format
    std::ifstream file(testFile);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    printTest("JSON contains 'patches' key", content.find("\"patches\"") != std::string::npos);
    printTest("JSON contains id field", content.find("\"id\": 3") != std::string::npos);
    printTest("JSON contains address field", content.find("\"address\": \"0xabcdef\"") != std::string::npos);
    printTest("JSON contains data field", content.find("\"data\": \"0123456789abcdef\"") != std::string::npos);
    printTest("JSON contains enabled field", content.find("\"enabled\": true") != std::string::npos);
    
    std::cout << "\nGenerated JSON:\n" << content << std::endl;
    
    std::remove(testFile.c_str());
}

// Test 5: SPI integration (if hardware available)
void testSPIIntegration() {
    std::cout << "\n=== Test 5: SPI Integration ===" << std::endl;
    
    SPIProtocol spi;
    PatchManager pm;
    
    // Try to open SPI device
    bool connected = spi.open("/dev/spidev0.0", 100000);
    
    if (!connected) {
        std::cout << "⚠ SPI device not available, skipping hardware tests" << std::endl;
        std::cout << "  Error: " << spi.getLastError() << std::endl;
        return;
    }
    
    printTest("Connected to SPI device", true);
    
    // Add some patches
    Patch p1;
    p1.id = 0;
    p1.address = 0x001000;
    p1.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    p1.enabled = true;
    pm.addPatch(p1);
    
    Patch p2;
    p2.id = 1;
    p2.address = 0x002000;
    p2.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    p2.enabled = true;
    pm.addPatch(p2);
    
    // Apply all patches
    bool applied = pm.applyAll(spi);
    printTest("Apply all patches succeeded", applied);
    if (!applied) {
        std::cout << "  Error: " << pm.getLastError() << std::endl;
    }
    
    // Clear all patches
    bool cleared = pm.clearAll(spi);
    printTest("Clear all patches succeeded", cleared);
    if (!cleared) {
        std::cout << "  Error: " << pm.getLastError() << std::endl;
    }
    printTest("Manager is empty after clearAll", pm.empty());
    
    spi.close();
    printTest("Disconnected from SPI", !spi.isConnected());
}

// Test 6: Error handling
void testErrorHandling() {
    std::cout << "\n=== Test 6: Error Handling ===" << std::endl;
    
    PatchManager pm;
    SPIProtocol spi;
    
    // Try to apply patches without connection
    Patch p;
    p.id = 0;
    p.address = 0x001000;
    p.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    p.enabled = true;
    pm.addPatch(p);
    
    bool applyFailed = !pm.applyAll(spi);
    printTest("Apply without connection fails", applyFailed);
    printTest("Error message set", !pm.getLastError().empty());
    
    // Try to clear without connection
    bool clearFailed = !pm.clearAll(spi);
    printTest("Clear without connection fails", clearFailed);
    
    // Try to remove non-existent patch
    bool removeFailed = !pm.removePatch(99);
    printTest("Remove non-existent patch fails", removeFailed);
    
    // Try to load malformed JSON
    const std::string badFile = "/tmp/bad.json";
    std::ofstream bad(badFile);
    bad << "{ this is not valid json }";
    bad.close();
    
    PatchManager pm2;
    bool loadFailed = !pm2.loadFromFile(badFile);
    printTest("Load malformed JSON fails", loadFailed);
    
    std::remove(badFile.c_str());
}

int main() {
    std::cout << "PatchManager Test Suite" << std::endl;
    std::cout << "=======================" << std::endl;
    
    testBasicOperations();
    testValidation();
    testJsonIO();
    testJsonFormat();
    testSPIIntegration();
    testErrorHandling();
    
    std::cout << "\n=======================" << std::endl;
    std::cout << "All tests completed!" << std::endl;
    
    return 0;
}
