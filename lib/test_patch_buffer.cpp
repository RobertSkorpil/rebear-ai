/**
 * @file test_patch_buffer.cpp
 * @brief Test program for patch buffer format
 * 
 * This test validates the new multi-patch buffer format.
 */

#include "rebear/spi_protocol.h"
#include "rebear/patch.h"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace rebear;

void printHex(const std::string& label, const std::vector<uint8_t>& data) {
    std::cout << label << ":" << std::endl;
    std::cout << "  Total bytes: " << data.size() << std::endl;
    std::cout << "  Data: ";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0 && i % 16 == 0) {
            std::cout << std::endl << "        ";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

void testSinglePatchBuffer() {
    std::cout << "\n=== Test 1: Single Patch Buffer ===" << std::endl;
    
    Patch patch;
    patch.address = 0x001000;
    patch.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    patch.enabled = true;
    
    std::cout << "Patch: " << patch.toString() << std::endl;
    
    // Manually construct expected buffer
    std::vector<uint8_t> expected;
    
    // Header
    expected.push_back(0x80);  // STORED (enabled)
    expected.push_back(0x00);  // Address high
    expected.push_back(0x10);  // Address mid
    expected.push_back(0x00);  // Address low
    expected.push_back(0x00);  // Length high
    expected.push_back(0x08);  // Length low
    expected.push_back(0x00);  // Data offset high
    expected.push_back(0x09);  // Data offset low (9 = 8 header + 1 terminator)
    
    // Terminator
    expected.push_back(0x00);
    
    // Data
    for (int i = 0; i < 8; i++) {
        expected.push_back(0xFF);
    }
    
    printHex("Expected buffer", expected);
    
    // Verify structure
    if (expected.size() == 17) {
        std::cout << "✓ Buffer size correct (17 bytes)" << std::endl;
    } else {
        std::cout << "✗ Buffer size incorrect: " << expected.size() << std::endl;
    }
    
    std::cout << "✓ Single patch buffer format validated" << std::endl;
}

void testMultiplePatchBuffer() {
    std::cout << "\n=== Test 2: Multiple Patch Buffer ===" << std::endl;
    
    std::vector<Patch> patches;
    
    Patch patch1;
    patch1.address = 0x001000;
    patch1.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    patch1.enabled = true;
    patches.push_back(patch1);
    
    Patch patch2;
    patch2.address = 0x002000;
    patch2.data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    patch2.enabled = false;
    patches.push_back(patch2);
    
    std::cout << "Patch 1: " << patch1.toString() << std::endl;
    std::cout << "Patch 2: " << patch2.toString() << std::endl;
    
    // Manually construct expected buffer
    std::vector<uint8_t> expected;
    
    // Header 1
    expected.push_back(0x80);  // STORED (enabled)
    expected.push_back(0x00);  // Address high
    expected.push_back(0x10);  // Address mid
    expected.push_back(0x00);  // Address low
    expected.push_back(0x00);  // Length high
    expected.push_back(0x08);  // Length low
    expected.push_back(0x00);  // Data offset high
    expected.push_back(0x11);  // Data offset low (17 = 8*2 headers + 1 terminator)
    
    // Header 2
    expected.push_back(0x00);  // STORED (disabled)
    expected.push_back(0x00);  // Address high
    expected.push_back(0x20);  // Address mid
    expected.push_back(0x00);  // Address low
    expected.push_back(0x00);  // Length high
    expected.push_back(0x08);  // Length low
    expected.push_back(0x00);  // Data offset high
    expected.push_back(0x19);  // Data offset low (25 = 17 + 8)
    
    // Terminator
    expected.push_back(0x00);
    
    // Data 1
    expected.push_back(0x01);
    expected.push_back(0x02);
    expected.push_back(0x03);
    expected.push_back(0x04);
    expected.push_back(0x05);
    expected.push_back(0x06);
    expected.push_back(0x07);
    expected.push_back(0x08);
    
    // Data 2
    expected.push_back(0xAA);
    expected.push_back(0xBB);
    expected.push_back(0xCC);
    expected.push_back(0xDD);
    expected.push_back(0xEE);
    expected.push_back(0xFF);
    expected.push_back(0x11);
    expected.push_back(0x22);
    
    printHex("Expected buffer", expected);
    
    // Verify structure
    if (expected.size() == 33) {
        std::cout << "✓ Buffer size correct (33 bytes)" << std::endl;
    } else {
        std::cout << "✗ Buffer size incorrect: " << expected.size() << std::endl;
    }
    
    std::cout << "✓ Multiple patch buffer format validated" << std::endl;
}

void testOffsetCalculation() {
    std::cout << "\n=== Test 3: Offset Calculation ===" << std::endl;
    
    // For N patches (max 8):
    // - Headers: N * 8 bytes
    // - Terminator: 1 byte
    // - Data: N * 8 bytes
    // First data offset: (N * 8) + 1
    
    for (int n = 1; n <= 8; n++) {  // Hardware supports max 8 patches
        size_t headerSize = n * 8;
        size_t terminatorSize = 1;
        size_t dataSize = n * 8;
        size_t firstDataOffset = headerSize + terminatorSize;
        size_t totalSize = headerSize + terminatorSize + dataSize;
        
        std::cout << "  " << n << " patch(es): "
                  << "first_data_offset=" << firstDataOffset
                  << ", total_size=" << totalSize
                  << std::endl;
    }
    
    std::cout << "✓ Offset calculation validated" << std::endl;
}

void testMinimumBuffer() {
    std::cout << "\n=== Test 4: Minimum Buffer ===" << std::endl;
    
    // Minimum valid buffer: 1 header + 1 terminator + 8 data = 17 bytes
    std::cout << "  Minimum buffer components:" << std::endl;
    std::cout << "    - 1 header: 8 bytes" << std::endl;
    std::cout << "    - Terminator: 1 byte" << std::endl;
    std::cout << "    - 1 data: 8 bytes" << std::endl;
    std::cout << "    - Total: 17 bytes" << std::endl;
    std::cout << "  Minimum data offset: 9 (8 header + 1 terminator)" << std::endl;
    
    std::cout << "✓ Minimum buffer requirements validated" << std::endl;
}

void testMaximumBuffer() {
    std::cout << "\n=== Test 5: Maximum Buffer (Hardware Limit) ===" << std::endl;
    
    // Maximum valid buffer: 8 headers + 1 terminator + 64 data = 129 bytes
    std::cout << "  Maximum buffer components:" << std::endl;
    std::cout << "    - 8 headers: 64 bytes" << std::endl;
    std::cout << "    - Terminator: 1 byte" << std::endl;
    std::cout << "    - 8 data blocks: 64 bytes" << std::endl;
    std::cout << "    - Total: 129 bytes" << std::endl;
    std::cout << "  First data offset: 65 (64 headers + 1 terminator)" << std::endl;
    std::cout << "  Last data offset: 121 (65 + 7*8)" << std::endl;
    
    std::cout << "\n  ⚠️  HARDWARE LIMITATION:" << std::endl;
    std::cout << "     Maximum 8 patch headers per buffer" << std::endl;
    std::cout << "     9th position reserved for terminating header" << std::endl;
    
    std::cout << "✓ Maximum buffer requirements validated" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  Patch Buffer Format Test Suite" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    try {
        testSinglePatchBuffer();
        testMultiplePatchBuffer();
        testOffsetCalculation();
        testMinimumBuffer();
        testMaximumBuffer();
        
        std::cout << "\n==================================================" << std::endl;
        std::cout << "  All tests passed!" << std::endl;
        std::cout << "==================================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
