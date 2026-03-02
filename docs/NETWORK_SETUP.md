# Network Setup Guide

## Overview

The rebear system supports network virtualization, allowing the GUI and CLI applications to run on a different machine than the Raspberry Pi that's physically connected to the FPGA hardware.

## Architecture

```
┌─────────────────────┐         ┌─────────────────────┐
│  Desktop/Laptop     │         │  Raspberry Pi 3     │
│                     │         │                     │
│  rebear-gui/cli ────┼────────▶│  rebear-server      │
│  (Client)           │  TCP/IP │  (Hardware Access)  │
│                     │  :9876  │                     │
└─────────────────────┘         └─────────────────────┘
```

## Server Setup (Raspberry Pi)

### 1. Build the Server

```bash
cd /path/to/rebear
mkdir -p build
cd build
cmake -DBUILD_SERVER=ON ..
make rebear-server
```

### 2. Install the Server

```bash
sudo make install
```

This installs `rebear-server` to `/usr/local/bin/`.

### 3. Run the Server

**Manual Start:**
```bash
rebear-server
```

**With Options:**
```bash
rebear-server --port 9876 --max-clients 10
```

**Command-line Options:**
- `-p, --port PORT` - Port to listen on (default: 9876)
- `-m, --max-clients NUM` - Maximum concurrent clients (default: 10)
- `-h, --help` - Show help message

### 4. Verify Server is Running

From another terminal:
```bash
netstat -tuln | grep 9876
```

You should see the server listening on port 9876.

### 5. Configure Firewall (Optional)

If you have a firewall enabled, allow incoming connections on port 9876:

```bash
sudo ufw allow 9876/tcp
```

## Client Setup (Desktop/Laptop)

### 1. Build the Client Library

The network client is part of the rebear library:

```bash
cd /path/to/rebear
mkdir -p build
cd build
cmake ..
make
```

### 2. Using Network Mode in Code

#### SPI Protocol Example

```cpp
#include <rebear/spi_protocol_network.h>

int main() {
    // Create network SPI client
    rebear::SPIProtocolNetwork spi("raspberrypi.local", 9876);
    
    // Open SPI device (on remote server)
    if (!spi.open("/dev/spidev0.0", 100000)) {
        std::cerr << "Failed to open SPI: " << spi.getLastError() << std::endl;
        return 1;
    }
    
    // Read transactions
    while (true) {
        auto trans = spi.readTransaction();
        if (trans.has_value()) {
            std::cout << "Transaction: 0x" << std::hex << trans->address 
                      << " count=" << std::dec << (int)trans->count << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
```

#### GPIO Control Example

```cpp
#include <rebear/gpio_control_network.h>

int main() {
    // Create network GPIO client
    rebear::GPIOControlNetwork gpio(3, rebear::GPIOControl::Direction::Output,
                                    "raspberrypi.local", 9876);
    
    // Initialize GPIO (on remote server)
    if (!gpio.init()) {
        std::cerr << "Failed to init GPIO: " << gpio.getLastError() << std::endl;
        return 1;
    }
    
    // Toggle GPIO
    for (int i = 0; i < 10; i++) {
        gpio.write(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        gpio.write(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return 0;
}
```

### 3. Connection String Format

Network clients use the following connection format:

- **Hostname**: `raspberrypi.local` or IP address like `192.168.1.100`
- **Port**: Default is `9876`, can be customized

## Network Requirements

### Bandwidth
- Minimal: ~5 KB/s for typical transaction monitoring
- No special network configuration needed

### Latency
- Local network (LAN): 5-10 ms typical
- Acceptable for non-real-time monitoring
- Not suitable for real-time control applications

### Reliability
- TCP ensures reliable, ordered delivery
- Automatic reconnection on network failures
- Connection timeout: 5 seconds

## Troubleshooting

### Server Won't Start

**Problem:** `Failed to bind socket to port 9876`

**Solution:**
1. Check if another process is using the port:
   ```bash
   sudo netstat -tuln | grep 9876
   ```
2. Kill the process or use a different port:
   ```bash
   rebear-server --port 9877
   ```

### Client Can't Connect

**Problem:** `Failed to connect to server: Connection refused`

**Solutions:**
1. Verify server is running:
   ```bash
   ssh pi@raspberrypi.local
   ps aux | grep rebear-server
   ```

2. Check network connectivity:
   ```bash
   ping raspberrypi.local
   ```

3. Verify firewall settings:
   ```bash
   sudo ufw status
   ```

4. Try using IP address instead of hostname:
   ```cpp
   rebear::SPIProtocolNetwork spi("192.168.1.100", 9876);
   ```

### Connection Drops

**Problem:** Connection lost during operation

**Solutions:**
1. Check network stability
2. Verify server is still running
3. Client will automatically attempt reconnection
4. Check server logs for errors

### Slow Performance

**Problem:** High latency or slow response

**Solutions:**
1. Check network latency:
   ```bash
   ping raspberrypi.local
   ```
2. Verify no network congestion
3. Consider using wired connection instead of WiFi
4. Check server CPU usage:
   ```bash
   top
   ```

## Security Considerations

### Current Implementation
- **No encryption**: Data is sent in plain text
- **No authentication**: Anyone on network can connect
- **Local network only**: Not designed for internet exposure

### Recommendations

1. **Use on trusted networks only**
   - Home network
   - Private lab network
   - VPN

2. **Firewall configuration**
   ```bash
   # Allow only specific IP
   sudo ufw allow from 192.168.1.0/24 to any port 9876
   ```

3. **SSH tunneling for remote access**
   ```bash
   # On client machine
   ssh -L 9876:localhost:9876 pi@raspberrypi.local
   
   # Then connect to localhost
   rebear::SPIProtocolNetwork spi("localhost", 9876);
   ```

4. **Network isolation**
   - Use separate VLAN for hardware control
   - Isolate from internet-facing networks

## Advanced Configuration

### Running as System Service

Create `/etc/systemd/system/rebear-server.service`:

```ini
[Unit]
Description=Rebear Hardware Server
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/rebear-server
Restart=on-failure
User=pi
Group=spi,gpio

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable rebear-server
sudo systemctl start rebear-server
```

Check status:
```bash
sudo systemctl status rebear-server
```

View logs:
```bash
sudo journalctl -u rebear-server -f
```

### Multiple Servers

You can run multiple servers on different ports for different hardware:

```bash
# Server 1 - FPGA A
rebear-server --port 9876

# Server 2 - FPGA B
rebear-server --port 9877
```

## Performance Tuning

### Server-Side
- Increase max clients if needed: `--max-clients 20`
- Monitor CPU usage: `top`
- Check memory usage: `free -h`

### Client-Side
- Use connection pooling for multiple operations
- Batch operations when possible
- Implement local caching for frequently accessed data

### Network
- Use wired Ethernet for best performance
- Minimize network hops
- Consider dedicated network interface for hardware control

## Testing

### Basic Connectivity Test

```bash
# On client machine
telnet raspberrypi.local 9876
```

If connection succeeds, you'll see a connection message. Press Ctrl+] then type `quit` to exit.

### Protocol Test

Use the provided test utilities (when available):

```bash
# Test SPI protocol
./test_spi_protocol_network

# Test GPIO control
./test_gpio_control_network
```

## Support

For issues or questions:
1. Check server logs
2. Verify network connectivity
3. Review this documentation
4. Check GitHub issues

## Future Enhancements

Planned improvements:
- SSL/TLS encryption
- Authentication tokens
- mDNS service discovery
- Configuration file support
- Web-based monitoring interface
