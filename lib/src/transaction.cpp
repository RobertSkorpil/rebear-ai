#include "rebear/transaction.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace rebear {

Transaction::Transaction()
    : address(0)
    , count(0)
    , timestamp(0)
    , captureTime(std::chrono::system_clock::now())
{
}

Transaction Transaction::fromBytes(const uint8_t* data) {
    if (data == nullptr) {
        throw std::invalid_argument("Transaction::fromBytes: data pointer is null");
    }
    
    Transaction trans;
    
    // Parse 24-bit address (big-endian, bytes 0-2)
    trans.address = (static_cast<uint32_t>(data[0]) << 16) |
                    (static_cast<uint32_t>(data[1]) << 8) |
                    (static_cast<uint32_t>(data[2]));
    
    // Parse 24-bit count (big-endian, bytes 3-5)
    trans.count = (static_cast<uint32_t>(data[3]) << 16) |
                  (static_cast<uint32_t>(data[4]) << 8) |
                  (static_cast<uint32_t>(data[5]));
    
    // Parse 16-bit timestamp (big-endian, bytes 6-7)
    trans.timestamp = (static_cast<uint16_t>(data[6]) << 8) |
                      (static_cast<uint16_t>(data[7]));
    
    // Set capture time to current system time
    trans.captureTime = std::chrono::system_clock::now();
    
    return trans;
}

std::vector<uint8_t> Transaction::toBytes() const {
    std::vector<uint8_t> bytes(8);
    
    // Serialize 24-bit address (big-endian, bytes 0-2)
    bytes[0] = static_cast<uint8_t>((address >> 16) & 0xFF);
    bytes[1] = static_cast<uint8_t>((address >> 8) & 0xFF);
    bytes[2] = static_cast<uint8_t>(address & 0xFF);
    
    // Serialize 24-bit count (big-endian, bytes 3-5)
    bytes[3] = static_cast<uint8_t>((count >> 16) & 0xFF);
    bytes[4] = static_cast<uint8_t>((count >> 8) & 0xFF);
    bytes[5] = static_cast<uint8_t>(count & 0xFF);
    
    // Serialize 16-bit timestamp (big-endian, bytes 6-7)
    bytes[6] = static_cast<uint8_t>((timestamp >> 8) & 0xFF);
    bytes[7] = static_cast<uint8_t>(timestamp & 0xFF);
    
    return bytes;
}

std::string Transaction::toString() const {
    std::ostringstream oss;
    oss << "Transaction(addr=0x" 
        << std::hex << std::setfill('0') << std::setw(6) << address
        << ", count=" << std::dec << count
        << ", time=" << timestamp << "ms)";
    return oss.str();
}

bool Transaction::isValid() const {
    // Address must be within 24-bit range
    if (address >= 0x1000000) {
        return false;
    }
    
    // Count should be reasonable (< 1MB) OR be the special patched value
    if (count >= 1048576 && count != 0xFFFFFF) {
        return false;
    }
    
    // Timestamp is always valid (uint16_t is inherently within 16-bit range)
    // No check needed
    
    return true;
}

bool Transaction::isDummy() const {
    // Dummy data from empty buffer: all 0xFF bytes
    // This translates to:
    //   address = 0xFFFFFF (invalid 24-bit address)
    //   count = 0xFFFFFF (invalid count)
    //   timestamp = 0xFFFF (max timestamp, but valid)
    return (address == 0xFFFFFF) && (count == 0xFFFFFF) && (timestamp == 0xFFFF);
}

bool Transaction::isPatched() const {
    // When FPGA actively patches a transaction, it doesn't count the data
    // and sets count to 0xFFFFFF to indicate patch was applied
    return (count == 0xFFFFFF);
}

} // namespace rebear
