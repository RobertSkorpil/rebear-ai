# Phase 1.8: Network Virtualization for SPI and GPIO

## Overview

This phase adds network virtualization to enable running the GUI application on a different machine than the Raspberry Pi that's physically connected to the FPGA. The virtualization layer provides transparent access to SPI and GPIO operations over TCP/IP.

## Architecture

### High-Level Design

```
┌─────────────────────────────────────┐
│  Remote Machine (GUI/CLI)           │
│  ┌───────────────────────────────┐  │
│  │  Application (rebear-gui)     │  │
│  └───────────────┬───────────────┘  │
│                  │                   │
│  ┌───────────────▼───────────────┐  │
│  │  librebear (client mode)      │  │
│  │  - SPIProtocol (network)      │  │
│  │  - GPIOControl (network)      │  │
│  └───────────────┬───────────────┘  │
│                  │                   │
│  ┌───────────────▼───────────────┐  │
│  │  Network Client               │  │
│  │  (TCP socket)                 │  │
│  └───────────────┬───────────────┘  │
└──────────────────┼───────────────────┘
                   │ TCP/IP
                   │ (Port 9876)
┌──────────────────▼───────────────────┐
│  Raspberry Pi 3                      │
│  ┌───────────────────────────────┐  │
│  │  rebear-server (daemon)       │  │
│  │  - Network Server             │  │
│  │  - Command Router             │  │
│  └───────────────┬───────────────┘  │
│                  │                   │
│  ┌───────────────▼───────────────┐  │
│  │  librebear (local mode)       │  │
│  │  - SPIProtocol (hardware)     │  │
│  │  - GPIOControl (hardware)     │  │
│  └───────────────┬───────────────┘  │
│                  │                   │
│  ┌───────────────▼───────────────┐  │
│  │  Hardware (SPI/GPIO)          │  │
│  └───────────────────────────────┘  │
└──────────────────────────────────────┘
```

### Key Components

1. **Server Daemon** (`rebear-server`) - Runs on Raspberry Pi
   - Listens for network connections
   - Routes commands to local hardware
   - Streams transaction data to clients
   - Manages multiple client connections

2. **Client Library** - Integrated into `librebear`
   - Transparent network communication
   - Same API as local hardware access
   - Automatic reconnection on network failures
   - Connection pooling and multiplexing

3. **Abstraction Layer** - Interface for local/remote selection
   - Factory pattern for creating SPI/GPIO objects
   - Runtime selection of local vs. network mode
   - Configuration file support

## Network Protocol

### Protocol Choice: TCP

**Rationale:**
- Reliable, ordered delivery (critical for command sequences)
- Built-in flow control
- Simpler error handling than UDP
- Adequate performance for this use case (not latency-critical)
- No SSL needed (local network only)

### Message Format

All messages use a simple binary protocol with length prefix:

```
┌──────────────┬──────────────┬──────────────┬──────────────┐
│ Magic (2B)   │ Length (2B)  │ Type (1B)    │ Payload (N)  │
├──────────────┼──────────────┼──────────────┼──────────────┤
│ 0x52 0x42    │ Big-endian   │ Command ID   │ Variable     │
│ ("RB")       │ uint16_t     │              │              │
└──────────────┴──────────────┴──────────────┴──────────────┘
```

**Magic Bytes:** `0x52 0x42` ("RB" for Rebear)
**Length:** Total message length including header (minimum 5 bytes)
**Type:** Command/response type identifier

### Command Types

#### SPI Commands (0x10-0x1F)

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x10 | SPI_OPEN | Client→Server | device_path (string), speed (4B) |
| 0x11 | SPI_OPEN_RESPONSE | Server→Client | success (1B), error_msg (string) |
| 0x12 | SPI_CLOSE | Client→Server | None |
| 0x13 | SPI_CLOSE_RESPONSE | Server→Client | success (1B) |
| 0x14 | SPI_CLEAR_TRANSACTIONS | Client→Server | None |
| 0x15 | SPI_CLEAR_TRANSACTIONS_RESPONSE | Server→Client | success (1B) |
| 0x16 | SPI_READ_TRANSACTION | Client→Server | None |
| 0x17 | SPI_READ_TRANSACTION_RESPONSE | Server→Client | has_data (1B), transaction (8B) |
| 0x18 | SPI_SET_PATCH | Client→Server | patch_data (12B) |
| 0x19 | SPI_SET_PATCH_RESPONSE | Server→Client | success (1B) |
| 0x1A | SPI_CLEAR_PATCHES | Client→Server | None |
| 0x1B | SPI_CLEAR_PATCHES_RESPONSE | Server→Client | success (1B) |

#### GPIO Commands (0x20-0x2F)

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x20 | GPIO_INIT | Client→Server | pin (1B), direction (1B) |
| 0x21 | GPIO_INIT_RESPONSE | Server→Client | success (1B), error_msg (string) |
| 0x22 | GPIO_CLOSE | Client→Server | pin (1B) |
| 0x23 | GPIO_CLOSE_RESPONSE | Server→Client | success (1B) |
| 0x24 | GPIO_WRITE | Client→Server | pin (1B), value (1B) |
| 0x25 | GPIO_WRITE_RESPONSE | Server→Client | success (1B) |
| 0x26 | GPIO_READ | Client→Server | pin (1B) |
| 0x27 | GPIO_READ_RESPONSE | Server→Client | success (1B), value (1B) |
| 0x28 | GPIO_WAIT_EDGE | Client→Server | pin (1B), timeout_ms (4B) |
| 0x29 | GPIO_WAIT_EDGE_RESPONSE | Server→Client | success (1B), edge_detected (1B) |

#### Control Commands (0x00-0x0F)

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x00 | PING | Client→Server | None |
| 0x01 | PONG | Server→Client | None |
| 0x02 | ERROR | Server→Client | error_code (1B), error_msg (string) |
| 0x03 | DISCONNECT | Both | None |

### String Encoding

Strings are encoded as:
```
┌──────────────┬──────────────┐
│ Length (2B)  │ UTF-8 Data   │
├──────────────┼──────────────┤
│ Big-endian   │ N bytes      │
│ uint16_t     │              │
└──────────────┴──────────────┘
```

### Example Message Flow

**SPI Open:**
```
Client → Server:
  Magic: 0x52 0x42
  Length: 0x00 0x14 (20 bytes total)
  Type: 0x10 (SPI_OPEN)
  Payload:
    Device path length: 0x00 0x0F (15 bytes)
    Device path: "/dev/spidev0.0"
    Speed: 0x00 0x01 0x86 0xA0 (100000 Hz)

Server → Client:
  Magic: 0x52 0x42
  Length: 0x00 0x06 (6 bytes total)
  Type: 0x11 (SPI_OPEN_RESPONSE)
  Payload:
    Success: 0x01 (true)
```

**Read Transaction:**
```
Client → Server:
  Magic: 0x52 0x42
  Length: 0x00 0x05 (5 bytes - header only)
  Type: 0x16 (SPI_READ_TRANSACTION)
  Payload: (none)

Server → Client:
  Magic: 0x52 0x42
  Length: 0x00 0x0E (14 bytes total)
  Type: 0x17 (SPI_READ_TRANSACTION_RESPONSE)
  Payload:
    Has data: 0x01 (true)
    Transaction: 8 bytes (address, count, timestamp)
```

## Implementation Details

### Server Daemon (`rebear-server`)

**Files:**
- `server/main.cpp` - Entry point, daemon setup
- `server/network_server.h/cpp` - TCP server implementation
- `server/command_handler.h/cpp` - Command routing and execution
- `server/client_session.h/cpp` - Per-client connection management

**Key Features:**
- Multi-threaded: One thread per client connection
- Graceful shutdown on SIGTERM/SIGINT
- Logging to syslog
- Configuration file support (`/etc/rebear/server.conf`)
- Systemd service integration
- Connection limits (max 10 concurrent clients)

**Configuration Options:**
```ini
[network]
port = 9876
bind_address = 0.0.0.0
max_clients = 10

[hardware]
spi_device = /dev/spidev0.0
spi_speed = 100000
gpio_button_pin = 3
gpio_buffer_ready_pin = 4

[logging]
level = info
syslog = true
```

**Systemd Service:**
```ini
[Unit]
Description=Rebear Hardware Server
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/rebear-server
Restart=on-failure
User=rebear
Group=spi,gpio

[Install]
WantedBy=multi-user.target
```

### Client Library Integration

**Files:**
- `lib/include/rebear/network_client.h` - Network client interface
- `lib/src/network_client.cpp` - TCP client implementation
- `lib/include/rebear/spi_protocol_network.h` - Network-based SPI
- `lib/src/spi_protocol_network.cpp` - Implementation
- `lib/include/rebear/gpio_control_network.h` - Network-based GPIO
- `lib/src/gpio_control_network.cpp` - Implementation

**Abstraction Layer:**

```cpp
namespace rebear {

// Factory for creating SPI protocol objects
class SPIProtocolFactory {
public:
    enum class Mode {
        Local,    // Direct hardware access
        Network   // Network virtualization
    };
    
    static std::unique_ptr<SPIProtocol> create(
        Mode mode,
        const std::string& connection_string = ""
    );
};

// Factory for creating GPIO control objects
class GPIOControlFactory {
public:
    enum class Mode {
        Local,    // Direct hardware access
        Network   // Network virtualization
    };
    
    static std::unique_ptr<GPIOControl> create(
        int pin,
        GPIOControl::Direction dir,
        Mode mode,
        const std::string& connection_string = ""
    );
};

} // namespace rebear
```

**Connection String Format:**
- Local mode: Empty string or "local"
- Network mode: "tcp://hostname:port" (e.g., "tcp://192.168.1.100:9876")

**Usage Example:**

```cpp
// Local mode (direct hardware)
auto spi = SPIProtocolFactory::create(
    SPIProtocolFactory::Mode::Local
);

// Network mode (remote hardware)
auto spi = SPIProtocolFactory::create(
    SPIProtocolFactory::Mode::Network,
    "tcp://raspberrypi.local:9876"
);

// Both have identical API
spi->open("/dev/spidev0.0", 100000);
auto trans = spi->readTransaction();
```

### Network Client Implementation

**Key Features:**
- Automatic reconnection with exponential backoff
- Connection timeout (5 seconds default)
- Keep-alive pings (every 30 seconds)
- Thread-safe operation
- Request/response correlation
- Error propagation to caller

**Connection Management:**

```cpp
class NetworkClient {
public:
    NetworkClient(const std::string& host, uint16_t port);
    ~NetworkClient();
    
    bool connect(int timeout_ms = 5000);
    void disconnect();
    bool isConnected() const;
    
    // Send request and wait for response
    bool sendRequest(uint8_t type, const std::vector<uint8_t>& payload,
                     std::vector<uint8_t>& response, int timeout_ms = 1000);
    
    // Async notification callback
    void setNotificationCallback(std::function<void(uint8_t, const std::vector<uint8_t>&)> cb);
    
private:
    int socket_fd_;
    std::string host_;
    uint16_t port_;
    std::mutex mutex_;
    std::thread recv_thread_;
    std::atomic<bool> running_;
    
    void receiveLoop();
    bool sendMessage(uint8_t type, const std::vector<uint8_t>& payload);
    bool receiveMessage(uint8_t& type, std::vector<uint8_t>& payload, int timeout_ms);
};
```

### Error Handling

**Network Errors:**
- Connection refused → Retry with backoff
- Connection timeout → Return error to caller
- Connection lost → Attempt reconnection
- Protocol error → Log and disconnect

**Hardware Errors:**
- SPI/GPIO errors propagated from server
- Error messages included in response
- Client receives same error as local mode

### Performance Considerations

**Latency:**
- Local SPI operation: ~1-2 ms
- Network operation: ~5-10 ms (LAN)
- Acceptable for this use case (not real-time critical)

**Throughput:**
- Transaction monitoring: ~100 Hz polling rate
- Network overhead: ~50 bytes per transaction
- Bandwidth: ~5 KB/s (negligible)

**Optimization:**
- Batch transaction reads (read multiple in one request)
- Connection pooling (reuse connections)
- Binary protocol (minimal overhead)

## Security Considerations

### Network Security

**No SSL/TLS:**
- Designed for local network only
- Adding SSL would complicate deployment
- Users can use SSH tunneling if needed

**Access Control:**
- Server binds to specific interface (configurable)
- Firewall rules recommended
- No authentication (trust local network)

**Future Enhancement:**
- Optional authentication token
- IP whitelist
- SSL/TLS support

### Hardware Safety

**Same as Local Mode:**
- SPI speed validation (max 100 kHz)
- GPIO direction enforcement
- Patch validation

**Additional Safeguards:**
- Server validates all commands
- Rate limiting on commands
- Connection limits

## Testing Strategy

### Unit Tests

**Network Protocol:**
- Message encoding/decoding
- String serialization
- Error handling

**Client Library:**
- Connection management
- Request/response correlation
- Reconnection logic

**Server:**
- Command routing
- Multi-client handling
- Resource cleanup

### Integration Tests

**Local Loopback:**
- Server and client on same machine
- Verify all commands work
- Test error conditions

**Network Testing:**
- Server on Pi, client on different machine
- Measure latency
- Test connection loss/recovery
- Verify concurrent clients

### Hardware Testing

**Full System:**
- GUI on remote machine
- Server on Pi with FPGA
- Verify all functionality works
- Compare with local mode

## Migration Path

### Backward Compatibility

**Existing Code:**
- No changes required for local mode
- Factory pattern allows gradual migration
- Configuration file selects mode

**CLI Tool:**
```bash
# Local mode (default)
rebear-cli monitor

# Network mode
rebear-cli --remote tcp://raspberrypi.local:9876 monitor
```

**GUI Application:**
- Connection dialog with mode selection
- Remember last connection
- Auto-detect local hardware

### Deployment

**Server Installation:**
```bash
# Build server
cd build
cmake -DBUILD_SERVER=ON ..
make rebear-server

# Install
sudo make install

# Enable systemd service
sudo systemctl enable rebear-server
sudo systemctl start rebear-server
```

**Client Configuration:**
```bash
# Create config file
mkdir -p ~/.config/rebear
cat > ~/.config/rebear/client.conf << EOF
[connection]
mode = network
host = raspberrypi.local
port = 9876
EOF
```

## Documentation Updates

### User Documentation

**New Sections:**
- Network setup guide
- Server installation
- Client configuration
- Troubleshooting network issues

**Updated Sections:**
- Installation (add server component)
- Configuration (add network options)
- CLI usage (add --remote flag)
- GUI usage (add connection dialog)

### Developer Documentation

**New Sections:**
- Network protocol specification
- Client library API
- Server architecture
- Adding new commands

## Implementation Phases

### Phase 1.8.1: Protocol Design
- Define message format
- Document all commands
- Create protocol specification

### Phase 1.8.2: Network Client Library
- Implement NetworkClient class
- Add message encoding/decoding
- Write unit tests

### Phase 1.8.3: Server Daemon
- Implement TCP server
- Add command routing
- Multi-client support

### Phase 1.8.4: SPI/GPIO Network Wrappers
- SPIProtocolNetwork class
- GPIOControlNetwork class
- Factory pattern integration

### Phase 1.8.5: Integration and Testing
- Integration tests
- Hardware testing
- Performance benchmarking

### Phase 1.8.6: CLI/GUI Updates
- Add network mode support
- Connection dialogs
- Configuration management

### Phase 1.8.7: Documentation
- User guide updates
- Developer documentation
- Example configurations

## Success Criteria

- [ ] Server daemon runs reliably on Pi
- [ ] Client library provides transparent network access
- [ ] All SPI commands work over network
- [ ] All GPIO commands work over network
- [ ] GUI runs successfully on remote machine
- [ ] Latency < 20ms for typical operations
- [ ] No data loss during network operations
- [ ] Graceful handling of network failures
- [ ] Documentation complete and clear
- [ ] Backward compatibility maintained

## Future Enhancements

### Phase 2: Advanced Features
- Transaction streaming (push model)
- Batch operations
- Compression for large data transfers
- WebSocket support for web GUI

### Phase 3: Security
- Optional SSL/TLS
- Authentication tokens
- IP whitelisting
- Audit logging

### Phase 4: Discovery
- mDNS/Avahi service discovery
- Auto-detect Pi on network
- Zero-configuration setup

## Files to Create

### Server
- `server/CMakeLists.txt`
- `server/main.cpp`
- `server/network_server.h`
- `server/network_server.cpp`
- `server/command_handler.h`
- `server/command_handler.cpp`
- `server/client_session.h`
- `server/client_session.cpp`
- `server/protocol.h` (shared with client)
- `server/rebear-server.service` (systemd)
- `server/rebear-server.conf.example`

### Client Library
- `lib/include/rebear/network_client.h`
- `lib/src/network_client.cpp`
- `lib/include/rebear/spi_protocol_network.h`
- `lib/src/spi_protocol_network.cpp`
- `lib/include/rebear/gpio_control_network.h`
- `lib/src/gpio_control_network.cpp`
- `lib/include/rebear/protocol.h` (shared with server)
- `lib/include/rebear/factory.h`
- `lib/src/factory.cpp`

### Tests
- `lib/test_network_client.cpp`
- `lib/test_spi_protocol_network.cpp`
- `lib/test_gpio_control_network.cpp`
- `server/test_server.cpp`

### Documentation
- `docs/NETWORK_SETUP.md`
- `docs/NETWORK_PROTOCOL.md`
- `docs/SERVER_ADMIN.md`

## Conclusion

Phase 1.8 adds critical network virtualization capability, enabling the GUI to run on any machine on the local network. The design prioritizes:

1. **Transparency** - Same API for local and network modes
2. **Reliability** - TCP with automatic reconnection
3. **Simplicity** - Binary protocol, no complex dependencies
4. **Performance** - Adequate for non-real-time use case
5. **Maintainability** - Clean separation of concerns

This enhancement significantly improves the usability of the system, allowing comfortable GUI operation on a desktop/laptop while the Pi remains connected to the hardware.
