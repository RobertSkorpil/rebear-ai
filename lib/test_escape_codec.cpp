#include "rebear/escape_codec.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace rebear;

void printHex(const std::string& label, const std::vector<uint8_t>& data) {
    std::cout << label << ": ";
    for (uint8_t byte : data) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
}

void testCase(const std::string& name, const std::vector<uint8_t>& input, 
              const std::vector<uint8_t>& expectedEncoded) {
    std::cout << "\n=== Test: " << name << " ===" << std::endl;
    printHex("Input", input);
    
    auto encoded = encode(input);
    printHex("Encoded", encoded);
    printHex("Expected", expectedEncoded);
    
    assert(encoded == expectedEncoded && "Encoding mismatch!");
    
    auto decoded = decode(encoded);
    printHex("Decoded", decoded);
    
    assert(decoded == input && "Round-trip failed!");
    std::cout << "✓ PASSED" << std::endl;
}

int main() {
    std::cout << "Testing EscapeCodec Implementation\n" << std::endl;
    
    // Test 1: Empty vector
    testCase("Empty vector", {}, {});
    
    // Test 2: Single byte (no escape needed)
    testCase("Single byte (0x01)", {0x01}, {0x01});
    
    // Test 3: Single 0x4a (should become 0x4d 0x6a)
    testCase("Single 0x4a", {0x4a}, {0x4d, 0x6a});
    
    // Test 4: Single 0x4d (should become 0x4d 0x6d)
    testCase("Single 0x4d", {0x4d}, {0x4d, 0x6d});
    
    // Test 5: Mixed data with escape sequences
    testCase("Mixed data", 
             {0x01, 0x4a, 0x02}, 
             {0x01, 0x4d, 0x6a, 0x02});
    
    // Test 6: Both escape characters
    testCase("Both 0x4a and 0x4d", 
             {0x4a, 0x4d}, 
             {0x4d, 0x6a, 0x4d, 0x6d});
    
    // Test 7: Multiple escape sequences
    testCase("Multiple escapes", 
             {0x00, 0x4a, 0x4d, 0xFF, 0x4a}, 
             {0x00, 0x4d, 0x6a, 0x4d, 0x6d, 0xFF, 0x4d, 0x6a});
    
    // Test 8: No escapes needed
    testCase("No escapes", 
             {0x00, 0x01, 0x02, 0xFF}, 
             {0x00, 0x01, 0x02, 0xFF});
    
    // Test 9: Verify XOR operation
    std::cout << "\n=== Verifying XOR operation ===" << std::endl;
    std::cout << "0x4a XOR 0x20 = 0x" << std::hex << (0x4a ^ 0x20) << " (expected 0x6a)" << std::endl;
    std::cout << "0x4d XOR 0x20 = 0x" << std::hex << (0x4d ^ 0x20) << " (expected 0x6d)" << std::endl;
    assert((0x4a ^ 0x20) == 0x6a);
    assert((0x4d ^ 0x20) == 0x6d);
    std::cout << "✓ XOR operations correct" << std::dec << std::endl;
    
    std::cout << "\n=== All tests PASSED! ===" << std::endl;
    return 0;
}
