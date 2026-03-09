# Patch Buffer Format Documentation

## Overview

The SPI Command 0x02 (SET_PATCH) now supports uploading multiple patches in a single buffer. This is more efficient than sending individual patches one at a time.

## Hardware Limitations

⚠️ **IMPORTANT**: The FPGA hardware supports a **maximum of 8 patch headers** per buffer upload.
- You can upload up to 8 patches in a single buffer
- The 9th header position is reserved for the terminating header (0x00 byte)
- Attempting to upload more than 8 patches will result in an error

## Buffer Structure

The buffer contains:
1. Patch headers (one per patch)
2. A terminating header (single 0x00 byte)
3. Patch data (8 bytes per patch)

```
[PATCH_HEADER_0, PATCH_HEADER_1, ..., TERMINATOR, PATCH_DATA_0, PATCH_DATA_1, ...]
```

## Patch Header Format (8 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 byte | STORED | 0x80 = enabled, 0x00 = disabled |
| 1-3 | 3 bytes | PATCH_ADDRESS | 24-bit address (big-endian) |
| 4-5 | 2 bytes | PATCH_LENGTH | Length of patch data (big-endian), always 0x0008 |
| 6-7 | 2 bytes | BUFFER_DATA | Offset to patch data in buffer (big-endian) |

## Terminator

The patch headers must be terminated by a single 0x00 byte. This signals the end of the header section.

## Patch Data

The patch data follows the terminator. Each patch has 8 bytes of replacement data. The BUFFER_DATA field in each header points to the offset of that patch's data within the entire buffer.

**Important:** The minimum valid BUFFER_DATA offset is 9 (for a single patch: 8 bytes header + 1 byte terminator).

## Example: Single Patch

Buffer for one patch at address 0x001000 with data `[0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]`:

```
Offset | Data | Description
-------|------|-------------
0      | 0x80 | STORED (enabled)
1-3    | 0x00 0x10 0x00 | PATCH_ADDRESS (0x001000)
4-5    | 0x00 0x08 | PATCH_LENGTH (8 bytes)
6-7    | 0x00 0x09 | BUFFER_DATA (offset 9)
8      | 0x00 | Terminator
9-16   | 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF | Patch data
```

Total: 17 bytes (before escape encoding)

## Example: Multiple Patches

Buffer for two patches:
- Patch 1: address 0x001000, data `[0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08]` (enabled)
- Patch 2: address 0x002000, data `[0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22]` (disabled)

```
Offset | Data | Description
-------|------|-------------
// Header 1
0      | 0x80 | STORED (enabled)
1-3    | 0x00 0x10 0x00 | PATCH_ADDRESS (0x001000)
4-5    | 0x00 0x08 | PATCH_LENGTH (8 bytes)
6-7    | 0x00 0x11 | BUFFER_DATA (offset 17)

// Header 2
8      | 0x00 | STORED (disabled)
9-11   | 0x00 0x20 0x00 | PATCH_ADDRESS (0x002000)
12-13  | 0x00 0x08 | PATCH_LENGTH (8 bytes)
14-15  | 0x00 0x19 | BUFFER_DATA (offset 25)

// Terminator
16     | 0x00 | Terminator

// Data
17-24  | 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 | Patch 1 data
25-32  | 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF 0x11 0x22 | Patch 2 data
```

Total: 33 bytes (before escape encoding)

## C++ Usage

### Single Patch (Backward Compatible)

```cpp
#include <rebear/spi_protocol.h>

rebear::SPIProtocol spi;
spi.open("/dev/spidev0.0");

rebear::Patch patch;
patch.id = 0;  // Not used in new format, but kept for compatibility
patch.address = 0x001000;
patch.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
patch.enabled = true;

// Old method (still works, converts to buffer internally)
spi.setPatch(patch);
```

### Multiple Patches (Efficient)

```cpp
#include <rebear/spi_protocol.h>

rebear::SPIProtocol spi;
spi.open("/dev/spidev0.0");

std::vector<rebear::Patch> patches;

rebear::Patch patch1;
patch1.address = 0x001000;
patch1.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
patch1.enabled = true;
patches.push_back(patch1);

rebear::Patch patch2;
patch2.address = 0x002000;
patch2.data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
patch2.enabled = false;
patches.push_back(patch2);

// New method (sends all patches in one SPI transaction)
spi.uploadPatchBuffer(patches);
```

### Using PatchManager

```cpp
#include <rebear/patch_manager.h>
#include <rebear/spi_protocol.h>

rebear::PatchManager manager;
rebear::SPIProtocol spi;

// Load patches from file
manager.loadFromFile("patches.json");

// Open SPI
spi.open("/dev/spidev0.0");

// Apply all patches efficiently
manager.applyAllBuffer(spi);  // Uses new buffer format
// OR
manager.applyAll(spi);  // Legacy method (sends patches one by one)
```

## Network Protocol Support

The network protocol (`SPIProtocolNetwork`) also supports the new buffer format:

```cpp
#include <rebear/spi_protocol_network.h>
#include <rebear/patch_manager.h>

rebear::SPIProtocolNetwork spi("192.168.1.100", 9876);
spi.open("/dev/spidev0.0");

rebear::PatchManager manager;
manager.loadFromFile("patches.json");

// Apply all patches via network
manager.applyAllBuffer(spi);
```

## SPI Encoding

All data sent via SPI must be escape-encoded using the Avalon protocol:
- `0x4A` → `0x4D 0x6A`
- `0x4D` → `0x4D 0x6D`

The `SPIProtocol` class handles this encoding automatically.

## Notes

- The `Patch.id` field is no longer used in the buffer format (patches are identified by their position in the buffer), but it's kept for backward compatibility and internal management.
- All multi-byte values (address, length, offset) are in big-endian format.
- **Hardware limitation: Maximum 8 patches per buffer.** The 9th header position is for the terminating header.
- The FPGA will process patches in the order they appear in the buffer.
- Disabled patches (STORED = 0x00) are still transmitted but will not be applied by the FPGA.
- The minimum buffer size is 9 bytes (1 header + 1 terminator + no data for a terminator-only buffer, but this is invalid).
- A valid single-patch buffer is 17 bytes minimum.
- Maximum buffer size is 130 bytes (8 headers + 1 terminator + 8×8 data = 64+1+64+1 command = 130).
