#ifndef REBEAR_COMMAND_HANDLER_H
#define REBEAR_COMMAND_HANDLER_H

#include <rebear/protocol.h>
#include <rebear/spi_protocol.h>
#include <rebear/gpio_control.h>
#include <memory>
#include <map>
#include <mutex>

namespace rebear {
namespace server {

/**
 * @brief Handles command execution on the server side
 * 
 * Routes incoming commands to appropriate hardware interfaces
 * and manages hardware resource lifecycle.
 */
class CommandHandler {
public:
    CommandHandler();
    ~CommandHandler();
    
    /**
     * @brief Process a command and generate response
     * @param request Incoming request message
     * @param response Outgoing response message
     * @return true if command was handled successfully
     */
    bool handleCommand(const protocol::Message& request, protocol::Message& response);
    
private:
    // Hardware interfaces
    std::unique_ptr<SPIProtocol> spi_;
    std::map<int, std::unique_ptr<GPIOControl>> gpio_pins_;
    
    // Thread safety
    std::mutex spi_mutex_;
    std::mutex gpio_mutex_;
    
    // Command handlers
    bool handleSpiOpen(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleSpiClose(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleSpiClearTransactions(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleSpiReadTransaction(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleSpiSetPatch(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleSpiClearPatches(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    
    bool handleGpioInit(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleGpioClose(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleGpioWrite(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleGpioRead(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    bool handleGpioWaitEdge(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
    
    bool handlePing(const std::vector<uint8_t>& payload, std::vector<uint8_t>& response);
};

} // namespace server
} // namespace rebear

#endif // REBEAR_COMMAND_HANDLER_H
