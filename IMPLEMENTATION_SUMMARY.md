# Patch Buffer Implementation - Summary of Changes

## Overview
Implemented a new SPI command 2 buffer format that allows uploading multiple patches in a single transaction. This is more efficient than sending patches individually.

## Modified Files

### 1. lib/src/spi_protocol.cpp
- **Modified `setPatch()`**: Now internally calls `uploadPatchBuffer()` with a single patch
- **Added `uploadPatchBuffer()`**: New method to upload multiple patches in buffer format
  - Builds buffer with headers, terminator, and data sections
  - Calculates offsets correctly
  - Validates all patches before sending

### 2. lib/include/rebear/spi_protocol.h
- Added declaration for `uploadPatchBuffer(const std::vector<Patch>& patches)`
- Updated documentation with buffer format details

### 3. lib/src/spi_protocol_network.cpp
- **Modified `setPatch()`**: Kept backward compatible with old format
- **Added `uploadPatchBuffer()`**: Network version of buffer upload
  - Uses same buffer format as local SPI
  - Sends via network protocol

### 4. lib/include/rebear/spi_protocol_network.h
- Added declaration for `uploadPatchBuffer()`
- Updated documentation

### 5. lib/include/rebear/patch_manager.h
- Added forward declaration of `SPIProtocolNetwork`
- **Added `applyAllBuffer(SPIProtocol&)`**: Efficient method to apply all patches using buffer format
- **Added `applyAllBuffer(SPIProtocolNetwork&)`**: Network version
- Kept existing `applyAll()` for backward compatibility

### 6. server/command_handler.cpp
- **Modified `handleSpiSetPatch()`**: Now supports both old and new formats
  - Auto-detects format based on payload structure
  - Parses new buffer format with multiple patches
  - Maintains backward compatibility with old single-patch format

## New Files

### 1. PATCH_BUFFER_FORMAT.md
- Complete documentation of the new buffer format
- Examples with byte-by-byte breakdown
- C++ usage examples
- Notes on encoding and compatibility

### 2. lib/test_patch_buffer.cpp
- Test suite for buffer format validation
- Tests single patch buffer
- Tests multiple patch buffer
- Tests offset calculations
- Tests minimum buffer requirements

## Buffer Format Specification

### Structure
```
[PATCH_HEADER_0, PATCH_HEADER_1, ..., TERMINATOR, PATCH_DATA_0, PATCH_DATA_1, ...]
```

### Patch Header (8 bytes)
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 byte | STORED | 0x80 = enabled, 0x00 = disabled |
| 1-3 | 3 bytes | PATCH_ADDRESS | 24-bit address (big-endian) |
| 4-5 | 2 bytes | PATCH_LENGTH | Always 0x0008 (big-endian) |
| 6-7 | 2 bytes | BUFFER_DATA | Offset to patch data (big-endian) |

### Terminator
- Single 0x00 byte after last header

### Patch Data
- 8 bytes per patch
- Referenced by BUFFER_DATA offset in header

## Backward Compatibility

✅ **Maintained**
- `setPatch()` still works with single patches
- Old client code continues to work without changes
- Server auto-detects format and handles both

## Usage Examples

### Before (Old Method)
```cpp
for (const auto& patch : patches) {
    spi.setPatch(patch);  // Multiple SPI transactions
}
```

### After (New Method)
```cpp
spi.uploadPatchBuffer(patches);  // Single SPI transaction
```

### With PatchManager
```cpp
manager.applyAllBuffer(spi);  // Efficient bulk upload
```

## Benefits

1. **Performance**: Single SPI transaction instead of multiple
2. **Efficiency**: Less overhead, faster uploads
3. **Simplicity**: Upload all patches at once
4. **Compatibility**: Old code still works
5. **Flexibility**: Support for both local and network protocols

## Testing

Run the test program:
```bash
./test_patch_buffer
```

Expected output:
- ✓ Single patch buffer format validated
- ✓ Multiple patch buffer format validated
- ✓ Offset calculation validated
- ✓ Minimum buffer requirements validated

## Notes

- The `Patch.id` field is still present but not used in buffer format
- All multi-byte values are big-endian
- Minimum valid buffer: 17 bytes (1 header + terminator + data)
- FPGA processes patches in buffer order
- Disabled patches (STORED=0x00) are transmitted but not applied
