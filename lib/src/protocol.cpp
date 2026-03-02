#include "rebear/protocol.h"
#include <cstring>
#include <arpa/inet.h>

namespace rebear {
namespace protocol {

std::vector<uint8_t> encodeMessage(const Message& msg) {
    std::vector<uint8_t> result;
    
    // Calculate total length
    uint16_t length = MIN_MESSAGE_SIZE + msg.payload.size();
    
    // Add magic bytes
    result.push_back(MAGIC_BYTE_1);
    result.push_back(MAGIC_BYTE_2);
    
    // Add length (big-endian)
    result.push_back((length >> 8) & 0xFF);
    result.push_back(length & 0xFF);
    
    // Add type
    result.push_back(msg.type);
    
    // Add payload
    result.insert(result.end(), msg.payload.begin(), msg.payload.end());
    
    return result;
}

bool decodeMessage(const std::vector<uint8_t>& data, Message& msg) {
    // Check minimum size
    if (data.size() < MIN_MESSAGE_SIZE) {
        return false;
    }
    
    // Check magic bytes
    if (data[0] != MAGIC_BYTE_1 || data[1] != MAGIC_BYTE_2) {
        return false;
    }
    
    // Extract length
    uint16_t length = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    
    // Validate length
    if (length < MIN_MESSAGE_SIZE || length > MAX_MESSAGE_SIZE) {
        return false;
    }
    
    // Check if we have enough data
    if (data.size() < length) {
        return false;
    }
    
    // Extract type
    msg.type = data[4];
    
    // Extract payload
    msg.payload.clear();
    if (length > MIN_MESSAGE_SIZE) {
        msg.payload.insert(msg.payload.end(), 
                          data.begin() + MIN_MESSAGE_SIZE, 
                          data.begin() + length);
    }
    
    return true;
}

void encodeString(std::vector<uint8_t>& buffer, const std::string& str) {
    uint16_t length = str.length();
    encodeUint16(buffer, length);
    buffer.insert(buffer.end(), str.begin(), str.end());
}

bool decodeString(const std::vector<uint8_t>& buffer, size_t& offset, std::string& str) {
    uint16_t length;
    if (!decodeUint16(buffer, offset, length)) {
        return false;
    }
    
    if (offset + length > buffer.size()) {
        return false;
    }
    
    str.assign(buffer.begin() + offset, buffer.begin() + offset + length);
    offset += length;
    return true;
}

void encodeUint16(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void encodeUint32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back((value >> 24) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void encodeUint64(std::vector<uint8_t>& buffer, uint64_t value) {
    buffer.push_back((value >> 56) & 0xFF);
    buffer.push_back((value >> 48) & 0xFF);
    buffer.push_back((value >> 40) & 0xFF);
    buffer.push_back((value >> 32) & 0xFF);
    buffer.push_back((value >> 24) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

bool decodeUint16(const std::vector<uint8_t>& buffer, size_t& offset, uint16_t& value) {
    if (offset + 2 > buffer.size()) {
        return false;
    }
    
    value = (static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1];
    offset += 2;
    return true;
}

bool decodeUint32(const std::vector<uint8_t>& buffer, size_t& offset, uint32_t& value) {
    if (offset + 4 > buffer.size()) {
        return false;
    }
    
    value = (static_cast<uint32_t>(buffer[offset]) << 24) |
            (static_cast<uint32_t>(buffer[offset + 1]) << 16) |
            (static_cast<uint32_t>(buffer[offset + 2]) << 8) |
            buffer[offset + 3];
    offset += 4;
    return true;
}

bool decodeUint64(const std::vector<uint8_t>& buffer, size_t& offset, uint64_t& value) {
    if (offset + 8 > buffer.size()) {
        return false;
    }
    
    value = (static_cast<uint64_t>(buffer[offset]) << 56) |
            (static_cast<uint64_t>(buffer[offset + 1]) << 48) |
            (static_cast<uint64_t>(buffer[offset + 2]) << 40) |
            (static_cast<uint64_t>(buffer[offset + 3]) << 32) |
            (static_cast<uint64_t>(buffer[offset + 4]) << 24) |
            (static_cast<uint64_t>(buffer[offset + 5]) << 16) |
            (static_cast<uint64_t>(buffer[offset + 6]) << 8) |
            buffer[offset + 7];
    offset += 8;
    return true;
}

void encodeByte(std::vector<uint8_t>& buffer, uint8_t value) {
    buffer.push_back(value);
}

bool decodeByte(const std::vector<uint8_t>& buffer, size_t& offset, uint8_t& value) {
    if (offset >= buffer.size()) {
        return false;
    }
    
    value = buffer[offset];
    offset++;
    return true;
}

} // namespace protocol
} // namespace rebear
