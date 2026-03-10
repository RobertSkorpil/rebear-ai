#include "command_handler.h"
#include <rebear/patch.h>
#include <rebear/transaction.h>
#include <iostream>

namespace rebear {
namespace server {

CommandHandler::CommandHandler()
    : spi_(std::make_unique<SPIProtocol>())
{
}

CommandHandler::~CommandHandler() {
    // Close all GPIO pins
    std::lock_guard<std::mutex> lock(gpio_mutex_);
    gpio_pins_.clear();
}

bool CommandHandler::handleCommand(const protocol::Message& request, protocol::Message& response) {
    using namespace protocol;
    
    switch (request.type) {
        // Control commands
        case CommandType::PING:
            return handlePing(request.payload, response.payload);
            
        // SPI commands
        case CommandType::SPI_OPEN:
            response.type = CommandType::SPI_OPEN_RESPONSE;
            return handleSpiOpen(request.payload, response.payload);
            
        case CommandType::SPI_CLOSE:
            response.type = CommandType::SPI_CLOSE_RESPONSE;
            return handleSpiClose(request.payload, response.payload);
            
        case CommandType::SPI_CLEAR_TRANSACTIONS:
            response.type = CommandType::SPI_CLEAR_TRANSACTIONS_RESPONSE;
            return handleSpiClearTransactions(request.payload, response.payload);
            
        case CommandType::SPI_READ_TRANSACTION:
            response.type = CommandType::SPI_READ_TRANSACTION_RESPONSE;
            return handleSpiReadTransaction(request.payload, response.payload);
            
        case CommandType::SPI_SET_PATCH:
            response.type = CommandType::SPI_SET_PATCH_RESPONSE;
            return handleSpiSetPatch(request.payload, response.payload);
            
        case CommandType::SPI_DUMP_PATCH_BUFFER:
            response.type = CommandType::SPI_DUMP_PATCH_BUFFER_RESPONSE;
            return handleSpiDumpPatchBuffer(request.payload, response.payload);
            
        case CommandType::SPI_CLEAR_PATCHES:
            response.type = CommandType::SPI_CLEAR_PATCHES_RESPONSE;
            return handleSpiClearPatches(request.payload, response.payload);
            
        // GPIO commands
        case CommandType::GPIO_INIT:
            response.type = CommandType::GPIO_INIT_RESPONSE;
            return handleGpioInit(request.payload, response.payload);
            
        case CommandType::GPIO_CLOSE:
            response.type = CommandType::GPIO_CLOSE_RESPONSE;
            return handleGpioClose(request.payload, response.payload);
            
        case CommandType::GPIO_WRITE:
            response.type = CommandType::GPIO_WRITE_RESPONSE;
            return handleGpioWrite(request.payload, response.payload);
            
        case CommandType::GPIO_READ:
            response.type = CommandType::GPIO_READ_RESPONSE;
            return handleGpioRead(request.payload, response.payload);
            
        case CommandType::GPIO_WAIT_EDGE:
            response.type = CommandType::GPIO_WAIT_EDGE_RESPONSE;
            return handleGpioWaitEdge(request.payload, response.payload);
            
        default:
            response.type = CommandType::ERROR;
            protocol::encodeByte(response.payload, ErrorCode::INVALID_COMMAND);
            protocol::encodeString(response.payload, "Unknown command type");
            return false;
    }
}

bool CommandHandler::handleSpiOpen(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    
    // Parse payload
    size_t offset = 0;
    std::string device;
    uint32_t speed;
    
    if (!protocol::decodeString(payload, offset, device) ||
        !protocol::decodeUint32(payload, offset, speed)) {
        protocol::encodeByte(response, 0);  // failure
        protocol::encodeString(response, "Invalid payload");
        return false;
    }
    
    // Open SPI
    if (!spi_->open(device, speed)) {
        protocol::encodeByte(response, 0);  // failure
        protocol::encodeString(response, spi_->getLastError());
        return false;
    }
    
    protocol::encodeByte(response, 1);  // success
    return true;
}

bool CommandHandler::handleSpiClose(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    
    spi_->close();
    protocol::encodeByte(response, 1);  // success
    return true;
}

bool CommandHandler::handleSpiClearTransactions(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    
    bool success = spi_->clearTransactions();
    protocol::encodeByte(response, success ? 1 : 0);
    return success;
}

bool CommandHandler::handleSpiReadTransaction(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    
    auto trans = spi_->readTransaction();
    
    if (trans.has_value()) {
        protocol::encodeByte(response, 1);  // has data
        protocol::encodeUint32(response, trans->address);
        protocol::encodeUint32(response, trans->count);
        protocol::encodeUint16(response, trans->timestamp);
    } else {
        protocol::encodeByte(response, 0);  // no data
    }
    
    return true;
}

bool CommandHandler::handleSpiSetPatch(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    
    if (payload.empty()) {
        protocol::encodeByte(response, 0);  // failure
        return false;
    }
    
    // Detect format based on payload structure
    // Old format: id(1) + address(4) + data(8) + enabled(1) = 14 bytes minimum
    // New format: headers + terminator + data (varies, minimum 9 bytes for 1 patch)
    
    size_t offset = 0;
    
    // Check if this is the new buffer format by examining the first byte
    // In new format, first byte is STORED (0x80 or 0x00)
    // In old format, first byte is patch ID (0-15)
    
    // Try to detect new format: first byte is 0x80 or 0x00, and we have at least 9 bytes
    bool isNewFormat = (payload.size() >= 9) && 
                       (payload[0] == 0x80 || payload[0] == 0x00);
    
    // Additional check: if old format, we should have exactly 14 bytes
    if (payload.size() == 14 && payload[0] <= 15) {
        isNewFormat = false;
    }
    
    if (isNewFormat) {
        // New buffer format - parse multiple patches
        std::vector<Patch> patches;
        
        // Parse headers
        while (offset < payload.size()) {
            uint8_t stored;
            if (!protocol::decodeByte(payload, offset, stored)) {
                protocol::encodeByte(response, 0);  // failure
                return false;
            }
            
            // Check for terminator
            if (stored == 0x00) {
                // Could be terminator or disabled patch
                // Terminator is just a single 0x00 byte
                // If we have more bytes, check if it's followed by patch headers
                if (offset >= payload.size() || offset == 1) {
                    // This is the terminator
                    break;
                }
                // Otherwise, it's a disabled patch, continue parsing
            }
            
            // Parse patch header
            uint32_t address = 0;
            uint16_t length = 0;
            uint16_t dataOffset = 0;
            
            // PATCH_ADDRESS (3 bytes, big-endian)
            uint8_t addr_high, addr_mid, addr_low;
            if (!protocol::decodeByte(payload, offset, addr_high) ||
                !protocol::decodeByte(payload, offset, addr_mid) ||
                !protocol::decodeByte(payload, offset, addr_low)) {
                protocol::encodeByte(response, 0);  // failure
                return false;
            }
            address = (static_cast<uint32_t>(addr_high) << 16) |
                      (static_cast<uint32_t>(addr_mid) << 8) |
                      static_cast<uint32_t>(addr_low);
            
            // PATCH_LENGTH (2 bytes, big-endian)
            if (!protocol::decodeUint16(payload, offset, length)) {
                protocol::encodeByte(response, 0);  // failure
                return false;
            }
            
            // BUFFER_DATA (2 bytes, big-endian)
            if (!protocol::decodeUint16(payload, offset, dataOffset)) {
                protocol::encodeByte(response, 0);  // failure
                return false;
            }
            
            // Store patch info
            Patch patch;
            patch.id = patches.size();  // Assign sequential IDs
            patch.address = address;
            patch.enabled = (stored == 0x80);
            
            // Read patch data from offset
            if (dataOffset + length > payload.size()) {
                protocol::encodeByte(response, 0);  // failure
                return false;
            }
            
            // Copy patch data
            patch.data.assign(payload.begin() + dataOffset, 
                            payload.begin() + dataOffset + length);
            
            patches.push_back(patch);
        }
        
        // Apply all patches using buffer upload
        if (!spi_->uploadPatchBuffer(patches)) {
            protocol::encodeByte(response, 0);  // failure
            protocol::encodeString(response, spi_->getLastError());
            return false;
        }
        
        protocol::encodeByte(response, 1);  // success
        return true;
        
    } else {
        // Old format - single patch
        uint8_t id;
        uint32_t address;
        std::array<uint8_t, 8> data;
        uint8_t enabled;
        
        if (!protocol::decodeByte(payload, offset, id) ||
            !protocol::decodeUint32(payload, offset, address) ||
            !protocol::decodeByte(payload, offset, data[0]) ||
            !protocol::decodeByte(payload, offset, data[1]) ||
            !protocol::decodeByte(payload, offset, data[2]) ||
            !protocol::decodeByte(payload, offset, data[3]) ||
            !protocol::decodeByte(payload, offset, data[4]) ||
            !protocol::decodeByte(payload, offset, data[5]) ||
            !protocol::decodeByte(payload, offset, data[6]) ||
            !protocol::decodeByte(payload, offset, data[7]) ||
            !protocol::decodeByte(payload, offset, enabled)) {
            protocol::encodeByte(response, 0);  // failure
            return false;
        }
        
        // Create patch
        Patch patch(id, address, data, enabled != 0);
        
        // Set patch
        bool success = spi_->setPatch(patch);
        protocol::encodeByte(response, success ? 1 : 0);
        return success;
    }
}

bool CommandHandler::handleSpiDumpPatchBuffer(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    
    std::vector<uint8_t> buffer;
    bool success = spi_->dumpPatchBuffer(buffer);
    
    if (!success) {
        protocol::encodeByte(response, 0);  // failure
        return false;
    }
    
    protocol::encodeByte(response, 1);  // success
    
    // Encode buffer size (16-bit, big-endian)
    uint16_t bufferSize = static_cast<uint16_t>(buffer.size());
    protocol::encodeUint16(response, bufferSize);
    
    // Encode buffer content
    for (uint8_t byte : buffer) {
        protocol::encodeByte(response, byte);
    }
    
    return true;
}

bool CommandHandler::handleSpiClearPatches(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    
    bool success = spi_->clearPatches();
    protocol::encodeByte(response, success ? 1 : 0);
    return success;
}

bool CommandHandler::handleGpioInit(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(gpio_mutex_);
    
    // Parse payload
    size_t offset = 0;
    uint8_t pin;
    uint8_t direction;
    
    if (!protocol::decodeByte(payload, offset, pin) ||
        !protocol::decodeByte(payload, offset, direction)) {
        protocol::encodeByte(response, 0);  // failure
        protocol::encodeString(response, "Invalid payload");
        return false;
    }
    
    // Create GPIO control
    GPIOControl::Direction dir = (direction == 0) ? GPIOControl::Direction::Input : GPIOControl::Direction::Output;
    auto gpio = std::make_unique<GPIOControl>(pin, dir);
    
    if (!gpio->init()) {
        protocol::encodeByte(response, 0);  // failure
        protocol::encodeString(response, gpio->getLastError());
        return false;
    }
    
    // Store GPIO control
    gpio_pins_[pin] = std::move(gpio);
    
    protocol::encodeByte(response, 1);  // success
    return true;
}

bool CommandHandler::handleGpioClose(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(gpio_mutex_);
    
    // Parse payload
    size_t offset = 0;
    uint8_t pin;
    
    if (!protocol::decodeByte(payload, offset, pin)) {
        protocol::encodeByte(response, 0);  // failure
        return false;
    }
    
    // Remove GPIO control
    gpio_pins_.erase(pin);
    
    protocol::encodeByte(response, 1);  // success
    return true;
}

bool CommandHandler::handleGpioWrite(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(gpio_mutex_);
    
    // Parse payload
    size_t offset = 0;
    uint8_t pin;
    uint8_t value;
    
    if (!protocol::decodeByte(payload, offset, pin) ||
        !protocol::decodeByte(payload, offset, value)) {
        protocol::encodeByte(response, 0);  // failure
        return false;
    }
    
    // Find GPIO control
    auto it = gpio_pins_.find(pin);
    if (it == gpio_pins_.end()) {
        protocol::encodeByte(response, 0);  // failure
        return false;
    }
    
    // Write value
    bool success = it->second->write(value != 0);
    protocol::encodeByte(response, success ? 1 : 0);
    return success;
}

bool CommandHandler::handleGpioRead(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    std::lock_guard<std::mutex> lock(gpio_mutex_);
    
    // Parse payload
    size_t offset = 0;
    uint8_t pin;
    
    if (!protocol::decodeByte(payload, offset, pin)) {
        protocol::encodeByte(response, 0);  // failure
        protocol::encodeByte(response, 0);  // value
        return false;
    }
    
    // Find GPIO control
    auto it = gpio_pins_.find(pin);
    if (it == gpio_pins_.end()) {
        protocol::encodeByte(response, 0);  // failure
        protocol::encodeByte(response, 0);  // value
        return false;
    }
    
    // Read value
    bool value = it->second->readInput();
    protocol::encodeByte(response, 1);  // success
    protocol::encodeByte(response, value ? 1 : 0);
    return true;
}

bool CommandHandler::handleGpioWaitEdge(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    // Note: We don't lock for the entire duration since waitForEdge can block
    
    // Parse payload
    size_t offset = 0;
    uint8_t pin;
    uint32_t timeout_ms;
    
    if (!protocol::decodeByte(payload, offset, pin) ||
        !protocol::decodeUint32(payload, offset, timeout_ms)) {
        protocol::encodeByte(response, 0);  // failure
        protocol::encodeByte(response, 0);  // edge detected
        return false;
    }
    
    // Find GPIO control (with lock)
    GPIOControl* gpio = nullptr;
    {
        std::lock_guard<std::mutex> lock(gpio_mutex_);
        auto it = gpio_pins_.find(pin);
        if (it == gpio_pins_.end()) {
            protocol::encodeByte(response, 0);  // failure
            protocol::encodeByte(response, 0);  // edge detected
            return false;
        }
        gpio = it->second.get();
    }
    
    // Wait for edge (without lock - this can block)
    bool edge_detected = gpio->waitForEdge(timeout_ms);
    
    protocol::encodeByte(response, 1);  // success
    protocol::encodeByte(response, edge_detected ? 1 : 0);
    return true;
}

bool CommandHandler::handlePing(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response) {
    // Just respond with PONG
    return true;
}

} // namespace server
} // namespace rebear
