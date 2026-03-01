#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include "transaction.h"
#include "patch.h"
#include "escape_codec.h"

namespace rebear {

/**
 * @brief SPI Protocol handler for FPGA communication
 * 
 * Manages SPI communication with the FPGA that monitors Flash memory access.
 * Handles escape encoding/decoding and implements the command protocol.
 * 
 * CRITICAL REQUIREMENTS:
 * - SPI MODE 1 (CPOL=0, CPHA=1)
 * - Maximum speed: 100 kHz (100000 Hz)
 * - All data must be escape encoded/decoded
 */
class SPIProtocol {
public:
    SPIProtocol();
    ~SPIProtocol();
    
    // Prevent copying
    SPIProtocol(const SPIProtocol&) = delete;
    SPIProtocol& operator=(const SPIProtocol&) = delete;
    
    /**
     * @brief Open SPI device
     * @param device SPI device path (e.g., "/dev/spidev0.0")
     * @param speed SPI clock speed in Hz (default 100 kHz, REQUIRED for FPGA)
     * @return true if successful, false otherwise
     */
    bool open(const std::string& device = "/dev/spidev0.0",
              uint32_t speed = 100000);
    
    /**
     * @brief Close SPI device
     */
    void close();
    
    /**
     * @brief Command 0x00: Clear transaction buffer
     * @return true if successful, false otherwise
     */
    bool clearTransactions();
    
    /**
     * @brief Command 0x01: Read next transaction from FPGA buffer
     * @return Transaction object if available, std::nullopt if buffer empty or error
     */
    std::optional<Transaction> readTransaction();
    
    /**
     * @brief Command 0x02: Set a patch in FPGA
     * @param patch Patch configuration to apply
     * @return true if successful, false otherwise
     */
    bool setPatch(const Patch& patch);
    
    /**
     * @brief Command 0x03: Clear all patches in FPGA
     * @return true if successful, false otherwise
     */
    bool clearPatches();
    
    /**
     * @brief Check if SPI device is connected
     * @return true if device is open and ready
     */
    bool isConnected() const;
    
    /**
     * @brief Get last error message
     * @return Error description string
     */
    std::string getLastError() const;
    
private:
    int fd_;                    ///< File descriptor for SPI device
    std::string lastError_;     ///< Last error message
    uint32_t speed_;            ///< SPI clock speed in Hz
    
    /**
     * @brief Low-level SPI transfer
     * @param tx Data to transmit (already escape encoded)
     * @param rx Buffer to receive data (will be escape decoded)
     * @param rxLen Expected number of bytes to receive (before decoding)
     * @return true if successful, false otherwise
     */
    bool transfer(const std::vector<uint8_t>& tx, 
                  std::vector<uint8_t>& rx, 
                  size_t rxLen);
    
    /**
     * @brief Send command byte only (no response expected)
     * @param cmd Command byte
     * @return true if successful, false otherwise
     */
    bool sendCommand(uint8_t cmd);
    
    /**
     * @brief Send command and receive response
     * @param cmd Command byte
     * @param rxLen Expected response length (before escape decoding)
     * @param rx Decoded response data
     * @return true if successful, false otherwise
     */
    bool sendCommandWithResponse(uint8_t cmd, size_t rxLen, std::vector<uint8_t>& rx);
    
    /**
     * @brief Set error message
     * @param error Error description
     */
    void setError(const std::string& error);
};

} // namespace rebear
