# Rebear CLI Usage Guide

## Overview

The `rebear-cli` command-line utility provides a scriptable interface for monitoring Flash memory transactions, managing patches, and controlling the teddy bear button.

## Installation

After building the project:

```bash
cd build
make rebear-cli
sudo make install  # Optional: installs to /usr/local/bin
```

Or run directly from the build directory:

```bash
./build/cli/rebear-cli <command> [options]
```

## Commands

### monitor - Real-time Transaction Monitoring

Monitor Flash memory transactions in real-time.

**Usage:**
```bash
rebear-cli monitor [options]
```

**Options:**
- `--device <path>` - SPI device path (default: `/dev/spidev0.0`)
- `--speed <hz>` - SPI speed in Hz (default: `100000`, max: `100000`)
- `--duration <seconds>` - Monitoring duration in seconds (default: continuous)
- `--format <text|json>` - Output format (default: `text`)

**Examples:**

Monitor for 30 seconds with text output:
```bash
rebear-cli monitor --duration 30
```

Monitor continuously (press CTRL+C to stop):
```bash
rebear-cli monitor
```

Monitor with JSON output:
```bash
rebear-cli monitor --duration 10 --format json
```

**Output (text format):**
```
Time(ms)  Address    Count  
--------  ---------  -----
0         0x001000   256
15        0x001100   128
32        0x002000   512
```

**Output (JSON format):**
```json
{
  "transactions": [
    {"timestamp": 0, "address": "0x001000", "count": 256},
    {"timestamp": 15, "address": "0x001100", "count": 128}
  ],
  "statistics": {
    "total": 2,
    "address_range": {"min": "0x001000", "max": "0x002000"}
  }
}
```

### patch - Patch Management

Manage virtual patches that modify Flash data in real-time.

#### Set a Patch

Apply a new patch to the FPGA.

**Usage:**
```bash
rebear-cli patch set --id <0-15> --address <hex> --data <16 hex chars> [options]
```

**Options:**
- `--id <0-15>` - Patch ID (0-15)
- `--address <hex>` - 24-bit Flash address (with or without 0x prefix)
- `--data <hex>` - 16 hex characters (8 bytes, no spaces)
- `--device <path>` - SPI device path (default: `/dev/spidev0.0`)

**Example:**
```bash
rebear-cli patch set --id 0 --address 0x001000 --data 0102030405060708
```

#### List Patches

Display all active patches.

**Usage:**
```bash
rebear-cli patch list [options]
```

**Options:**
- `--format <text|json>` - Output format (default: `text`)
- `--file <path>` - Load patches from file before listing

**Example:**
```bash
rebear-cli patch list
```

**Output:**
```
Active patches:
ID    Address     Data                Status
--------------------------------------------------
0     0x001000    0102030405060708    Active
5     0x002000    ffeeddccbbaa9988    Active

Total: 2 patches
```

#### Clear Patches

Remove patches from the FPGA.

**Usage:**
```bash
rebear-cli patch clear [options]
```

**Options:**
- `--all` - Clear all patches
- `--id <0-15>` - Clear specific patch by ID
- `--device <path>` - SPI device path (default: `/dev/spidev0.0`)

**Examples:**

Clear all patches:
```bash
rebear-cli patch clear --all
```

Clear specific patch:
```bash
rebear-cli patch clear --id 0
```

#### Load Patches from File

Load and apply patches from a JSON file.

**Usage:**
```bash
rebear-cli patch load --file <path> [options]
```

**Options:**
- `--file <path>` - JSON file containing patches
- `--device <path>` - SPI device path (default: `/dev/spidev0.0`)

**Example:**
```bash
rebear-cli patch load --file my_patches.json
```

**JSON File Format:**
```json
{
  "patches": [
    {
      "id": 0,
      "address": "0x001000",
      "data": "0102030405060708",
      "enabled": true
    },
    {
      "id": 5,
      "address": "0x002000",
      "data": "ffeeddccbbaa9988",
      "enabled": true
    }
  ]
}
```

#### Save Patches to File

Save current patches to a JSON file.

**Usage:**
```bash
rebear-cli patch save --file <path>
```

**Example:**
```bash
rebear-cli patch save --file my_patches.json
```

### button - Button Control

Control the teddy bear's playback button via GPIO 3.

#### Press Button

Set GPIO 3 HIGH (button pressed).

**Usage:**
```bash
rebear-cli button press
```

#### Release Button

Set GPIO 3 LOW (button released).

**Usage:**
```bash
rebear-cli button release
```

#### Click Button

Perform a complete button click (press, wait, release).

**Usage:**
```bash
rebear-cli button click [options]
```

**Options:**
- `--duration <ms>` - Press duration in milliseconds (default: `100`)

**Examples:**

Click with default 100ms duration:
```bash
rebear-cli button click
```

Click with custom duration:
```bash
rebear-cli button click --duration 200
```

#### Check Button Status

Display current button state.

**Usage:**
```bash
rebear-cli button status
```

**Output:**
```
Button status: Released
```

### export - Export Transaction Log

Collect transactions and export to a file.

**Usage:**
```bash
rebear-cli export --output <path> [options]
```

**Options:**
- `--output <path>` - Output file path (required)
- `--format <csv|json>` - Output format (default: `csv`)
- `--device <path>` - SPI device path (default: `/dev/spidev0.0`)
- `--duration <seconds>` - Collection duration in seconds (default: `10`)

**Examples:**

Export to CSV:
```bash
rebear-cli export --output transactions.csv --format csv --duration 30
```

Export to JSON:
```bash
rebear-cli export --output transactions.json --format json --duration 30
```

**CSV Format:**
```csv
timestamp_ms,address,count
0,0x001000,256
15,0x001100,128
```

**JSON Format:**
```json
{
  "transactions": [
    {"timestamp": 0, "address": "0x001000", "count": 256},
    {"timestamp": 15, "address": "0x001100", "count": 128}
  ]
}
```

### clear - Clear Transaction Buffer

Clear the FPGA's transaction buffer.

**Usage:**
```bash
rebear-cli clear [options]
```

**Options:**
- `--device <path>` - SPI device path (default: `/dev/spidev0.0`)

**Example:**
```bash
rebear-cli clear
```

## Common Workflows

### Automated Testing Workflow

```bash
# 1. Clear old transactions
rebear-cli clear

# 2. Apply a test patch
rebear-cli patch set --id 0 --address 0x001000 --data ffffffffffffffff

# 3. Press button to start playback
rebear-cli button click

# 4. Monitor what the MCU reads
rebear-cli monitor --duration 10

# 5. Clear patches
rebear-cli patch clear --all
```

### Story Mapping Workflow

```bash
# 1. Clear buffer
rebear-cli clear

# 2. Start monitoring in background
rebear-cli monitor --duration 60 --format json > story1.json &

# 3. Press button to play story
rebear-cli button click

# 4. Wait for story to complete
sleep 60

# 5. Analyze the JSON output
cat story1.json
```

### Patch Development Workflow

```bash
# 1. Create patches file
cat > test_patches.json << EOF
{
  "patches": [
    {
      "id": 0,
      "address": "0x001000",
      "data": "0102030405060708",
      "enabled": true
    }
  ]
}
EOF

# 2. Load and apply patches
rebear-cli patch load --file test_patches.json

# 3. Test the patches
rebear-cli button click
rebear-cli monitor --duration 5

# 4. Save modified patches
rebear-cli patch save --file test_patches_v2.json

# 5. Clear when done
rebear-cli patch clear --all
```

### Data Collection for Analysis

```bash
# Collect multiple sessions
for i in {1..5}; do
    echo "Session $i"
    rebear-cli clear
    rebear-cli button click
    rebear-cli export --output "session_${i}.csv" --duration 10
    sleep 2
done

# Combine CSV files
echo "timestamp_ms,address,count" > all_sessions.csv
tail -n +2 -q session_*.csv >> all_sessions.csv
```

## Scripting Examples

### Bash Script: Automated Patch Testing

```bash
#!/bin/bash
# test_patches.sh - Test multiple patches automatically

PATCHES=(
    "0:0x001000:0102030405060708"
    "1:0x002000:ffeeddccbbaa9988"
    "2:0x003000:1122334455667788"
)

for patch in "${PATCHES[@]}"; do
    IFS=':' read -r id addr data <<< "$patch"
    
    echo "Testing patch $id at $addr"
    
    # Clear and apply patch
    rebear-cli patch clear --all
    rebear-cli patch set --id "$id" --address "$addr" --data "$data"
    
    # Test
    rebear-cli clear
    rebear-cli button click
    rebear-cli monitor --duration 5 > "patch_${id}_test.log"
    
    echo "Results saved to patch_${id}_test.log"
done

# Cleanup
rebear-cli patch clear --all
```

### Python Script: Transaction Analysis

```python
#!/usr/bin/env python3
# analyze_transactions.py - Analyze transaction patterns

import json
import subprocess
import sys

def collect_transactions(duration=30):
    """Collect transactions using rebear-cli"""
    result = subprocess.run(
        ['rebear-cli', 'monitor', '--duration', str(duration), '--format', 'json'],
        capture_output=True,
        text=True
    )
    return json.loads(result.stdout)

def analyze(data):
    """Analyze transaction patterns"""
    transactions = data['transactions']
    
    # Find most accessed addresses
    addr_counts = {}
    for t in transactions:
        addr = t['address']
        addr_counts[addr] = addr_counts.get(addr, 0) + 1
    
    print("Most accessed addresses:")
    for addr, count in sorted(addr_counts.items(), key=lambda x: x[1], reverse=True)[:10]:
        print(f"  {addr}: {count} times")
    
    # Find address ranges
    addresses = [int(t['address'], 16) for t in transactions]
    print(f"\nAddress range: 0x{min(addresses):06x} - 0x{max(addresses):06x}")
    
    # Timing analysis
    timestamps = [t['timestamp'] for t in transactions]
    if len(timestamps) > 1:
        avg_interval = (timestamps[-1] - timestamps[0]) / (len(timestamps) - 1)
        print(f"Average interval: {avg_interval:.2f} ms")

if __name__ == '__main__':
    duration = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    print(f"Collecting transactions for {duration} seconds...")
    
    data = collect_transactions(duration)
    analyze(data)
```

## Troubleshooting

### Permission Denied

If you get "Permission denied" when accessing `/dev/spidev0.0`:

```bash
# Add your user to the spi group
sudo usermod -a -G spi $USER

# Or run with sudo (not recommended for regular use)
sudo rebear-cli monitor
```

### SPI Device Not Found

If `/dev/spidev0.0` doesn't exist:

```bash
# Enable SPI interface on Raspberry Pi
sudo raspi-config
# Navigate to: Interface Options -> SPI -> Enable

# Reboot
sudo reboot
```

### Button Control Not Working

If button commands fail:

```bash
# Check GPIO permissions
ls -l /dev/gpiochip0

# Add user to gpio group
sudo usermod -a -G gpio $USER

# Reboot or log out and back in
```

### No Transactions Captured

If monitoring shows no transactions:

1. Verify FPGA is connected and powered
2. Check SPI wiring
3. Ensure teddy bear MCU is active (press physical button)
4. Try clearing the buffer first: `rebear-cli clear`

## See Also

- [Implementation Guide](../plans/implementation-guide.md) - Development details
- [GPIO Interface Documentation](../plans/gpio-interface.md) - GPIO pin details
- [Technical Details](../plans/technical-details.md) - SPI protocol specification
- [Example Patches](../examples/sample_patches.json) - Sample patch configurations
