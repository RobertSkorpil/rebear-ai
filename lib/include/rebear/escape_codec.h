#ifndef REBEAR_ESCAPE_CODEC_H
#define REBEAR_ESCAPE_CODEC_H

#include <vector>
#include <cstdint>

namespace rebear {

// Altera/Avalon SPI IP Core Escape Sequence Constants
constexpr uint8_t ESCAPE_CHAR = 0x4d;
constexpr uint8_t IDLE_CHAR = 0x4a;
constexpr uint8_t XOR_MASK = 0x20;

/**
 * Check if a byte needs escaping for Avalon SPI transmission.
 * 
 * @param byte The byte to check
 * @return true if the byte needs escaping (0x4a or 0x4d)
 */
bool needsEscape(uint8_t byte);

/**
 * Encode data for transmission to FPGA using Avalon escape sequences.
 * 
 * Encoding rules:
 * - 0x4a → 0x4d 0x6a (escape + (0x4a XOR 0x20))
 * - 0x4d → 0x4d 0x6d (escape + (0x4d XOR 0x20))
 * - Other bytes → unchanged
 * 
 * @param data The raw data to encode
 * @return Encoded data ready for SPI transmission
 */
std::vector<uint8_t> encode(const std::vector<uint8_t>& data);

/**
 * Decode data received from FPGA using Avalon escape sequences.
 * 
 * Decoding rules:
 * - 0x4d followed by X → (X XOR 0x20)
 * - Other bytes → unchanged
 * 
 * @param data The encoded data received from FPGA
 * @return Decoded raw data
 */
std::vector<uint8_t> decode(const std::vector<uint8_t>& data);

} // namespace rebear

#endif // REBEAR_ESCAPE_CODEC_H
