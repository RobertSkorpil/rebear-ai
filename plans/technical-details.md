# Technical Details: SPI Protocol and Implementation

## Overview

This document provides in-depth technical details about the SPI communication protocol between the Raspberry Pi host and the FPGA that monitors the teddy bear's Flash memory access, as well as the GPIO interfaces for button control and buffer status monitoring.

## Hardware Context

### Signal Flow

```
Teddy Bear MCU ←SPI→ Flash Memory
                      ↑
                      | (Passive tap + active patch injection)
                      |
                    FPGA
                      ↑
                      | (SPI slave interface)
                      |
                Raspberry Pi 3 (SPI master)
```

### FPGA Capabilities

The FPGA performs three main functions:

1. **Passive Monitoring**: Records all SPI transactions between MCU and Flash
2. **Active Patching**: Injects modified data when specific addresses are accessed
3. **Host Communication**: Reports transaction logs and receives patch configurations via SPI

## Altera/Avalon SPI IP Core Escape Sequences

### The 0x4a Problem

The Altera/Avalon SPI IP core used in the FPGA treats byte value `0x4a` as a special "idle" character. This means:
- `0x4a` in the data stream is interpreted as padding/idle, not actual data
- To transmit the actual byte value `0x4a`, it must be escaped

### Escape Mechanism

**Escape Character**: `0x4d`
**XOR Mask**: `0x20`

**Encoding Rules**:
1. If byte == `0x4a`: transmit `0x4d 0x6a` (escape + (0x4a XOR 0x20))
2. If byte == `0x4d`: transmit `0x4d 0x6d` (escape + (0x4d XOR 0x20))
3. Otherwise: transmit byte as-is

**Decoding Rules**:
1. If byte == `0x4d`: read next byte, XOR with `0x20`, use result
2. Otherwise: use byte as-is

### Examples

| Original Data | Encoded Data | Explanation |
|--------------|--------------|-------------|
| `0x01` | `0x01` | No escape needed |
| `0x4a` | `0x4d 0x6a` | 0x4a → escape + (0x4a XOR 0x20) |
| `0x4d` | `0x4d 0x6d` | 0x4d → escape + (0x4d XOR 0x20) |
| `0x01 0x4a 0x02` | `0x01 0x4d 0x6a 0x02` | Middle byte escaped |
| `0x4a 0x4d` | `0x4d 0x6a 0x4d 0x6d` | Both bytes escaped |

### Implementation Considerations

- **Encoding**: Must be applied to ALL data sent from Pi to FPGA
- **Decoding**: Must be applied to ALL data received from FPGA
- **Buffer Size**: Encoded data can be up to 2x original size (worst case: all bytes are 0x4a or 0x4d)
- **Performance**: Minimal overhead for typical data (most bytes don't need escaping)

## SPI Command Protocol

### Command Structure

All commands start with a single command byte from the master (Raspberry Pi):

| Command Byte | Name | Direction | Additional Data |
|-------------|------|-----------|-----------------|
| `0x00` | Clear Transactions | Master → FPGA | None |
| `0x01` | Read Transaction | Master → FPGA | None (FPGA responds with 8 bytes) |
| `0x02` | Set Patch | Master → FPGA | 12 bytes |
| `0x03` | Clear Patches | Master → FPGA | None |

### Command 0x00: Clear Transactions

**Purpose**: Clears the FPGA's transaction buffer.

**Master Sends**:
```
Byte 0: 0x00
```

**FPGA Response**: None (or acknowledgment if implemented)

**Use Case**: Clear old transaction data before starting a new monitoring session.

### Command 0x01: Read Transaction

**Purpose**: Retrieves the next recorded transaction from FPGA's buffer.

**Master Sends**:
```
Byte 0: 0x01
```

**FPGA Responds** (8 bytes, after escape decoding):
```
Bytes 0-2: 24-bit Flash address (big-endian, MSB first)
Bytes 3-5: 24-bit byte count (big-endian, MSB first)
Bytes 6-7: 16-bit timestamp in milliseconds (big-endian, MSB first)
```

**CRITICAL - SPI Full-Duplex Behavior**:
- In SPI full-duplex mode, data is exchanged simultaneously in both directions
- When the master sends the command byte (0x01), it receives a dummy byte concurrently
- The FPGA doesn't know what command is being sent until it receives it
- Therefore, the first byte from the slave is always dummy data
- **The actual 8-byte response starts AFTER the command byte transmission**
- Software must skip the first received byte and read the next 8 bytes

**CRITICAL - Patch Active Behavior**:
- When the FPGA is actively patching a transaction, it does NOT count the actual data
- In this case, the count field will be `0xFF 0xFF 0xFF` (0xFFFFFF)
- This indicates that a patch was applied and the actual byte count is unknown
- Software should detect this special value and handle patched transactions appropriately

**Note**: All multi-byte values are transmitted **big-endian** (most significant byte first).

**Example (Normal Transaction)**:
```
Raw FPGA response: 0x00 0x10 0x00 0x00 0x01 0x00 0x00 0x0F
Decoded:
  Address: 0x001000 (4096 decimal)
  Count:   0x000100 (256 bytes)
  Timestamp: 0x000F (15 ms)
```

**Example (Patched Transaction)**:
```
Raw FPGA response: 0x00 0x10 0x00 0xFF 0xFF 0xFF 0x00 0x0F
Decoded:
  Address: 0x001000 (4096 decimal)
  Count:   0xFFFFFF (special value - patch was applied)
  Timestamp: 0x000F (15 ms)
```

**Buffer Behavior**:
- FPGA maintains a circular buffer of transactions
- Each read advances to the next transaction
- If buffer is empty, behavior is implementation-defined (likely returns zeros or stale data)

### Command 0x02: Set Patch

**Purpose**: Configures a virtual patch that modifies data when a specific address is accessed.

**Master Sends** (12 bytes total, after escape encoding):
```
Byte 0:    0x02 (command)
Byte 1:    Patch ID (0-15)
Bytes 2-4: 24-bit patch address (big-endian)
Bytes 5-12: 8 bytes of replacement data
```

**Example**:
```
Command: 0x02
Patch ID: 0x00
Address: 0x001000
Data: 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08

Encoded transmission (before escape encoding):
0x02 0x00 0x00 0x10 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08
```

**Patch Behavior**:
- Patch is stored in FPGA with given ID (0-15)
- When MCU reads from Flash starting at patch address, FPGA injects the 8 replacement bytes
- **Critical**: Patch only triggers if transaction START address matches patch address exactly
- If MCU reads from address+1, patch is NOT applied
- Maximum 16 simultaneous patches (IDs 0-15)
- Setting a patch with existing ID overwrites the previous patch

### Command 0x03: Clear Patches

**Purpose**: Removes all active patches, restoring normal Flash passthrough.

**Master Sends**:
```
Byte 0: 0x03
```

**FPGA Response**: None

**Effect**: All 16 patch slots are cleared; subsequent Flash reads return unmodified data.

## Patch Triggering Logic

### Address Matching

The FPGA compares the START address of each MCU→Flash read transaction against all active patch addresses.

**Scenario 1: Exact Match**
```
Patch: Address 0x001000, Data: [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08]
MCU reads 16 bytes starting at 0x001000
Result: First 8 bytes are patched, remaining 8 bytes are original Flash data
```

**Scenario 2: Offset Read (No Match)**
```
Patch: Address 0x001000
MCU reads 16 bytes starting at 0x001001
Result: All 16 bytes are original Flash data (patch NOT applied)
```

**Scenario 3: Multiple Patches**
```
Patch 0: Address 0x001000
Patch 1: Address 0x002000
MCU reads from 0x001000 → Patch 0 applied
MCU reads from 0x002000 → Patch 1 applied
MCU reads from 0x003000 → No patch applied
```

### Patch Data Length

- Each patch provides exactly 8 bytes of replacement data
- If MCU reads more than 8 bytes, only the first 8 are patched
- If MCU reads fewer than 8 bytes, only the requested bytes are patched

**Example**:
```
Patch: Address 0x001000, Data: [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22]
MCU reads 4 bytes from 0x001000
Result: MCU receives [0xAA, 0xBB, 0xCC, 0xDD]
```

## Transaction Timestamp

### Timestamp Format

- **16-bit unsigned integer** (big-endian, MSB first)
- Resolution: 1 millisecond
- Range: 0-65,535 ms (approximately 65.5 seconds)
- Wraps around after 65,535 ms

### Timestamp Behavior

- Timestamp represents time since FPGA reset or last clear
- Useful for analyzing timing patterns in MCU behavior
- Can identify periodic access patterns (e.g., polling loops)
- Limited to ~65 seconds before wraparound

### Timestamp Wraparound Handling

Application software should detect wraparound:
```cpp
uint16_t prev_timestamp = 65530;
uint16_t curr_timestamp = 5;  // Wrapped around

// Detect wraparound
if (curr_timestamp < prev_timestamp) {
    // Wraparound occurred
    uint32_t delta = (65536 - prev_timestamp) + curr_timestamp;
    // delta = 11 ms
}
```

## SPI Bus Configuration

### Recommended Settings for Raspberry Pi

```cpp
// SPI device: /dev/spidev0.0 or /dev/spidev0.1
// Mode: SPI_MODE_1 (CPOL=0, CPHA=1) - REQUIRED for FPGA
// Speed: 100 kHz (100000 Hz) - REQUIRED, do not exceed
// Bits per word: 8
// LSB first: No (MSB first)
```

**CRITICAL**: The FPGA requires SPI MODE 1 and a maximum clock frequency of 100 kHz. Higher speeds will cause communication errors.

### Speed Considerations

- **100 kHz**: REQUIRED - Maximum safe speed for FPGA communication
- **Lower speeds**: Acceptable (e.g., 50 kHz) but slower
- **Higher speeds**: NOT SUPPORTED - Will cause errors

### Timing Constraints

- Allow settling time between command byte and data bytes
- FPGA may need time to prepare response data
- Recommended: 1-10 μs delay between command and data read

## Error Handling

### SPI Communication Errors

**Possible Issues**:
1. Device not found (`/dev/spidev*` doesn't exist)
2. Permission denied (user not in `spi` group)
3. Bus contention (another process using SPI)
4. Hardware failure (loose connection, power issue)

**Detection**:
- Check return values from `open()`, `ioctl()`, `read()`, `write()`
- Verify escape decoding produces valid data
- Validate transaction data (address ranges, reasonable counts)

**Recovery**:
- Retry with exponential backoff
- Reset FPGA (if reset line available)
- Notify user and allow manual intervention

### Data Validation

**Transaction Data**:
- Address should be within Flash size (e.g., 0x000000 - 0xFFFFFF for 16MB)
- Count should be reasonable (typically < 4KB per transaction)
- Timestamp should increment monotonically (except wraparound)

**Patch Data**:
- Patch ID must be 0-15
- Address must be valid Flash address
- Data can be any 8 bytes (no validation needed)

## Performance Characteristics

### Transaction Buffer Size

- FPGA buffer size is implementation-dependent (likely 256-1024 transactions)
- High MCU activity may overflow buffer
- Application should poll frequently to avoid data loss

### Recommended Polling Rate

- **Real-time monitoring**: 10-100 Hz (every 10-100 ms)
- **Background logging**: 1-10 Hz (every 100-1000 ms)
- **Burst mode**: Poll until buffer empty, then wait

### Throughput Estimation

At 1 MHz SPI speed:
- Command byte: ~8 μs
- Read transaction (8 bytes): ~64 μs
- Total per transaction: ~72 μs
- Maximum rate: ~13,800 transactions/second

In practice, with escape encoding and processing overhead:
- Realistic rate: ~5,000-10,000 transactions/second

## Security and Safety

### Access Control

- SPI device requires appropriate permissions
- Add user to `spi` group: `sudo usermod -a -G spi username`
- Or use `sudo` (not recommended for production)

### Data Integrity

- Escape codec ensures data integrity
- No checksums in protocol (rely on SPI hardware reliability)
- Application should validate received data

### Fail-Safe Behavior

- If Pi crashes, FPGA continues monitoring
- Patches remain active until explicitly cleared or FPGA reset
- Transaction buffer may overflow if not read

## Debugging Tips

### Verify Escape Codec

Test with known problematic sequences:
```cpp
std::vector<uint8_t> test = {0x4a, 0x4d, 0x00, 0xFF};
auto encoded = codec.encode(test);
auto decoded = codec.decode(encoded);
assert(decoded == test);
```

### Monitor Raw SPI Traffic

Use logic analyzer or oscilloscope to verify:
- Clock frequency
- Data integrity
- Timing between command and response

### Test with Loopback

If possible, configure FPGA in loopback mode:
- Send data, verify it's echoed back correctly
- Validates SPI configuration and escape codec

### Incremental Testing

1. Test escape codec in isolation
2. Test SPI open/close
3. Test clear commands (no data return)
4. Test transaction read with known data
5. Test patch set/clear
6. Test full workflow

## Reference Implementation Pseudocode

### Sending a Command

```cpp
void sendCommand(int spi_fd, uint8_t command, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> payload = {command};
    payload.insert(payload.end(), data.begin(), data.end());
    
    // Apply escape encoding
    auto encoded = escapeCodec.encode(payload);
    
    // Send via SPI
    struct spi_ioc_transfer transfer = {};
    transfer.tx_buf = (unsigned long)encoded.data();
    transfer.len = encoded.size();
    transfer.speed_hz = 1000000;
    transfer.bits_per_word = 8;
    
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer);
}
```

### Reading a Transaction

```cpp
Transaction readTransaction(int spi_fd) {
    // Send command 0x01
    uint8_t cmd = 0x01;
    auto encoded_cmd = escapeCodec.encode({cmd});
    write(spi_fd, encoded_cmd.data(), encoded_cmd.size());
    
    // Read response (up to 16 bytes to account for escaping)
    uint8_t buffer[16];
    int bytes_read = read(spi_fd, buffer, sizeof(buffer));
    
    // Decode
    std::vector<uint8_t> encoded(buffer, buffer + bytes_read);
    auto decoded = escapeCodec.decode(encoded);
    
    // Parse transaction (8 bytes expected)
    return Transaction::fromBytes(decoded.data());
}
```

## Appendix: Complete Command Examples

### Example 1: Clear and Monitor

```
1. Send: 0x00 (clear transactions)
2. Wait for MCU activity
3. Send: 0x01 (read transaction)
4. Receive: 0x00 0x10 0x00 0x00 0x01 0x00 0x00 0x0A
   → Address: 0x001000 (big-endian), Count: 256 (big-endian), Time: 10ms (big-endian)
5. Repeat step 3-4 until no more transactions
```

### Example 2: Apply Patch and Observe

```
1. Send: 0x02 0x00 0x00 0x10 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
   → Set patch 0 at address 0x001000 with all 0xFF bytes
2. Send: 0x00 (clear old transactions)
3. Trigger MCU to read from 0x001000
4. Send: 0x01 (read transaction)
5. Receive transaction showing read from 0x001000
   → Confirms patch was triggered
6. Send: 0x03 (clear patches)
7. Repeat steps 3-5 to verify normal behavior restored
```

### Example 3: Multiple Patches

```
1. Send: 0x02 0x00 0x00 0x10 0x00 [8 bytes data]  → Patch 0
2. Send: 0x02 0x01 0x00 0x20 0x00 [8 bytes data]  → Patch 1
3. Send: 0x02 0x02 0x00 0x30 0x00 [8 bytes data]  → Patch 2
4. Monitor transactions to see which patches trigger
5. Send: 0x03 (clear all patches when done)
```
