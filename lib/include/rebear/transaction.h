#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>

namespace rebear {

/**
 * @brief Represents a single Flash read transaction captured by the FPGA.
 * 
 * The FPGA monitors SPI communication between the MCU and Flash memory,
 * recording transaction details (address, byte count, timestamp) in an
 * internal buffer. This class represents one such transaction.
 * 
 * All multi-byte values from the FPGA are transmitted big-endian (MSB first).
 */
class Transaction {
public:
    /**
     * @brief 24-bit Flash memory address (stored as 32-bit).
     * 
     * Valid range: 0x000000 - 0xFFFFFF
     * Received from FPGA in big-endian format.
     */
    uint32_t address;
    
    /**
     * @brief 24-bit byte count (stored as 32-bit).
     * 
     * Number of bytes read in this transaction.
     * Received from FPGA in big-endian format.
     */
    uint32_t count;
    
    /**
     * @brief 16-bit timestamp in milliseconds.
     * 
     * Time since FPGA reset or last clear.
     * Range: 0-65535 ms (wraps around after ~65.5 seconds)
     * Received from FPGA in big-endian format.
     */
    uint16_t timestamp;
    
    /**
     * @brief System time when transaction was captured by host.
     * 
     * This is set by the host application when the transaction is read
     * from the FPGA, providing absolute timing information.
     */
    std::chrono::system_clock::time_point captureTime;
    
    /**
     * @brief Default constructor.
     * 
     * Initializes all fields to zero and captureTime to current time.
     */
    Transaction();
    
    /**
     * @brief Parse transaction from 8-byte FPGA response.
     * 
     * Expected format (all big-endian):
     *   Bytes 0-2: 24-bit address (MSB first)
     *   Bytes 3-5: 24-bit count (MSB first)
     *   Bytes 6-7: 16-bit timestamp (MSB first)
     * 
     * @param data Pointer to 8-byte buffer containing FPGA response
     * @return Parsed Transaction object
     * @throws std::invalid_argument if data is nullptr
     */
    static Transaction fromBytes(const uint8_t* data);
    
    /**
     * @brief Serialize transaction to bytes for export/storage.
     * 
     * Returns 8 bytes in the same format as received from FPGA:
     *   Bytes 0-2: 24-bit address (big-endian)
     *   Bytes 3-5: 24-bit count (big-endian)
     *   Bytes 6-7: 16-bit timestamp (big-endian)
     * 
     * Note: captureTime is not included in serialization.
     * 
     * @return 8-byte vector containing serialized transaction
     */
    std::vector<uint8_t> toBytes() const;
    
    /**
     * @brief Generate human-readable string representation.
     * 
     * Format: "Transaction(addr=0x001000, count=256, time=15ms)"
     * 
     * @return String representation of transaction
     */
    std::string toString() const;
    
    /**
     * @brief Validate transaction data.
     * 
     * Checks:
     * - Address is within 24-bit range (< 0x1000000)
     * - Count is reasonable (< 1MB = 1048576 bytes)
     * - Timestamp is within 16-bit range (< 65536)
     * 
     * @return true if transaction data is valid, false otherwise
     */
    bool isValid() const;
    
    /**
     * @brief Check if transaction appears to be dummy data.
     *
     * When the FPGA buffer is empty, reading may return all 0xFF bytes.
     * This method detects such dummy transactions.
     *
     * @return true if transaction appears to be dummy data (all 0xFF)
     */
    bool isDummy() const;
    
    /**
     * @brief Check if transaction was patched by the FPGA.
     *
     * When the FPGA actively patches a transaction, it does NOT count the
     * actual data being read. In this case, the count field will be 0xFFFFFF,
     * indicating that a patch was applied and the actual byte count is unknown.
     *
     * @return true if count is 0xFFFFFF (patch was applied)
     */
    bool isPatched() const;
};

} // namespace rebear
