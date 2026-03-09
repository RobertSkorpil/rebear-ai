# Hardware Limitation Update

## 8-Patch Maximum per Buffer

### Overview
The FPGA hardware is configured for a **maximum of 8 patch headers** per buffer upload.
- Positions 0-7: Active patch headers (up to 8 patches)
- Position 8: Terminating header (0x00 byte)

### Implementation

Added validation in both local and network protocols:

```cpp
constexpr size_t MAX_PATCHES_PER_BUFFER = 8;

bool uploadPatchBuffer(const std::vector<Patch>& patches) {
    // Hardware limitation check
    if (patches.size() > MAX_PATCHES_PER_BUFFER) {
        setError("Too many patches: hardware supports maximum 8 patches per buffer");
        return false;
    }
    // ... rest of implementation
}
```

### Buffer Size Limits

| Patches | Headers | Terminator | Data | Total | Status |
|---------|---------|------------|------|-------|--------|
| 1 | 8 | 1 | 8 | 18 bytes | ✅ Valid |
| 2 | 16 | 1 | 16 | 34 bytes | ✅ Valid |
| 4 | 32 | 1 | 32 | 66 bytes | ✅ Valid |
| 8 | 64 | 1 | 64 | 130 bytes | ✅ Valid (MAX) |
| 9+ | N/A | N/A | N/A | N/A | ❌ ERROR |

### Handling More Than 8 Patches

If you need to upload more than 8 patches, split them into batches:

```cpp
// Split patches into batches of 8
for (size_t i = 0; i < allPatches.size(); i += 8) {
    size_t count = std::min(size_t(8), allPatches.size() - i);
    std::vector<Patch> batch(allPatches.begin() + i, 
                            allPatches.begin() + i + count);
    
    if (!spi.uploadPatchBuffer(batch)) {
        std::cerr << "Batch upload failed: " << spi.getLastError() << std::endl;
        break;
    }
}
```

### Error Message

When attempting to upload more than 8 patches:
```
Too many patches: hardware supports maximum 8 patches per buffer (got 10)
```

### Documentation Updates

All documentation has been updated to reflect this limit:
- ✅ `PATCH_BUFFER_FORMAT.md` - Hardware limitations section added
- ✅ `PATCH_BUFFER_UPLOAD.md` - Maximum buffer size updated
- ✅ `BUFFER_DIAGRAM.txt` - Calculation examples updated to max 8
- ✅ `VERIFICATION.md` - Requirement 9 added
- ✅ API documentation in headers - IMPORTANT note added
- ✅ Test suite - Maximum buffer test added
- ✅ Examples - Batch upload example added

### Key Points

1. **Maximum 8 active patches** per buffer upload
2. **9th position** reserved for terminating header (0x00)
3. **Validation enforced** in code - errors returned if exceeded
4. **Batch processing** recommended for more than 8 patches
5. **No change** to minimum requirements (1 patch still valid)

### Backward Compatibility

✅ No breaking changes:
- Single patch uploads still work
- Existing code that uploads ≤8 patches unaffected
- Only code attempting >8 patches will get validation error

### Testing

Updated test suite includes:
- ✅ Single patch (1)
- ✅ Multiple patches (2)
- ✅ Maximum patches (8)
- ✅ Offset calculations for 1-8 patches
- ✅ Batch upload example for >8 patches

### Maximum Buffer Details

For 8 patches (maximum):
```
Command:        1 byte  (0x02)
Headers:       64 bytes (8 × 8)
Terminator:     1 byte  (0x00)
Data:          64 bytes (8 × 8)
─────────────────────────────
Total:        130 bytes
```

First data offset: `(8 × 8) + 1 = 65`
Last data offset: `65 + (7 × 8) = 121`

### Summary

The hardware limit of 8 patches per buffer has been properly implemented with:
- ✅ Code validation
- ✅ Clear error messages
- ✅ Updated documentation
- ✅ Batch processing examples
- ✅ Test coverage
- ✅ No breaking changes

The implementation correctly enforces this hardware limitation while maintaining backward compatibility and providing clear guidance for users who need to upload more patches.
