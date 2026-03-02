#include "rebear/spi_protocol_network.h"
#include "rebear/protocol.h"

namespace rebear {

SPIProtocolNetwork::SPIProtocolNetwork(const std::string& host, uint16_t port)
    : client_(std::make_unique<NetworkClient>(host, port))
    , connected_(false)
{
}

SPIProtocolNetwork::~SPIProtocolNetwork() {
    close();
}

bool SPIProtocolNetwork::open(const std::string& device, uint32_t speed) {
    // Connect to server if not already connected
    if (!client_->isConnected()) {
        if (!client_->connect()) {
            setError("Failed to connect to server: " + client_->getLastError());
            return false;
        }
    }
    
    // Build request payload
    std::vector<uint8_t> payload;
    protocol::encodeString(payload, device);
    protocol::encodeUint32(payload, speed);
    
    // Send SPI_OPEN command
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::SPI_OPEN, payload, response)) {
        setError("SPI open failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    if (!protocol::decodeByte(response, offset, success)) {
        setError("Invalid SPI open response");
        return false;
    }
    
    if (!success) {
        std::string error_msg;
        if (protocol::decodeString(response, offset, error_msg)) {
            setError("SPI open failed: " + error_msg);
        } else {
            setError("SPI open failed");
        }
        return false;
    }
    
    connected_ = true;
    return true;
}

void SPIProtocolNetwork::close() {
    if (!connected_) {
        return;
    }
    
    // Send SPI_CLOSE command
    std::vector<uint8_t> payload;
    std::vector<uint8_t> response;
    client_->sendRequest(protocol::CommandType::SPI_CLOSE, payload, response);
    
    connected_ = false;
}

bool SPIProtocolNetwork::clearTransactions() {
    if (!connected_) {
        setError("Not connected");
        return false;
    }
    
    // Send SPI_CLEAR_TRANSACTIONS command
    std::vector<uint8_t> payload;
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::SPI_CLEAR_TRANSACTIONS, payload, response)) {
        setError("Clear transactions failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    if (!protocol::decodeByte(response, offset, success)) {
        setError("Invalid clear transactions response");
        return false;
    }
    
    return success != 0;
}

std::optional<Transaction> SPIProtocolNetwork::readTransaction() {
    if (!connected_) {
        setError("Not connected");
        return std::nullopt;
    }
    
    // Send SPI_READ_TRANSACTION command
    std::vector<uint8_t> payload;
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::SPI_READ_TRANSACTION, payload, response)) {
        setError("Read transaction failed: " + client_->getLastError());
        return std::nullopt;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t has_data;
    if (!protocol::decodeByte(response, offset, has_data)) {
        setError("Invalid read transaction response");
        return std::nullopt;
    }
    
    if (!has_data) {
        return std::nullopt;  // No transaction available
    }
    
    // Decode transaction data (address, count, timestamp)
    uint32_t address;
    uint32_t count;
    uint16_t timestamp;
    
    if (!protocol::decodeUint32(response, offset, address) ||
        !protocol::decodeUint32(response, offset, count) ||
        !protocol::decodeUint16(response, offset, timestamp)) {
        setError("Invalid transaction data");
        return std::nullopt;
    }
    
    // Create transaction and set fields
    Transaction trans;
    trans.address = address;
    trans.count = count;
    trans.timestamp = timestamp;
    return trans;
}

bool SPIProtocolNetwork::setPatch(const Patch& patch) {
    if (!connected_) {
        setError("Not connected");
        return false;
    }
    
    // Build request payload
    std::vector<uint8_t> payload;
    protocol::encodeByte(payload, patch.id);
    protocol::encodeUint32(payload, patch.address);
    
    // Encode replacement data (8 bytes)
    for (int i = 0; i < 8; i++) {
        protocol::encodeByte(payload, patch.data[i]);
    }
    
    protocol::encodeByte(payload, patch.enabled ? 1 : 0);
    
    // Send SPI_SET_PATCH command
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::SPI_SET_PATCH, payload, response)) {
        setError("Set patch failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    if (!protocol::decodeByte(response, offset, success)) {
        setError("Invalid set patch response");
        return false;
    }
    
    return success != 0;
}

bool SPIProtocolNetwork::clearPatches() {
    if (!connected_) {
        setError("Not connected");
        return false;
    }
    
    // Send SPI_CLEAR_PATCHES command
    std::vector<uint8_t> payload;
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::SPI_CLEAR_PATCHES, payload, response)) {
        setError("Clear patches failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    if (!protocol::decodeByte(response, offset, success)) {
        setError("Invalid clear patches response");
        return false;
    }
    
    return success != 0;
}

bool SPIProtocolNetwork::isConnected() const {
    return connected_ && client_->isConnected();
}

std::string SPIProtocolNetwork::getLastError() const {
    return lastError_;
}

void SPIProtocolNetwork::setError(const std::string& error) {
    lastError_ = error;
}

} // namespace rebear
