#include "rebear/spi_protocol.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace rebear {

// SPI command bytes
constexpr uint8_t CMD_CLEAR_TRANSACTIONS = 0x00;
constexpr uint8_t CMD_READ_TRANSACTION = 0x01;
constexpr uint8_t CMD_SET_PATCH = 0x02;
constexpr uint8_t CMD_DUMP_PATCH_BUFFER = 0x03;

// Hardware limitations
constexpr size_t MAX_PATCHES_PER_BUFFER = 8;  // FPGA supports up to 8 patch headers

// Expected response sizes (before escape decoding)
constexpr size_t TRANSACTION_RESPONSE_SIZE = 8;
constexpr size_t PATCH_DATA_SIZE = 12;

SPIProtocol::SPIProtocol() 
    : fd_(-1), speed_(100000) {
}

SPIProtocol::~SPIProtocol() {
    close();
}

bool SPIProtocol::open(const std::string& device, uint32_t speed) {
    if (isConnected()) {
        close();
    }
    
    // Validate speed (must not exceed 100 kHz)
    if (speed > 100000) {
        setError("Speed exceeds maximum 100 kHz (requested: " + std::to_string(speed) + " Hz)");
        return false;
    }
    
    speed_ = speed;
    
    // Open SPI device
    fd_ = ::open(device.c_str(), O_RDWR);
    if (fd_ < 0) {
        setError("Failed to open SPI device '" + device + "': " + std::strerror(errno));
        return false;
    }
    
    // Configure SPI mode (MODE 1: CPOL=0, CPHA=1)
    uint8_t mode = SPI_MODE_1;
    if (ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0) {
        setError("Failed to set SPI mode: " + std::string(std::strerror(errno)));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Configure bits per word (8 bits)
    uint8_t bits = 8;
    if (ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        setError("Failed to set bits per word: " + std::string(std::strerror(errno)));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Configure speed
    if (ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_) < 0) {
        setError("Failed to set SPI speed: " + std::string(std::strerror(errno)));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    lastError_.clear();
    return true;
}

void SPIProtocol::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SPIProtocol::clearTransactions() {
    if (!isConnected()) {
        setError("SPI device not connected");
        return false;
    }
    
    return sendCommand(CMD_CLEAR_TRANSACTIONS);
}

std::optional<Transaction> SPIProtocol::readTransaction() {
    if (!isConnected()) {
        setError("SPI device not connected");
        return std::nullopt;
    }
    
    std::vector<uint8_t> response;
    if (!sendCommandWithResponse(CMD_READ_TRANSACTION, TRANSACTION_RESPONSE_SIZE, response)) {
        return std::nullopt;
    }
    
    // Check if we got exactly 8 bytes
    if (response.size() != TRANSACTION_RESPONSE_SIZE) {
        setError("Invalid transaction response size: " + std::to_string(response.size()) + " (expected 8)");
        return std::nullopt;
    }
    
    // Parse transaction from bytes
    try {
        Transaction trans = Transaction::fromBytes(response.data());
        
        // Check for dummy/invalid transaction (all 0xFF indicates empty buffer)
        if (trans.address == 0xFFFFFF && trans.count == 0xFFFFFF && trans.timestamp == 0xFFFF) {
            // Buffer is empty, return nullopt without setting error
            return std::nullopt;
        }
        
        return trans;
    } catch (const std::exception& e) {
        setError("Failed to parse transaction: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool SPIProtocol::setPatch(const Patch& patch) {
    if (!isConnected()) {
        setError("SPI device not connected");
        return false;
    }
    
    // Validate patch
    if (!patch.isValid()) {
        setError("Invalid patch configuration");
        return false;
    }
    
    // Create a single-patch buffer and upload it
    std::vector<Patch> patches = {patch};
    return uploadPatchBuffer(patches);
}

bool SPIProtocol::uploadPatchBuffer(const std::vector<Patch>& patches) {
    if (!isConnected()) {
        setError("SPI device not connected");
        return false;
    }
    
    // Hardware limitation: maximum 8 patch headers
    if (patches.size() > MAX_PATCHES_PER_BUFFER) {
        setError("Too many patches: hardware supports maximum " + 
                 std::to_string(MAX_PATCHES_PER_BUFFER) + " patches per buffer (got " + 
                 std::to_string(patches.size()) + ")");
        return false;
    }
    
    // Validate all patches
    for (const auto& patch : patches) {
        if (!patch.isValid()) {
            setError("Invalid patch configuration in buffer");
            return false;
        }
    }
    
    // Build patch buffer:
    // CMD_SET_PATCH + [PATCH_HEADER_0, PATCH_HEADER_1, ..., TERMINATOR] + [PATCH_DATA_0, PATCH_DATA_1, ...]
    std::vector<uint8_t> buffer;
    
    // Command byte
    buffer.push_back(CMD_SET_PATCH);
    
    // Calculate data section start offset (relative to buffer start, AFTER command byte)
    // 8 bytes per header + 1 byte (terminator)
    size_t dataOffset = (patches.size() * 8) + 1;
    size_t currentDataOffset = dataOffset;
    
    // Write patch headers
    for (const auto& patch : patches) {
        // STORED (1 byte): 0x80 for enabled, 0x00 for disabled
        buffer.push_back(patch.enabled ? 0x80 : 0x00);
        
        // PATCH_ADDRESS (3 bytes, big-endian)
        buffer.push_back((patch.address >> 16) & 0xFF);
        buffer.push_back((patch.address >> 8) & 0xFF);
        buffer.push_back(patch.address & 0xFF);
        
        // PATCH_LENGTH (2 bytes, big-endian) - actual data length
        uint16_t length = static_cast<uint16_t>(patch.data.size());
        buffer.push_back((length >> 8) & 0xFF);
        buffer.push_back(length & 0xFF);
        
        // BUFFER_DATA offset (2 bytes, big-endian)
        // Offset is relative to the buffer structure (after command byte)
        buffer.push_back((currentDataOffset >> 8) & 0xFF);
        buffer.push_back(currentDataOffset & 0xFF);
        
        currentDataOffset += patch.data.size();
    }
    
    // Write terminating header (just the STORED byte = 0)
    buffer.push_back(0x00);
    
    // Write patch data
    for (const auto& patch : patches) {
        buffer.insert(buffer.end(), patch.data.begin(), patch.data.end());
    }
    
    // Encode and send
    auto encoded = encode(buffer);
    std::vector<uint8_t> dummy;
    
    return transfer(encoded, dummy, 0);
}

bool SPIProtocol::uploadPatchBufferVerbose(const std::vector<Patch>& patches, std::vector<uint8_t>& misoData) {
    if (!isConnected()) {
        setError("SPI device not connected");
        return false;
    }
    
    // Hardware limitation: maximum 8 patch headers
    if (patches.size() > MAX_PATCHES_PER_BUFFER) {
        setError("Too many patches: hardware supports maximum " + 
                 std::to_string(MAX_PATCHES_PER_BUFFER) + " patches per buffer (got " + 
                 std::to_string(patches.size()) + ")");
        return false;
    }
    
    // Validate all patches
    for (const auto& patch : patches) {
        if (!patch.isValid()) {
            setError("Invalid patch configuration in buffer");
            return false;
        }
    }
    
    // Build patch buffer (same as uploadPatchBuffer)
    std::vector<uint8_t> buffer;
    buffer.push_back(CMD_SET_PATCH);
    
    size_t dataOffset = (patches.size() * 8) + 1;
    size_t currentDataOffset = dataOffset;
    
    for (const auto& patch : patches) {
        buffer.push_back(patch.enabled ? 0x80 : 0x00);
        buffer.push_back((patch.address >> 16) & 0xFF);
        buffer.push_back((patch.address >> 8) & 0xFF);
        buffer.push_back(patch.address & 0xFF);
        
        uint16_t length = static_cast<uint16_t>(patch.data.size());
        buffer.push_back((length >> 8) & 0xFF);
        buffer.push_back(length & 0xFF);
        
        buffer.push_back((currentDataOffset >> 8) & 0xFF);
        buffer.push_back(currentDataOffset & 0xFF);
        
        currentDataOffset += patch.data.size();
    }
    
    buffer.push_back(0x00);
    
    for (const auto& patch : patches) {
        buffer.insert(buffer.end(), patch.data.begin(), patch.data.end());
    }
    
    // Encode
    auto encoded = encode(buffer);
    
    // Prepare to capture MISO data
    // We need a buffer at least as large as the encoded TX data
    size_t totalLen = encoded.size();
    std::vector<uint8_t> rxBuffer(totalLen);
    misoData.clear();
    
    struct spi_ioc_transfer xfer;
    std::memset(&xfer, 0, sizeof(xfer));
    
    xfer.tx_buf = reinterpret_cast<unsigned long>(encoded.data());
    xfer.rx_buf = reinterpret_cast<unsigned long>(rxBuffer.data());
    xfer.len = totalLen;
    xfer.speed_hz = speed_;
    xfer.bits_per_word = 8;
    xfer.delay_usecs = 0;
    
    if (ioctl(fd_, SPI_IOC_MESSAGE(1), &xfer) < 0) {
        setError("SPI transfer failed: " + std::string(std::strerror(errno)));
        return false;
    }
    
    // Return raw MISO data (before any decoding)
    misoData = rxBuffer;
    
    return true;
}

bool SPIProtocol::dumpPatchBuffer(std::vector<uint8_t>& buffer) {
    if (!isConnected()) {
        setError("SPI device not connected");
        return false;
    }
    
    // Send command 0x03 to dump patch buffer
    // First, we need to read the 16-bit size prefix (big-endian)
    std::vector<uint8_t> sizeResponse;
    if (!sendCommandWithResponse(CMD_DUMP_PATCH_BUFFER, 2, sizeResponse)) {
        return false;
    }
    
    // Check if we got exactly 2 bytes for the size
    if (sizeResponse.size() != 2) {
        setError("Invalid size response: expected 2 bytes, got " + std::to_string(sizeResponse.size()));
        return false;
    }
    
    // Parse size (big-endian)
    uint16_t bufferSize = (static_cast<uint16_t>(sizeResponse[0]) << 8) | sizeResponse[1];
    
    // If size is 0, buffer is empty
    if (bufferSize == 0) {
        buffer.clear();
        return true;
    }
    
    // Read the actual buffer content
    std::vector<uint8_t> contentResponse;
    
    // We need to continue reading from the SPI bus
    // Create a dummy TX buffer and read the content
    std::vector<uint8_t> dummyTx(bufferSize, 0xFF);
    auto encoded = encode(dummyTx);
    
    if (!transfer(encoded, contentResponse, bufferSize)) {
        return false;
    }
    
    // Check if we got the expected amount of data
    if (contentResponse.size() != bufferSize) {
        setError("Invalid buffer content size: expected " + std::to_string(bufferSize) + 
                 " bytes, got " + std::to_string(contentResponse.size()));
        return false;
    }
    
    buffer = contentResponse;
    return true;
}

bool SPIProtocol::clearPatches() {
    if (!isConnected()) {
        setError("SPI device not connected");
        return false;
    }
    
    // Clear patches by sending command 0x02 with a single zero byte
    std::vector<uint8_t> clearData = {CMD_SET_PATCH, 0x00};
    auto encoded = encode(clearData);
    std::vector<uint8_t> dummy;
    
    return transfer(encoded, dummy, 0);
}

bool SPIProtocol::isConnected() const {
    return fd_ >= 0;
}

std::string SPIProtocol::getLastError() const {
    return lastError_;
}

bool SPIProtocol::transfer(const std::vector<uint8_t>& tx,
                           std::vector<uint8_t>& rx,
                           size_t rxLen) {
    if (!isConnected()) {
        setError("SPI device not connected");
        return false;
    }
    
    // Prepare receive buffer
    // Need to account for:
    // 1. Dummy byte (first byte concurrent with command)
    // 2. Potential escape sequences (worst case 2x)
    // Total length = tx.size() + rxLen * 2
    size_t totalLen = tx.size() + (rxLen * 2);
    std::vector<uint8_t> rxBuffer(totalLen);
    
    struct spi_ioc_transfer transfer;
    std::memset(&transfer, 0, sizeof(transfer));
    
    transfer.tx_buf = reinterpret_cast<unsigned long>(tx.data());
    transfer.rx_buf = reinterpret_cast<unsigned long>(rxBuffer.data());
    transfer.len = totalLen;
    transfer.speed_hz = speed_;
    transfer.bits_per_word = 8;
    transfer.delay_usecs = 0;
    
    if (ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        setError("SPI transfer failed: " + std::string(std::strerror(errno)));
        return false;
    }
    
    // Decode received data if we expect a response
    if (rxLen > 0) {
        try {
            // Skip the first tx.size() bytes (dummy bytes concurrent with command transmission)
            // The actual response starts after the command bytes
            std::vector<uint8_t> encodedRx(rxBuffer.begin() + tx.size(), rxBuffer.end());
            rx = decode(encodedRx);
            
            // Trim to expected size if we got more
            if (rx.size() > rxLen) {
                rx.resize(rxLen);
            }
        } catch (const std::exception& e) {
            setError("Failed to decode response: " + std::string(e.what()));
            return false;
        }
    }
    
    return true;
}

bool SPIProtocol::sendCommand(uint8_t cmd) {
    std::vector<uint8_t> cmdData = {cmd};
    auto encoded = encode(cmdData);
    std::vector<uint8_t> dummy;
    
    return transfer(encoded, dummy, 0);
}

bool SPIProtocol::sendCommandWithResponse(uint8_t cmd, size_t rxLen, std::vector<uint8_t>& rx) {
    std::vector<uint8_t> cmdData = {cmd};
    auto encoded = encode(cmdData);
    
    return transfer(encoded, rx, rxLen);
}

void SPIProtocol::setError(const std::string& error) {
    lastError_ = error;
}

} // namespace rebear
