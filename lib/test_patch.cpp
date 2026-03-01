#include "rebear/patch.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace rebear;

void test_default_constructor() {
    std::cout << "Testing default constructor..." << std::endl;
    
    Patch patch;
    assert(patch.id == 0);
    assert(patch.address == 0);
    assert(patch.enabled == true);
    
    // Check data is zeroed
    for (size_t i = 0; i < patch.data.size(); ++i) {
        assert(patch.data[i] == 0);
    }
    
    std::cout << "  ✓ Default constructor works" << std::endl;
}

void test_parameterized_constructor() {
    std::cout << "Testing parameterized constructor..." << std::endl;
    
    std::array<uint8_t, 8> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Patch patch(5, 0x001000, data, true);
    
    assert(patch.id == 5);
    assert(patch.address == 0x001000);
    assert(patch.enabled == true);
    assert(patch.data == data);
    
    std::cout << "  ✓ Parameterized constructor works" << std::endl;
}

void test_to_bytes() {
    std::cout << "Testing toBytes()..." << std::endl;
    
    std::array<uint8_t, 8> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Patch patch(0, 0x001000, data);
    
    auto bytes = patch.toBytes();
    
    // Should be 12 bytes
    assert(bytes.size() == 12);
    
    // Byte 0: ID
    assert(bytes[0] == 0);
    
    // Bytes 1-3: Address (big-endian)
    assert(bytes[1] == 0x00);  // MSB
    assert(bytes[2] == 0x10);
    assert(bytes[3] == 0x00);  // LSB
    
    // Bytes 4-11: Data
    for (size_t i = 0; i < 8; ++i) {
        assert(bytes[4 + i] == data[i]);
    }
    
    std::cout << "  ✓ toBytes() produces correct format" << std::endl;
}

void test_to_bytes_different_address() {
    std::cout << "Testing toBytes() with different address..." << std::endl;
    
    std::array<uint8_t, 8> data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    Patch patch(15, 0xABCDEF, data);
    
    auto bytes = patch.toBytes();
    
    assert(bytes.size() == 12);
    assert(bytes[0] == 15);
    assert(bytes[1] == 0xAB);  // MSB
    assert(bytes[2] == 0xCD);
    assert(bytes[3] == 0xEF);  // LSB
    
    for (size_t i = 0; i < 8; ++i) {
        assert(bytes[4 + i] == 0xFF);
    }
    
    std::cout << "  ✓ toBytes() handles different addresses correctly" << std::endl;
}

void test_from_bytes() {
    std::cout << "Testing fromBytes()..." << std::endl;
    
    uint8_t data[12] = {
        0x05,              // ID
        0x00, 0x20, 0x00,  // Address: 0x002000 (big-endian)
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22  // Data
    };
    
    Patch patch = Patch::fromBytes(data);
    
    assert(patch.id == 5);
    assert(patch.address == 0x002000);
    assert(patch.enabled == true);  // Default
    
    assert(patch.data[0] == 0xAA);
    assert(patch.data[1] == 0xBB);
    assert(patch.data[2] == 0xCC);
    assert(patch.data[3] == 0xDD);
    assert(patch.data[4] == 0xEE);
    assert(patch.data[5] == 0xFF);
    assert(patch.data[6] == 0x11);
    assert(patch.data[7] == 0x22);
    
    std::cout << "  ✓ fromBytes() parses correctly" << std::endl;
}

void test_round_trip() {
    std::cout << "Testing round-trip (toBytes -> fromBytes)..." << std::endl;
    
    std::array<uint8_t, 8> data = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    Patch original(7, 0x123456, data, true);
    
    auto bytes = original.toBytes();
    Patch reconstructed = Patch::fromBytes(bytes.data());
    
    assert(reconstructed.id == original.id);
    assert(reconstructed.address == original.address);
    assert(reconstructed.data == original.data);
    // Note: enabled is not serialized, so it defaults to true
    
    std::cout << "  ✓ Round-trip serialization works" << std::endl;
}

void test_to_string() {
    std::cout << "Testing toString()..." << std::endl;
    
    std::array<uint8_t, 8> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Patch patch(3, 0x001000, data, true);
    
    std::string str = patch.toString();
    
    // Should contain key information
    assert(str.find("ID=3") != std::string::npos);
    assert(str.find("0x001000") != std::string::npos);
    assert(str.find("Enabled=true") != std::string::npos);
    
    std::cout << "  String: " << str << std::endl;
    std::cout << "  ✓ toString() produces readable output" << std::endl;
}

void test_is_valid() {
    std::cout << "Testing isValid()..." << std::endl;
    
    std::array<uint8_t, 8> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    
    // Valid patch
    Patch valid(5, 0x001000, data);
    assert(valid.isValid() == true);
    
    // Invalid ID (> 15)
    Patch invalid_id(16, 0x001000, data);
    assert(invalid_id.isValid() == false);
    
    // Invalid address (>= 0x1000000)
    Patch invalid_addr(5, 0x1000000, data);
    assert(invalid_addr.isValid() == false);
    
    // Edge case: ID = 15 (max valid)
    Patch edge_id(15, 0x001000, data);
    assert(edge_id.isValid() == true);
    
    // Edge case: Address = 0xFFFFFF (max valid)
    Patch edge_addr(5, 0xFFFFFF, data);
    assert(edge_addr.isValid() == true);
    
    std::cout << "  ✓ isValid() correctly validates patches" << std::endl;
}

void test_equality() {
    std::cout << "Testing equality operators..." << std::endl;
    
    std::array<uint8_t, 8> data1 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::array<uint8_t, 8> data2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09};
    
    Patch patch1(5, 0x001000, data1, true);
    Patch patch2(5, 0x001000, data1, true);
    Patch patch3(5, 0x001000, data2, true);
    Patch patch4(6, 0x001000, data1, true);
    
    assert(patch1 == patch2);
    assert(patch1 != patch3);
    assert(patch1 != patch4);
    
    std::cout << "  ✓ Equality operators work correctly" << std::endl;
}

void test_edge_cases() {
    std::cout << "Testing edge cases..." << std::endl;
    
    // Zero address
    std::array<uint8_t, 8> data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Patch zero_addr(0, 0x000000, data);
    assert(zero_addr.isValid() == true);
    
    auto bytes = zero_addr.toBytes();
    assert(bytes[1] == 0x00);
    assert(bytes[2] == 0x00);
    assert(bytes[3] == 0x00);
    
    // All 0xFF data
    std::array<uint8_t, 8> ff_data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    Patch ff_patch(15, 0xFFFFFF, ff_data);
    assert(ff_patch.isValid() == true);
    
    bytes = ff_patch.toBytes();
    assert(bytes[0] == 15);
    assert(bytes[1] == 0xFF);
    assert(bytes[2] == 0xFF);
    assert(bytes[3] == 0xFF);
    for (size_t i = 4; i < 12; ++i) {
        assert(bytes[i] == 0xFF);
    }
    
    std::cout << "  ✓ Edge cases handled correctly" << std::endl;
}

void test_disabled_patch() {
    std::cout << "Testing disabled patch..." << std::endl;
    
    std::array<uint8_t, 8> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Patch patch(5, 0x001000, data, false);
    
    assert(patch.enabled == false);
    assert(patch.isValid() == true);  // Still valid even if disabled
    
    std::string str = patch.toString();
    assert(str.find("Enabled=false") != std::string::npos);
    
    std::cout << "  ✓ Disabled patches work correctly" << std::endl;
}

int main() {
    std::cout << "=== Patch Class Tests ===" << std::endl << std::endl;
    
    try {
        test_default_constructor();
        test_parameterized_constructor();
        test_to_bytes();
        test_to_bytes_different_address();
        test_from_bytes();
        test_round_trip();
        test_to_string();
        test_is_valid();
        test_equality();
        test_edge_cases();
        test_disabled_patch();
        
        std::cout << std::endl << "=== All tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << std::endl << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
