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

bool SPIProtocolNetwork::uploadPatchBuffer(const std::vector<Patch>& patches) {
    if (!connected_) {
        setError("Not connected");
        return false;
    }
    
    // Hardware limitation: maximum 8 patch headers
    if (patches.size() > 8) {
        setError("Too many patches: hardware supports maximum 8 patches per buffer (got " + 
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
    
    // Build patch buffer (same format as local SPI protocol)
    std::vector<uint8_t> payload;
    
    // Calculate data section start offset (relative to buffer start, NOT including command)
    // 8 bytes per header + 1 byte terminator
    size_t dataOffset = (patches.size() * 8) + 1;
    size_t currentDataOffset = dataOffset;
    
    // Write patch headers
    for (const auto& patch : patches) {
        // STORED (1 byte): 0x80 for enabled, 0x00 for disabled
        protocol::encodeByte(payload, patch.enabled ? 0x80 : 0x00);
        
        // PATCH_ADDRESS (3 bytes, big-endian)
        protocol::encodeByte(payload, (patch.address >> 16) & 0xFF);
        protocol::encodeByte(payload, (patch.address >> 8) & 0xFF);
        protocol::encodeByte(payload, patch.address & 0xFF);
        
        // PATCH_LENGTH (2 bytes, big-endian) - actual data length
        uint16_t length = static_cast<uint16_t>(patch.data.size());
        protocol::encodeByte(payload, (length >> 8) & 0xFF);
        protocol::encodeByte(payload, length & 0xFF);
        
        // BUFFER_DATA offset (2 bytes, big-endian)
        // Offset is relative to the buffer structure (not including command byte)
        protocol::encodeByte(payload, (currentDataOffset >> 8) & 0xFF);
        protocol::encodeByte(payload, currentDataOffset & 0xFF);
        
        currentDataOffset += patch.data.size();
    }
    
    // Write terminating header (just the STORED byte = 0)
    protocol::encodeByte(payload, 0x00);
    
    // Write patch data
    for (const auto& patch : patches) {
        for (size_t i = 0; i < patch.data.size(); i++) {
            protocol::encodeByte(payload, patch.data[i]);
        }
    }
    
    // Send SPI_SET_PATCH command with buffer
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::SPI_SET_PATCH, payload, response)) {
        setError("Upload patch buffer failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    if (!protocol::decodeByte(response, offset, success)) {
        setError("Invalid upload patch buffer response");
        return false;
    }
    
    if (!success) {
        // Read error message if present
        std::string errorMsg;
        if (protocol::decodeString(response, offset, errorMsg)) {
            setError("Upload patch buffer failed: " + errorMsg);
        } else {
            setError("Upload patch buffer failed");
        }
        return false;
    }
    
    return true;
}

bool SPIProtocolNetwork::dumpPatchBuffer(std::vector<uint8_t>& buffer) {
    if (!connected_) {
        setError("Not connected");
        return false;
    }
    
    // Send SPI_DUMP_PATCH_BUFFER command
    std::vector<uint8_t> payload;
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::SPI_DUMP_PATCH_BUFFER, payload, response)) {
        setError("Dump patch buffer failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    if (!protocol::decodeByte(response, offset, success)) {
        setError("Invalid dump patch buffer response");
        return false;
    }
    
    if (!success) {
        setError("Dump patch buffer failed on server");
        return false;
    }
    
    // Read buffer size (16-bit, big-endian)
    uint16_t bufferSize;
    if (!protocol::decodeUint16(response, offset, bufferSize)) {
        setError("Invalid buffer size in response");
        return false;
    }
    
    // Read buffer content
    buffer.clear();
    for (uint16_t i = 0; i < bufferSize; i++) {
        uint8_t byte;
        if (!protocol::decodeByte(response, offset, byte)) {
            setError("Incomplete buffer content in response");
            return false;
        }
        buffer.push_back(byte);
    }
    
    return true;
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
