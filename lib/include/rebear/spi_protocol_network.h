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
     * @brief Command 0x03: Clear all patches in FPGA
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
