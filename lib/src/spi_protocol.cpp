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
constexpr uint8_t CMD_CLEAR_PATCHES = 0x03;

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
    
    // Build patch data: command + ID + address (3 bytes) + data (8 bytes)
    std::vector<uint8_t> patchData;
    patchData.reserve(13);
    
    patchData.push_back(CMD_SET_PATCH);
    patchData.push_back(patch.id);
    
    // Address (24-bit, big-endian)
    patchData.push_back((patch.address >> 16) & 0xFF);
    patchData.push_back((patch.address >> 8) & 0xFF);
    patchData.push_back(patch.address & 0xFF);
    
    // Data (8 bytes)
    patchData.insert(patchData.end(), patch.data.begin(), patch.data.end());
    
    // Encode and send
    auto encoded = encode(patchData);
    std::vector<uint8_t> dummy;
    
    return transfer(encoded, dummy, 0);
}

bool SPIProtocol::clearPatches() {
    if (!isConnected()) {
        setError("SPI device not connected");
        return false;
    }
    
    return sendCommand(CMD_CLEAR_PATCHES);
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
