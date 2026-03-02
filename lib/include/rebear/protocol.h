#ifndef REBEAR_PROTOCOL_H
#define REBEAR_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <string>

namespace rebear {
namespace protocol {

// Magic bytes for protocol identification
constexpr uint8_t MAGIC_BYTE_1 = 0x52;  // 'R'
constexpr uint8_t MAGIC_BYTE_2 = 0x42;  // 'B'

// Minimum message size (magic + length + type)
constexpr size_t MIN_MESSAGE_SIZE = 5;

// Maximum message size (64KB)
constexpr size_t MAX_MESSAGE_SIZE = 65536;

// Command type definitions
namespace CommandType {
    // Control commands (0x00-0x0F)
    constexpr uint8_t PING = 0x00;
    constexpr uint8_t PONG = 0x01;
    constexpr uint8_t ERROR = 0x02;
    constexpr uint8_t DISCONNECT = 0x03;
    
    // SPI commands (0x10-0x1F)
    constexpr uint8_t SPI_OPEN = 0x10;
    constexpr uint8_t SPI_OPEN_RESPONSE = 0x11;
    constexpr uint8_t SPI_CLOSE = 0x12;
    constexpr uint8_t SPI_CLOSE_RESPONSE = 0x13;
    constexpr uint8_t SPI_CLEAR_TRANSACTIONS = 0x14;
    constexpr uint8_t SPI_CLEAR_TRANSACTIONS_RESPONSE = 0x15;
    constexpr uint8_t SPI_READ_TRANSACTION = 0x16;
    constexpr uint8_t SPI_READ_TRANSACTION_RESPONSE = 0x17;
    constexpr uint8_t SPI_SET_PATCH = 0x18;
    constexpr uint8_t SPI_SET_PATCH_RESPONSE = 0x19;
    constexpr uint8_t SPI_CLEAR_PATCHES = 0x1A;
    constexpr uint8_t SPI_CLEAR_PATCHES_RESPONSE = 0x1B;
    
    // GPIO commands (0x20-0x2F)
    constexpr uint8_t GPIO_INIT = 0x20;
    constexpr uint8_t GPIO_INIT_RESPONSE = 0x21;
    constexpr uint8_t GPIO_CLOSE = 0x22;
    constexpr uint8_t GPIO_CLOSE_RESPONSE = 0x23;
    constexpr uint8_t GPIO_WRITE = 0x24;
    constexpr uint8_t GPIO_WRITE_RESPONSE = 0x25;
    constexpr uint8_t GPIO_READ = 0x26;
    constexpr uint8_t GPIO_READ_RESPONSE = 0x27;
    constexpr uint8_t GPIO_WAIT_EDGE = 0x28;
    constexpr uint8_t GPIO_WAIT_EDGE_RESPONSE = 0x29;
}

// Error codes
namespace ErrorCode {
    constexpr uint8_t NONE = 0x00;
    constexpr uint8_t INVALID_COMMAND = 0x01;
    constexpr uint8_t INVALID_PAYLOAD = 0x02;
    constexpr uint8_t HARDWARE_ERROR = 0x03;
    constexpr uint8_t TIMEOUT = 0x04;
    constexpr uint8_t INTERNAL_ERROR = 0x05;
}

// Message structure
struct Message {
    uint8_t type;
    std::vector<uint8_t> payload;
    
    Message() : type(0) {}
    Message(uint8_t t, const std::vector<uint8_t>& p) : type(t), payload(p) {}
};

// Utility functions for encoding/decoding

// Encode a message into wire format
std::vector<uint8_t> encodeMessage(const Message& msg);

// Decode a message from wire format
// Returns true on success, false on error
bool decodeMessage(const std::vector<uint8_t>& data, Message& msg);

// String encoding/decoding helpers
void encodeString(std::vector<uint8_t>& buffer, const std::string& str);
bool decodeString(const std::vector<uint8_t>& buffer, size_t& offset, std::string& str);

// Integer encoding/decoding helpers (big-endian)
void encodeUint16(std::vector<uint8_t>& buffer, uint16_t value);
void encodeUint32(std::vector<uint8_t>& buffer, uint32_t value);
void encodeUint64(std::vector<uint8_t>& buffer, uint64_t value);

bool decodeUint16(const std::vector<uint8_t>& buffer, size_t& offset, uint16_t& value);
bool decodeUint32(const std::vector<uint8_t>& buffer, size_t& offset, uint32_t& value);
bool decodeUint64(const std::vector<uint8_t>& buffer, size_t& offset, uint64_t& value);

// Byte encoding/decoding
void encodeByte(std::vector<uint8_t>& buffer, uint8_t value);
bool decodeByte(const std::vector<uint8_t>& buffer, size_t& offset, uint8_t& value);

} // namespace protocol
} // namespace rebear

#endif // REBEAR_PROTOCOL_H
