#ifndef REBEAR_SPI_PROTOCOL_NETWORK_H
#define REBEAR_SPI_PROTOCOL_NETWORK_H

#include "transaction.h"
#include "patch.h"
#include "network_client.h"
#include <memory>
#include <optional>
#include <string>

namespace rebear {

/**
 * @brief Network-based SPI Protocol implementation
 * 
 * Provides the same interface as SPIProtocol but communicates with
 * a remote rebear-server over TCP/IP instead of accessing hardware directly.
 */
class SPIProtocolNetwork {
public:
    /**
     * @brief Construct a network SPI protocol client
     * @param host Hostname or IP address of rebear-server
     * @param port Port number (default 9876)
     */
    SPIProtocolNetwork(const std::string& host, uint16_t port = 9876);
    
    /**
     * @brief Destructor
     */
    ~SPIProtocolNetwork();
    
    // Prevent copying
    SPIProtocolNetwork(const SPIProtocolNetwork&) = delete;
    SPIProtocolNetwork& operator=(const SPIProtocolNetwork&) = delete;
    
    /**
     * @brief Open SPI device on remote server
     * @param device SPI device path (e.g., "/dev/spidev0.0")
     * @param speed SPI clock speed in Hz (default 100 kHz)
     * @return true if successful, false otherwise
     */
    bool open(const std::string& device = "/dev/spidev0.0",
              uint32_t speed = 100000);
    
    /**
     * @brief Close SPI device on remote server
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
     * @brief Command 0x02: Upload multiple patches in a single buffer
     * 
     * Uploads a buffer containing multiple patch headers and data.
     * Each patch can have variable-length data (1 byte to ~16KB).
     * 
     * Buffer format:
     *   [PATCH_HEADER_0, PATCH_HEADER_1, ..., TERMINATOR, PATCH_DATA_0, PATCH_DATA_1, ...]
     * 
     * PATCH_HEADER (8 bytes):
     *   - STORED (1 byte): 0x80 if enabled, 0x00 if disabled
     *   - PATCH_ADDRESS (3 bytes, big-endian)
     *   - PATCH_LENGTH (2 bytes, big-endian): actual length of patch data
     *   - BUFFER_DATA (2 bytes, big-endian): offset of patch data in buffer
     * 
     * TERMINATOR: Single 0x00 byte
     * 
     * IMPORTANT: Hardware supports maximum 8 patch headers per buffer.
     * The 9th header position is reserved for the terminating header.
     * Maximum total buffer size is ~16KB.
     * 
     * @param patches Vector of patches to upload (max 8, variable data lengths)
     * @return true if successful, false otherwise
     */
    bool uploadPatchBuffer(const std::vector<Patch>& patches);
    
    /**
     * @brief Command 0x03: Dump patch buffer content from FPGA
     * 
     * Retrieves the current patch buffer content from the FPGA.
     * The response is prefixed with a 16-bit size value (big-endian).
     * 
     * @param buffer Output vector to receive the patch buffer content (without size prefix)
     * @return true if successful, false otherwise
     */
    bool dumpPatchBuffer(std::vector<uint8_t>& buffer);
    
    /**
     * @brief Clear all patches in FPGA
     * 
     * Clears patches by sending SPI command 0x02 with a single zero byte.
     * 
     * @return true if successful, false otherwise
     */
    bool clearPatches();
    
    /**
     * @brief Check if connected to remote server
     * @return true if connected
     */
    bool isConnected() const;
    
    /**
     * @brief Get last error message
     * @return Error description string
     */
    std::string getLastError() const;

private:
    std::unique_ptr<NetworkClient> client_;
    std::string lastError_;
    bool connected_;
    
    void setError(const std::string& error);
};

} // namespace rebear

#endif // REBEAR_SPI_PROTOCOL_NETWORK_H
