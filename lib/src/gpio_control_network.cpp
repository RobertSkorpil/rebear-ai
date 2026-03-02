#include "rebear/gpio_control_network.h"
#include "rebear/protocol.h"

namespace rebear {

GPIOControlNetwork::GPIOControlNetwork(int pin, GPIOControl::Direction dir,
                                       const std::string& host, uint16_t port)
    : client_(std::make_unique<NetworkClient>(host, port))
    , pin_(pin)
    , direction_(dir)
    , is_open_(false)
    , current_value_(false)
{
}

GPIOControlNetwork::~GPIOControlNetwork() {
    close();
}

bool GPIOControlNetwork::init() {
    // Connect to server if not already connected
    if (!client_->isConnected()) {
        if (!client_->connect()) {
            setError("Failed to connect to server: " + client_->getLastError());
            return false;
        }
    }
    
    // Build request payload
    std::vector<uint8_t> payload;
    protocol::encodeByte(payload, static_cast<uint8_t>(pin_));
    protocol::encodeByte(payload, direction_ == GPIOControl::Direction::Input ? 0 : 1);
    
    // Send GPIO_INIT command
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::GPIO_INIT, payload, response)) {
        setError("GPIO init failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    if (!protocol::decodeByte(response, offset, success)) {
        setError("Invalid GPIO init response");
        return false;
    }
    
    if (!success) {
        std::string error_msg;
        if (protocol::decodeString(response, offset, error_msg)) {
            setError("GPIO init failed: " + error_msg);
        } else {
            setError("GPIO init failed");
        }
        return false;
    }
    
    is_open_ = true;
    return true;
}

void GPIOControlNetwork::close() {
    if (!is_open_) {
        return;
    }
    
    // Build request payload
    std::vector<uint8_t> payload;
    protocol::encodeByte(payload, static_cast<uint8_t>(pin_));
    
    // Send GPIO_CLOSE command
    std::vector<uint8_t> response;
    client_->sendRequest(protocol::CommandType::GPIO_CLOSE, payload, response);
    
    is_open_ = false;
}

bool GPIOControlNetwork::write(bool value) {
    if (!is_open_) {
        setError("GPIO not open");
        return false;
    }
    
    if (direction_ != GPIOControl::Direction::Output) {
        setError("Cannot write to input pin");
        return false;
    }
    
    // Build request payload
    std::vector<uint8_t> payload;
    protocol::encodeByte(payload, static_cast<uint8_t>(pin_));
    protocol::encodeByte(payload, value ? 1 : 0);
    
    // Send GPIO_WRITE command
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::GPIO_WRITE, payload, response)) {
        setError("GPIO write failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    if (!protocol::decodeByte(response, offset, success)) {
        setError("Invalid GPIO write response");
        return false;
    }
    
    if (success) {
        current_value_ = value;
        return true;
    }
    
    return false;
}

bool GPIOControlNetwork::read() const {
    return current_value_;
}

bool GPIOControlNetwork::readInput() const {
    if (!is_open_) {
        const_cast<GPIOControlNetwork*>(this)->setError("GPIO not open");
        return false;
    }
    
    if (direction_ != GPIOControl::Direction::Input) {
        const_cast<GPIOControlNetwork*>(this)->setError("Cannot read from output pin");
        return false;
    }
    
    // Build request payload
    std::vector<uint8_t> payload;
    protocol::encodeByte(payload, static_cast<uint8_t>(pin_));
    
    // Send GPIO_READ command
    std::vector<uint8_t> response;
    if (!client_->sendRequest(protocol::CommandType::GPIO_READ, payload, response)) {
        const_cast<GPIOControlNetwork*>(this)->setError("GPIO read failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    uint8_t value;
    if (!protocol::decodeByte(response, offset, success) ||
        !protocol::decodeByte(response, offset, value)) {
        const_cast<GPIOControlNetwork*>(this)->setError("Invalid GPIO read response");
        return false;
    }
    
    if (!success) {
        return false;
    }
    
    return value != 0;
}

bool GPIOControlNetwork::waitForEdge(int timeout_ms) {
    if (!is_open_) {
        setError("GPIO not open");
        return false;
    }
    
    if (direction_ != GPIOControl::Direction::Input) {
        setError("Cannot wait for edge on output pin");
        return false;
    }
    
    // Build request payload
    std::vector<uint8_t> payload;
    protocol::encodeByte(payload, static_cast<uint8_t>(pin_));
    protocol::encodeUint32(payload, static_cast<uint32_t>(timeout_ms));
    
    // Send GPIO_WAIT_EDGE command (with extended timeout)
    std::vector<uint8_t> response;
    int request_timeout = timeout_ms > 0 ? timeout_ms + 1000 : 60000;  // Add 1s buffer or use 60s for infinite
    if (!client_->sendRequest(protocol::CommandType::GPIO_WAIT_EDGE, payload, response, request_timeout)) {
        setError("GPIO wait edge failed: " + client_->getLastError());
        return false;
    }
    
    // Parse response
    size_t offset = 0;
    uint8_t success;
    uint8_t edge_detected;
    if (!protocol::decodeByte(response, offset, success) ||
        !protocol::decodeByte(response, offset, edge_detected)) {
        setError("Invalid GPIO wait edge response");
        return false;
    }
    
    if (!success) {
        return false;
    }
    
    return edge_detected != 0;
}

void GPIOControlNetwork::setError(const std::string& error) {
    lastError_ = error;
}

} // namespace rebear
