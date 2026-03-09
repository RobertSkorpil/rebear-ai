#include "rebear/patch.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace rebear {

Patch::Patch()
    : id(0), address(0), data(), enabled(true) {
}

Patch::Patch(uint8_t id, uint32_t address, const std::vector<uint8_t>& data, bool enabled)
    : id(id), address(address), data(data), enabled(enabled) {
}

// Backward compatibility constructor
Patch::Patch(uint8_t id, uint32_t address, const std::array<uint8_t, 8>& data, bool enabled)
    : id(id), address(address), data(data.begin(), data.end()), enabled(enabled) {
}

std::vector<uint8_t> Patch::toBytes() const {
    std::vector<uint8_t> bytes;
    bytes.reserve(12);
    
    // Byte 0: Patch ID
    bytes.push_back(id);
    
    // Bytes 1-3: 24-bit address (big-endian)
    bytes.push_back(static_cast<uint8_t>((address >> 16) & 0xFF));  // MSB
    bytes.push_back(static_cast<uint8_t>((address >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(address & 0xFF));          // LSB
    
    // Bytes 4-11: First 8 bytes of replacement data (legacy format)
    for (size_t i = 0; i < 8 && i < data.size(); ++i) {
        bytes.push_back(data[i]);
    }
    // Pad with zeros if less than 8 bytes
    while (bytes.size() < 12) {
        bytes.push_back(0);
    }
    
    return bytes;
}

Patch Patch::fromBytes(const uint8_t* data_ptr) {
    if (data_ptr == nullptr) {
        throw std::invalid_argument("Patch::fromBytes: null data pointer");
    }
    
    Patch patch;
    
    // Byte 0: Patch ID
    patch.id = data_ptr[0];
    
    // Bytes 1-3: 24-bit address (big-endian)
    patch.address = (static_cast<uint32_t>(data_ptr[1]) << 16) |
                    (static_cast<uint32_t>(data_ptr[2]) << 8) |
                    static_cast<uint32_t>(data_ptr[3]);
    
    // Bytes 4-11: 8 bytes of replacement data
    patch.data.assign(data_ptr + 4, data_ptr + 12);
    
    // Default to enabled
    patch.enabled = true;
    
    return patch;
}

std::string Patch::toString() const {
    std::ostringstream oss;
    
    oss << "Patch ID=" << static_cast<int>(id)
        << " Addr=0x" << std::hex << std::setw(6) << std::setfill('0') << address
        << " Data=[";
    
    size_t displayCount = std::min(data.size(), size_t(16)); // Show first 16 bytes
    for (size_t i = 0; i < displayCount; ++i) {
        if (i > 0) oss << " ";
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    
    if (data.size() > 16) {
        oss << " ...";
    }
    
    oss << "] (" << std::dec << data.size() << " bytes) Enabled=" << (enabled ? "true" : "false");
    
    return oss.str();
}

bool Patch::isValid() const {
    // ID must be 0-15
    if (id > 15) {
        return false;
    }
    
    // Address must be < 0x1000000 (24-bit max)
    if (address >= 0x1000000) {
        return false;
    }
    
    // Data must not be empty and must be reasonable size (< 64KB)
    if (data.empty() || data.size() > 65535) {
        return false;
    }
    
    return true;
}

bool Patch::operator==(const Patch& other) const {
    return id == other.id &&
           address == other.address &&
           data == other.data &&
           enabled == other.enabled;
}

bool Patch::operator!=(const Patch& other) const {
    return !(*this == other);
}

} // namespace rebear
