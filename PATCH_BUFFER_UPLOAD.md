# Patch Buffer Upload Feature

## Quick Start

### Before (Inefficient - Multiple SPI Transactions)
```cpp
for (const auto& patch : patches) {
    spi.setPatch(patch);  // One SPI transaction per patch
}
```

### After (Efficient - Single SPI Transaction)
```cpp
spi.uploadPatchBuffer(patches);  // One SPI transaction for all patches
```

## What Changed?

The SPI Command 0x02 (SET_PATCH) now supports uploading multiple patches in a single buffer. This provides:

✅ **Better Performance** - One SPI transaction instead of many  
✅ **Lower Overhead** - Less protocol overhead  
✅ **Easier to Use** - Upload all patches at once  
✅ **Backward Compatible** - Old code still works  

## API Reference

### SPIProtocol

#### `bool uploadPatchBuffer(const std::vector<Patch>& patches)`
Upload multiple patches in a single SPI transaction.

**Parameters:**
- `patches`: Vector of patches to upload

**Returns:**
- `true` if upload succeeded
- `false` if upload failed (check `getLastError()`)

**Example:**
```cpp
std::vector<Patch> patches;
// ... add patches ...
spi.uploadPatchBuffer(patches);
```

#### `bool setPatch(const Patch& patch)`
Upload a single patch (backward compatible).

**Note:** This now uses `uploadPatchBuffer()` internally for consistency.

### PatchManager

#### `bool applyAllBuffer(SPIProtocol& spi)`
Apply all managed patches using efficient buffer upload.

**Example:**
```cpp
PatchManager manager;
manager.loadFromFile("patches.json");
manager.applyAllBuffer(spi);
```

#### `bool applyAllBuffer(SPIProtocolNetwork& spi)`
Apply all managed patches via network protocol.

#### `bool applyAll(SPIType& spi)` (Legacy)
Apply patches one at a time. Still available for compatibility.

## Buffer Format

### Structure
```
[HEADERS...][TERMINATOR][DATA...]
```

### Patch Header (8 bytes each)
```
+0: STORED (0x80=enabled, 0x00=disabled)
+1: ADDRESS high byte
+2: ADDRESS mid byte
+3: ADDRESS low byte
+4: LENGTH high byte (always 0x00)
+5: LENGTH low byte (always 0x08)
+6: BUFFER_DATA offset high byte
+7: BUFFER_DATA offset low byte
```

### Terminator
Single 0x00 byte after last header

### Data Section
8 bytes per patch, referenced by BUFFER_DATA offset

## Examples

### Example 1: Simple Upload
```cpp
#include <rebear/spi_protocol.h>

SPIProtocol spi;
spi.open("/dev/spidev0.0");

std::vector<Patch> patches;

Patch p1;
p1.address = 0x001000;
p1.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
p1.enabled = true;
patches.push_back(p1);

Patch p2;
p2.address = 0x002000;
p2.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
p2.enabled = true;
patches.push_back(p2);

spi.uploadPatchBuffer(patches);
```

### Example 2: Using PatchManager
```cpp
#include <rebear/patch_manager.h>
#include <rebear/spi_protocol.h>

PatchManager manager;
manager.loadFromFile("my_patches.json");

SPIProtocol spi;
spi.open("/dev/spidev0.0");

manager.applyAllBuffer(spi);
```

### Example 3: Network Protocol
```cpp
#include <rebear/spi_protocol_network.h>
#include <rebear/patch_manager.h>

SPIProtocolNetwork spi("192.168.1.100", 9876);
spi.open("/dev/spidev0.0");

PatchManager manager;
manager.loadFromFile("patches.json");

manager.applyAllBuffer(spi);
```

## Buffer Size Calculation

For N patches:
```
Buffer Size = (N × 8) + 1 + (N × 8)
            = (N × 16) + 1
```

Examples:
- 1 patch: 17 bytes
- 2 patches: 33 bytes
- 8 patches (max): 129 bytes
- 10 patches: ❌ ERROR - exceeds hardware limit

## Offset Calculation

First patch data offset: `(N × 8) + 1`  
Patch[i] data offset: `(N × 8) + 1 + (i × 8)`

Examples:
- 1 patch: data at offset 9
- 2 patches: data at offsets 17, 25
- 3 patches: data at offsets 25, 33, 41
- 8 patches: data at offsets 65, 73, ..., 121

## Files Modified

- `lib/src/spi_protocol.cpp`
- `lib/include/rebear/spi_protocol.h`
- `lib/src/spi_protocol_network.cpp`
- `lib/include/rebear/spi_protocol_network.h`
- `lib/include/rebear/patch_manager.h`
- `server/command_handler.cpp`

## New Files

- `PATCH_BUFFER_FORMAT.md` - Detailed format specification
- `BUFFER_DIAGRAM.txt` - Visual buffer layout diagrams
- `lib/test_patch_buffer.cpp` - Test suite
- `lib/example_patch_buffer.cpp` - Usage examples
- `IMPLEMENTATION_SUMMARY.md` - Technical summary

## Testing

Run the test suite:
```bash
cd build
cmake .. -DBUILD_TESTS=ON
make test_patch_buffer
./test_patch_buffer
```

Run the examples:
```bash
make example_patch_buffer
./example_patch_buffer
```

## Migration Guide

### If you're using `setPatch()` directly
✅ **No changes needed** - it still works the same way

### If you're looping through patches
Before:
```cpp
for (const auto& patch : myPatches) {
    spi.setPatch(patch);
}
```

After (more efficient):
```cpp
spi.uploadPatchBuffer(myPatches);
```

### If you're using PatchManager
Before:
```cpp
manager.applyAll(spi);
```

After (more efficient):
```cpp
manager.applyAllBuffer(spi);
```

Both methods still work, but `applyAllBuffer()` is faster.

## Backward Compatibility

✅ All existing code continues to work  
✅ `setPatch()` still accepts single patches  
✅ Old network protocol format still supported  
✅ Server auto-detects old vs new format  

## Performance Comparison

Uploading 10 patches:

**Old Method:**
- 10 SPI transactions
- ~10× protocol overhead
- Slower

**New Method:**
- 1 SPI transaction
- 1× protocol overhead
- Faster

The more patches you upload, the bigger the performance gain!

## Notes

- Patch ID field is no longer used in buffer format (but kept for compatibility)
- All multi-byte values are big-endian
- **Maximum 8 patches per buffer (FPGA hardware limitation)**
- Maximum 16 patches per buffer (FPGA limitation)
- Disabled patches are transmitted but not applied by FPGA
- SPI escape encoding is handled automatically

## Troubleshooting

### "Too many patches: hardware supports maximum 8 patches"
You're trying to upload more than 8 patches at once. Split them into multiple uploads:
```cpp
// Split into batches of 8
for (size_t i = 0; i < patches.size(); i += 8) {
    size_t count = std::min(size_t(8), patches.size() - i);
    std::vector<Patch> batch(patches.begin() + i, patches.begin() + i + count);
    spi.uploadPatchBuffer(batch);
}
```

### "Invalid patch configuration in buffer"
Check that all patches have:
- Valid address (< 0x1000000)
- Valid ID (0-15, if using PatchManager)
- 8 bytes of data

### "Upload failed"
Check:
- SPI device is open
- Device path is correct
- You have permissions to access /dev/spidev*

### "Buffer too large"
You might be uploading too many patches. **Maximum is 8 patches per buffer** (hardware limitation). Split your patches into batches.

## See Also

- [PATCH_BUFFER_FORMAT.md](PATCH_BUFFER_FORMAT.md) - Detailed format specification
- [BUFFER_DIAGRAM.txt](BUFFER_DIAGRAM.txt) - Visual diagrams
- [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) - Implementation details
