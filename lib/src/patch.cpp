#include "rebear/patch.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace rebear {

Patch::Patch()
    : id(0), address(0), data{}, enabled(true) {
}

Patch::Patch(uint8_t id, uint32_t address, const std::array<uint8_t, 8>& data, bool enabled)
    : id(id), address(address), data(data), enabled(enabled) {
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
    
    // Bytes 4-11: 8 bytes of replacement data
    bytes.insert(bytes.end(), data.begin(), data.end());
    
    return bytes;
}

Patch Patch::fromBytes(const uint8_t* data) {
    if (data == nullptr) {
        throw std::invalid_argument("Patch::fromBytes: null data pointer");
    }
    
    Patch patch;
    
    // Byte 0: Patch ID
    patch.id = data[0];
    
    // Bytes 1-3: 24-bit address (big-endian)
    patch.address = (static_cast<uint32_t>(data[1]) << 16) |
                    (static_cast<uint32_t>(data[2]) << 8) |
                    static_cast<uint32_t>(data[3]);
    
    // Bytes 4-11: 8 bytes of replacement data
    std::copy(data + 4, data + 12, patch.data.begin());
    
    // Default to enabled
    patch.enabled = true;
    
    return patch;
}

std::string Patch::toString() const {
    std::ostringstream oss;
    
    oss << "Patch ID=" << static_cast<int>(id)
        << " Addr=0x" << std::hex << std::setw(6) << std::setfill('0') << address
        << " Data=[";
    
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) oss << " ";
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    
    oss << "] Enabled=" << (enabled ? "true" : "false");
    
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
    
    // Data can be any 8 bytes (always valid)
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
