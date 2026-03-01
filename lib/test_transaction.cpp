/**
 * @file test_transaction.cpp
 * @brief Test program for Transaction class
 * 
 * Compile with:
 *   g++ -std=c++17 -I../lib/include -L../build/lib -o test_transaction test_transaction.cpp -lrebear
 * 
 * Run with:
 *   LD_LIBRARY_PATH=../build/lib ./test_transaction
 */

#include "rebear/transaction.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace rebear;

void printBytes(const std::vector<uint8_t>& bytes) {
    std::cout << "  Bytes: ";
    for (uint8_t b : bytes) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << std::endl;
}

void testBasicParsing() {
    std::cout << "\n=== Test 1: Basic Parsing ===" << std::endl;
    
    // Example FPGA response (big-endian):
    // Address: 0x001000 (4096)
    // Count: 0x000100 (256)
    // Timestamp: 0x000F (15 ms)
    uint8_t data[8] = {
        0x00, 0x10, 0x00,  // Address: 0x001000
        0x00, 0x01, 0x00,  // Count: 0x000100
        0x00, 0x0F         // Timestamp: 0x000F
    };
    
    Transaction trans = Transaction::fromBytes(data);
    
    std::cout << "  " << trans.toString() << std::endl;
    assert(trans.address == 0x001000);
    assert(trans.count == 0x000100);
    assert(trans.timestamp == 0x000F);
    assert(trans.isValid());
    assert(!trans.isDummy());
    
    std::cout << "  ✓ Parsing successful" << std::endl;
}

void testSerialization() {
    std::cout << "\n=== Test 2: Serialization ===" << std::endl;
    
    Transaction trans;
    trans.address = 0x002000;
    trans.count = 512;
    trans.timestamp = 100;
    
    std::cout << "  Original: " << trans.toString() << std::endl;
    
    auto bytes = trans.toBytes();
    printBytes(bytes);
    
    // Verify serialization
    assert(bytes.size() == 8);
    assert(bytes[0] == 0x00);
    assert(bytes[1] == 0x20);
    assert(bytes[2] == 0x00);
    assert(bytes[3] == 0x00);
    assert(bytes[4] == 0x02);
    assert(bytes[5] == 0x00);
    assert(bytes[6] == 0x00);
    assert(bytes[7] == 0x64);
    
    std::cout << "  ✓ Serialization successful" << std::endl;
}

void testRoundTrip() {
    std::cout << "\n=== Test 3: Round-Trip Encode/Decode ===" << std::endl;
    
    Transaction original;
    original.address = 0xABCDEF;
    original.count = 1024;
    original.timestamp = 30000;
    
    std::cout << "  Original: " << original.toString() << std::endl;
    
    auto bytes = original.toBytes();
    printBytes(bytes);
    
    Transaction decoded = Transaction::fromBytes(bytes.data());
    std::cout << "  Decoded:  " << decoded.toString() << std::endl;
    
    assert(decoded.address == original.address);
    assert(decoded.count == original.count);
    assert(decoded.timestamp == original.timestamp);
    
    std::cout << "  ✓ Round-trip successful" << std::endl;
}

void testValidation() {
    std::cout << "\n=== Test 4: Validation ===" << std::endl;
    
    // Valid transaction
    Transaction valid;
    valid.address = 0x001000;
    valid.count = 256;
    valid.timestamp = 100;
    assert(valid.isValid());
    std::cout << "  ✓ Valid transaction accepted" << std::endl;
    
    // Invalid address (> 24-bit)
    Transaction invalidAddr;
    invalidAddr.address = 0x1000000;  // Too large
    invalidAddr.count = 256;
    invalidAddr.timestamp = 100;
    assert(!invalidAddr.isValid());
    std::cout << "  ✓ Invalid address rejected" << std::endl;
    
    // Invalid count (> 1MB)
    Transaction invalidCount;
    invalidCount.address = 0x001000;
    invalidCount.count = 2000000;  // Too large
    invalidCount.timestamp = 100;
    assert(!invalidCount.isValid());
    std::cout << "  ✓ Invalid count rejected" << std::endl;
}

void testDummyDetection() {
    std::cout << "\n=== Test 5: Dummy Data Detection ===" << std::endl;
    
    // Dummy data (all 0xFF)
    uint8_t dummyData[8] = {
        0xFF, 0xFF, 0xFF,  // Address: 0xFFFFFF
        0xFF, 0xFF, 0xFF,  // Count: 0xFFFFFF
        0xFF, 0xFF         // Timestamp: 0xFFFF
    };
    
    Transaction dummy = Transaction::fromBytes(dummyData);
    std::cout << "  " << dummy.toString() << std::endl;
    assert(dummy.isDummy());
    assert(!dummy.isValid());  // Dummy data is also invalid
    std::cout << "  ✓ Dummy data detected" << std::endl;
    
    // Non-dummy data
    uint8_t realData[8] = {
        0x00, 0x10, 0x00,
        0x00, 0x01, 0x00,
        0x00, 0x0F
    };
    
    Transaction real = Transaction::fromBytes(realData);
    assert(!real.isDummy());
    std::cout << "  ✓ Real data not flagged as dummy" << std::endl;
}

void testEdgeCases() {
    std::cout << "\n=== Test 6: Edge Cases ===" << std::endl;
    
    // Maximum valid 24-bit address (0xFFFFFF is valid, < 0x1000000)
    Transaction maxAddr;
    maxAddr.address = 0xFFFFFF;
    maxAddr.count = 1;
    maxAddr.timestamp = 0;
    assert(maxAddr.isValid());  // 0xFFFFFF is valid (< 0x1000000)
    std::cout << "  ✓ Maximum 24-bit address valid" << std::endl;
    
    // Just above maximum valid address (invalid)
    Transaction tooLargeAddr;
    tooLargeAddr.address = 0x1000000;
    tooLargeAddr.count = 1;
    tooLargeAddr.timestamp = 0;
    assert(!tooLargeAddr.isValid());
    std::cout << "  ✓ Address >= 0x1000000 rejected" << std::endl;
    
    // Zero values
    Transaction zeros;
    zeros.address = 0;
    zeros.count = 0;
    zeros.timestamp = 0;
    assert(zeros.isValid());
    std::cout << "  ✓ Zero values valid" << std::endl;
    
    // Maximum timestamp
    Transaction maxTime;
    maxTime.address = 0x001000;
    maxTime.count = 256;
    maxTime.timestamp = 0xFFFF;
    assert(maxTime.isValid());
    std::cout << "  ✓ Maximum timestamp valid" << std::endl;
}

int main() {
    std::cout << "Transaction Class Test Suite" << std::endl;
    std::cout << "=============================" << std::endl;
    
    try {
        testBasicParsing();
        testSerialization();
        testRoundTrip();
        testValidation();
        testDummyDetection();
        testEdgeCases();
        
        std::cout << "\n=============================" << std::endl;
        std::cout << "All tests passed! ✓" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
