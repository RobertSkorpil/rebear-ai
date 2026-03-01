# Data Directory

This directory contains data files related to the teddy bear reverse-engineering project.

## Files

### flash.bin

**Size**: ~4 MB (4,009,015 bytes)

**Description**: Complete dump of the external Flash memory chip from the storytelling teddy bear. This file contains the encrypted audio data and bookkeeping information that the MCU reads during operation.

**Content**: 
- Encrypted audio stories
- Index/header information for story navigation
- Bookkeeping data (story boundaries, metadata, etc.)

**Note**: This is NOT the MCU's program code. The MCU has its own internal Flash memory that contains the firmware/program code, which is not accessible via the external SPI bus.

## Usage

### Analyzing Flash Content

The flash.bin file can be used for:

1. **Static Analysis**: Examine the structure and patterns in the encrypted data
2. **Reference Data**: Compare live transaction addresses against known Flash content
3. **Patch Development**: Identify specific addresses to patch based on observed access patterns
4. **Decryption Attempts**: Analyze encryption patterns (though direct cryptanalysis has been unsuccessful)

### Example Workflows

#### 1. Find Story Boundaries

```bash
# Monitor which addresses are accessed during playback
rebear-cli clear
rebear-cli button click
rebear-cli monitor --duration 30 --output story1_access.csv

# Analyze the addresses to identify story boundaries
# (Use external tools or custom scripts)
```

#### 2. Correlate Transactions with Flash Content

```python
# Python example
import struct

# Read flash.bin
with open('data/flash.bin', 'rb') as f:
    flash_data = f.read()

# Read transaction log
import csv
with open('story1_access.csv', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        addr = int(row['address'], 16)
        count = int(row['count'])
        
        # Extract data from flash.bin at this address
        data = flash_data[addr:addr+count]
        
        # Analyze data (look for patterns, headers, etc.)
        print(f"Address {hex(addr)}: {data[:16].hex()}...")
```

#### 3. Create Targeted Patches

```bash
# After identifying interesting addresses, create patches
# Example: Modify data at address 0x001000
rebear-cli patch set --id 0 --address 0x001000 --data "FFFFFFFFFFFFFFFF"

# Test the patch
rebear-cli button click
rebear-cli monitor --duration 30 --output story1_patched.csv

# Compare behavior
diff story1_access.csv story1_patched.csv
```

### Hex Dump Analysis

View specific regions of the Flash:

```bash
# View first 256 bytes (possible header)
hexdump -C data/flash.bin | head -n 16

# View data at specific address (e.g., 0x001000)
hexdump -C -s 0x001000 -n 256 data/flash.bin

# Search for patterns
strings data/flash.bin | less
```

### Binary Analysis Tools

Recommended tools for analyzing flash.bin:

- **hexdump**: View hex representation
- **binwalk**: Identify file signatures and embedded data
- **strings**: Extract ASCII strings
- **xxd**: Create hex dumps
- **010 Editor**: Advanced binary editor with templates
- **Ghidra**: Reverse engineering framework (for structure analysis)

### Encryption Analysis

The data is encrypted, but you can still analyze:

1. **Entropy**: High entropy suggests strong encryption
2. **Patterns**: Look for repeated blocks or headers
3. **Boundaries**: Identify where different sections start/end
4. **Metadata**: Some metadata might be unencrypted

```bash
# Calculate entropy (requires ent tool)
ent data/flash.bin

# Look for low-entropy regions (possible headers/metadata)
# (Custom script needed)
```

## File Format (Hypothetical)

Based on the size (~4 MB) and purpose (audio stories), the Flash likely contains:

```
0x000000 - 0x000FFF: Header/Index (4 KB)
  - Story count
  - Story addresses
  - Story lengths
  - Metadata (titles, durations, etc.)
  - Encryption keys/IVs (if stored)

0x001000 - 0x3FFFFF: Audio Data (~4 MB)
  - Multiple stories
  - Possibly compressed (MP3, AAC, or custom format)
  - Encrypted with unknown algorithm
  - May include checksums or padding
```

**Note**: This is speculative. Actual structure must be determined through analysis.

## Backup and Version Control

**Important**: The flash.bin file should be:
- Backed up regularly (it's your only copy of the original data)
- Version controlled (track any modifications)
- Never modified directly (work with copies)

```bash
# Create a working copy
cp data/flash.bin data/flash_working.bin

# Create a backup
cp data/flash.bin data/flash_backup_$(date +%Y%m%d).bin
```

## Future Enhancements

Potential tools to add to the project:

1. **Flash Analyzer**: GUI tool to visualize Flash structure
2. **Pattern Detector**: Automatically identify story boundaries
3. **Diff Tool**: Compare Flash dumps before/after patches
4. **Decryption Helper**: Assist with encryption analysis
5. **Audio Extractor**: Extract and decode audio (once decryption is solved)

## Related Documentation

- [Project Architecture](../plans/project-architecture.md)
- [Technical Details](../plans/technical-details.md)
- [Implementation Guide](../plans/implementation-guide.md)

## Notes

- The Flash chip is likely 4 MB (32 Mbit) based on file size
- Common Flash chips: W25Q32, MX25L3206E, AT25SF041, etc.
- SPI Flash typically uses 24-bit addressing (0x000000 - 0xFFFFFF)
- The actual used space might be less than 4 MB (check for 0xFF padding at end)

## Security Considerations

- This file contains copyrighted audio content (stories)
- Reverse engineering is for personal/educational use only
- Do not distribute decrypted audio content
- Respect intellectual property rights
