/**
 * @file example_patch_buffer.cpp
 * @brief Example demonstrating the new patch buffer upload feature
 * 
 * This example shows how to use the new uploadPatchBuffer() method
 * to efficiently upload multiple patches in a single SPI transaction.
 */

#include <rebear/spi_protocol.h>
#include <rebear/patch.h>
#include <iostream>
#include <vector>

using namespace rebear;

// Example 1: Direct buffer upload with SPIProtocol
void example_direct_upload() {
    std::cout << "\n=== Example 1: Direct Buffer Upload ===" << std::endl;
    
    // Create SPI protocol instance
    SPIProtocol spi;
    
    // Open SPI device
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return;
    }
    
    std::cout << "✓ SPI device opened" << std::endl;
    
    // Create multiple patches
    std::vector<Patch> patches;
    
    // Patch 1: Replace bootloader signature at 0x001000
    Patch patch1;
    patch1.address = 0x001000;
    patch1.data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    patch1.enabled = true;
    patches.push_back(patch1);
    
    // Patch 2: Patch configuration bytes at 0x002000
    Patch patch2;
    patch2.address = 0x002000;
    patch2.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    patch2.enabled = true;
    patches.push_back(patch2);
    
    // Patch 3: Disabled patch (for testing)
    Patch patch3;
    patch3.address = 0x003000;
    patch3.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    patch3.enabled = false;  // This won't be applied
    patches.push_back(patch3);
    
    std::cout << "Created " << patches.size() << " patches" << std::endl;
    
    // Upload all patches in one transaction
    if (spi.uploadPatchBuffer(patches)) {
        std::cout << "✓ All patches uploaded successfully!" << std::endl;
    } else {
        std::cerr << "✗ Failed to upload patches: " << spi.getLastError() << std::endl;
    }
    
    spi.close();
}

// Example 2: Using vector for multiple patches
void example_vector_patches() {
    std::cout << "\n=== Example 2: Using Vector for Multiple Patches ===" << std::endl;
    
    // Create patches
    std::vector<Patch> patches;
    
    Patch p1;
    p1.id = 0;
    p1.address = 0x001000;
    p1.data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    p1.enabled = true;
    patches.push_back(p1);
    
    Patch p2;
    p2.id = 1;
    p2.address = 0x002000;
    p2.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    p2.enabled = true;
    patches.push_back(p2);
    
    std::cout << "Created " << patches.size() << " patches" << std::endl;
    
    // Open SPI
    SPIProtocol spi;
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return;
    }
    
    // Apply all patches using efficient buffer method
    if (manager.applyAllBuffer(spi)) {
        std::cout << "✓ All patches applied via buffer upload!" << std::endl;
    } else {
        std::cerr << "✗ Failed to apply patches: " << manager.getLastError() << std::endl;
    }
    
    spi.close();
}

// Example 3: Load from file and apply
void example_load_and_apply() {
    std::cout << "\n=== Example 3: Load from File and Apply ===" << std::endl;
    
    // First, create and save a patch configuration
    PatchManager manager;
    
    Patch p1;
    p1.id = 0;
    p1.address = 0x001000;
    p1.data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    p1.enabled = true;
    manager.addPatch(p1);
    
    Patch p2;
    p2.id = 5;
    p2.address = 0x002000;
    p2.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    p2.enabled = true;
    manager.addPatch(p2);
    
    // Save to file
    if (manager.saveToFile("/tmp/patches.json")) {
        std::cout << "✓ Patches saved to /tmp/patches.json" << std::endl;
    } else {
        std::cerr << "✗ Failed to save: " << manager.getLastError() << std::endl;
        return;
    }
    
    // Now load from file and apply
    PatchManager loader;
    if (!loader.loadFromFile("/tmp/patches.json")) {
        std::cerr << "✗ Failed to load: " << loader.getLastError() << std::endl;
        return;
    }
    
    std::cout << "✓ Loaded " << loader.count() << " patches from file" << std::endl;
    
    // Open SPI and apply
    SPIProtocol spi;
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return;
    }
    
    if (loader.applyAllBuffer(spi)) {
        std::cout << "✓ All patches from file applied successfully!" << std::endl;
    } else {
        std::cerr << "✗ Failed to apply patches: " << loader.getLastError() << std::endl;
    }
    
    spi.close();
}

// Example 4: Backward compatibility - single patch at a time
void example_backward_compatibility() {
    std::cout << "\n=== Example 4: Backward Compatibility ===" << std::endl;
    
    SPIProtocol spi;
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return;
    }
    
    // Old way still works - setPatch() now uses buffer format internally
    Patch patch;
    patch.address = 0x001000;
    patch.data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    patch.enabled = true;
    
    if (spi.setPatch(patch)) {
        std::cout << "✓ Single patch uploaded (using new buffer format internally)" << std::endl;
    } else {
        std::cerr << "✗ Failed to upload patch: " << spi.getLastError() << std::endl;
    }
    
    spi.close();
}

// Example 5: Clear all patches
void example_clear_patches() {
    std::cout << "\n=== Example 5: Clear All Patches ===" << std::endl;
    
    SPIProtocol spi;
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return;
    }
    
    if (spi.clearPatches()) {
        std::cout << "✓ All patches cleared from FPGA" << std::endl;
    } else {
        std::cerr << "✗ Failed to clear patches: " << spi.getLastError() << std::endl;
    }
    
    spi.close();
}

// Example 6: Handling the 8-patch hardware limit
void example_batch_upload() {
    std::cout << "\n=== Example 6: Batch Upload (8-patch limit) ===" << std::endl;
    
    // Create more than 8 patches to demonstrate batching
    std::vector<Patch> allPatches;
    for (int i = 0; i < 20; i++) {
        Patch p;
        p.address = 0x001000 + (i * 0x100);
        p.data = {
            static_cast<uint8_t>(i * 8 + 0),
            static_cast<uint8_t>(i * 8 + 1),
            static_cast<uint8_t>(i * 8 + 2),
            static_cast<uint8_t>(i * 8 + 3),
            static_cast<uint8_t>(i * 8 + 4),
            static_cast<uint8_t>(i * 8 + 5),
            static_cast<uint8_t>(i * 8 + 6),
            static_cast<uint8_t>(i * 8 + 7)
        };
        p.enabled = true;
        allPatches.push_back(p);
    }
    
    std::cout << "Total patches to upload: " << allPatches.size() << std::endl;
    std::cout << "Hardware limit: 8 patches per buffer" << std::endl;
    
    SPIProtocol spi;
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return;
    }
    
    // Split into batches of 8
    size_t batchCount = 0;
    for (size_t i = 0; i < allPatches.size(); i += 8) {
        size_t count = std::min(size_t(8), allPatches.size() - i);
        std::vector<Patch> batch(allPatches.begin() + i, allPatches.begin() + i + count);
        
        std::cout << "Uploading batch " << (batchCount + 1) 
                  << " (" << batch.size() << " patches)..." << std::endl;
        
        if (spi.uploadPatchBuffer(batch)) {
            std::cout << "  ✓ Batch " << (batchCount + 1) << " uploaded successfully" << std::endl;
        } else {
            std::cerr << "  ✗ Batch " << (batchCount + 1) << " failed: " 
                      << spi.getLastError() << std::endl;
        }
        
        batchCount++;
    }
    
    std::cout << "✓ All " << allPatches.size() << " patches uploaded in " 
              << batchCount << " batches" << std::endl;
    
    spi.close();
}

// Example 7: Comparison - Old vs New method
void example_performance_comparison() {
    std::cout << "\n=== Example 6: Performance Comparison ===" << std::endl;
    
    // Create test patches (8 max for hardware)
    std::vector<Patch> patches;
    for (int i = 0; i < 8; i++) {
        Patch p;
        p.address = 0x001000 + (i * 0x100);
        p.data = {
            static_cast<uint8_t>(i * 8 + 0),
            static_cast<uint8_t>(i * 8 + 1),
            static_cast<uint8_t>(i * 8 + 2),
            static_cast<uint8_t>(i * 8 + 3),
            static_cast<uint8_t>(i * 8 + 4),
            static_cast<uint8_t>(i * 8 + 5),
            static_cast<uint8_t>(i * 8 + 6),
            static_cast<uint8_t>(i * 8 + 7)
        };
        p.enabled = true;
        patches.push_back(p);
    }
    
    SPIProtocol spi;
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return;
    }
    
    std::cout << "Uploading " << patches.size() << " patches (max allowed)..." << std::endl;
    
    // Old way (commented out to avoid actually doing it)
    // std::cout << "Old method: " << patches.size() << " SPI transactions" << std::endl;
    // for (const auto& patch : patches) {
    //     spi.setPatch(patch);
    // }
    
    // New way
    std::cout << "New method: 1 SPI transaction" << std::endl;
    if (spi.uploadPatchBuffer(patches)) {
        std::cout << "✓ All " << patches.size() << " patches uploaded in single transaction!" << std::endl;
        std::cout << "  Buffer size: " << (patches.size() * 16 + 1) << " bytes" << std::endl;
        std::cout << "  Efficiency: " << patches.size() << "× fewer SPI transactions" << std::endl;
    } else {
        std::cerr << "✗ Failed: " << spi.getLastError() << std::endl;
    }
    
    spi.close();
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  Patch Buffer Upload Examples" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    std::cout << "\nNote: These examples require actual FPGA hardware." << std::endl;
    std::cout << "If hardware is not present, errors are expected." << std::endl;
    std::cout << "\n⚠️  IMPORTANT: Hardware supports maximum 8 patches per buffer" << std::endl;
    
    try {
        example_direct_upload();
        example_patch_manager();
        example_load_and_apply();
        example_backward_compatibility();
        example_clear_patches();
        example_batch_upload();
        example_performance_comparison();
        
        std::cout << "\n==================================================" << std::endl;
        std::cout << "  Examples completed!" << std::endl;
        std::cout << "==================================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Example failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
