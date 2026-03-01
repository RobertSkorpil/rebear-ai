#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>

namespace rebear {

/**
 * @brief Represents a virtual patch configuration for the FPGA
 * 
 * A patch modifies data when the MCU reads from a specific Flash address.
 * The FPGA supports up to 16 simultaneous patches (IDs 0-15).
 * Each patch provides 8 bytes of replacement data.
 * 
 * Patches only trigger when the MCU initiates a read transaction starting
 * at the exact patch address. Offset reads do not trigger the patch.
 */
class Patch {
public:
    uint8_t id;                      ///< Patch ID (0-15)
    uint32_t address;                ///< 24-bit Flash address (stored as 32-bit)
    std::array<uint8_t, 8> data;     ///< 8 bytes of replacement data
    bool enabled;                    ///< Whether this patch is active
    
    /**
     * @brief Default constructor
     */
    Patch();
    
    /**
     * @brief Construct a patch with specified parameters
     * @param id Patch ID (0-15)
     * @param address 24-bit Flash address
     * @param data 8 bytes of replacement data
     * @param enabled Whether the patch is active (default: true)
     */
    Patch(uint8_t id, uint32_t address, const std::array<uint8_t, 8>& data, bool enabled = true);
    
    /**
     * @brief Serialize patch for SPI transmission
     * 
     * Format (12 bytes total, before escape encoding):
     * - Byte 0: Patch ID (0-15)
     * - Bytes 1-3: 24-bit address (big-endian)
     * - Bytes 4-11: 8 bytes of replacement data
     * 
     * @return Vector of 12 bytes ready for SPI transmission
     */
    std::vector<uint8_t> toBytes() const;
    
    /**
     * @brief Parse patch from byte array
     * 
     * Expects 12 bytes in the format produced by toBytes()
     * 
     * @param data Pointer to 12-byte array
     * @return Parsed Patch object
     * @throws std::invalid_argument if data is invalid
     */
    static Patch fromBytes(const uint8_t* data);
    
    /**
     * @brief Convert patch to human-readable string
     * 
     * Format: "Patch ID=0 Addr=0x001000 Data=[01 02 03 04 05 06 07 08] Enabled=true"
     * 
     * @return String representation
     */
    std::string toString() const;
    
    /**
     * @brief Validate patch data
     * 
     * Checks:
     * - ID is in range 0-15
     * - Address is < 0x1000000 (24-bit max)
     * - Data can be any 8 bytes (always valid)
     * 
     * @return true if patch is valid, false otherwise
     */
    bool isValid() const;
    
    /**
     * @brief Equality comparison
     */
    bool operator==(const Patch& other) const;
    
    /**
     * @brief Inequality comparison
     */
    bool operator!=(const Patch& other) const;
};

} // namespace rebear
