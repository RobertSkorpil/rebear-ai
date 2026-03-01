#include "rebear/escape_codec.h"

namespace rebear {

bool needsEscape(uint8_t byte) {
    return byte == IDLE_CHAR || byte == ESCAPE_CHAR;
}

std::vector<uint8_t> encode(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> encoded;
    encoded.reserve(data.size() * 2); // Worst case: all bytes need escaping
    
    for (uint8_t byte : data) {
        if (needsEscape(byte)) {
            // Escape sequence: 0x4d followed by (byte XOR 0x20)
            encoded.push_back(ESCAPE_CHAR);
            encoded.push_back(byte ^ XOR_MASK);
        } else {
            // No escaping needed
            encoded.push_back(byte);
        }
    }
    
    return encoded;
}

std::vector<uint8_t> decode(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> decoded;
    decoded.reserve(data.size()); // At most same size as input
    
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == ESCAPE_CHAR && i + 1 < data.size()) {
            // Escape sequence found: decode next byte
            decoded.push_back(data[i + 1] ^ XOR_MASK);
            ++i; // Skip the escaped byte
        } else {
            // Regular byte
            decoded.push_back(data[i]);
        }
    }
    
    return decoded;
}

} // namespace rebear
