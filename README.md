# Rebear - Teddy Bear Reverse Engineering Toolkit

A comprehensive toolkit for reverse-engineering storytelling teddy bears through FPGA-based Flash memory monitoring and patching.

## Project Overview

This project enables analysis of encrypted Flash memory in storytelling teddy bears by intercepting and modifying SPI communication between the MCU and Flash memory using an FPGA. Since direct cryptographic analysis proved unsuccessful, this approach allows behavioral analysis through selective data modification.

The external Flash chip (~4 MB) contains encrypted audio stories and bookkeeping data. A complete dump of this Flash memory ([`data/flash.bin`](data/flash.bin)) is available for static analysis and reference.

## Features

### Core Capabilities
- **Real-time Transaction Monitoring**: Capture and analyze Flash memory access patterns
- **Virtual Patching**: Inject modified data at specific addresses to observe behavioral changes
- **Button Control**: Programmatically control teddy bear playback via GPIO
- **Efficient Buffering**: Hardware-assisted transaction buffering with ready signal

### Components
- **librebear**: Core C++ library for SPI/GPIO communication and data management
- **rebear-cli**: Command-line utility for scripting and automation
- **rebear-gui**: Qt5-based GUI application for interactive analysis

## Hardware Requirements

- Raspberry Pi 3 (or compatible)
- FPGA with Altera/Avalon SPI core (configured as SPI slave)
- Storytelling teddy bear (target device)
- Wiring for SPI bus tap and GPIO connections

### Pin Connections

| Raspberry Pi | FPGA/Device | Function |
|--------------|-------------|----------|
| SPI0 MOSI | FPGA SPI MOSI | Command/data to FPGA |
| SPI0 MISO | FPGA SPI MISO | Transaction data from FPGA |
| SPI0 SCLK | FPGA SPI SCLK | SPI clock |
| SPI0 CE0 | FPGA SPI CS | Chip select |
| GPIO 3 | Teddy button | Button control (output) |
| GPIO 4 | FPGA ready | Buffer ready signal (input) |

## Software Requirements

### Build Dependencies
- CMake 3.10+
- C++17 compiler (GCC 7+ or Clang 5+)
- Qt5 (Core, Widgets, Charts) - for GUI only
- Linux kernel with SPI and GPIO support

### Runtime Requirements
- Raspberry Pi OS (or compatible Linux)
- SPI interface enabled (`/dev/spidev0.0`)
- GPIO access permissions

## Quick Start

### 1. Enable SPI and GPIO

```bash
# Enable SPI interface
sudo raspi-config
# Navigate to: Interface Options -> SPI -> Enable

# Add user to spi and gpio groups
sudo usermod -a -G spi,gpio $USER

# Reboot for changes to take effect
sudo reboot
```

### 2. Build the Project

```bash
# Clone repository
cd /home/robert/rebear

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build (use -j4 for parallel build on Pi)
make -j4

# Install (optional)
sudo make install
```

### 3. Basic Usage

#### Command-Line Interface

```bash
# Monitor transactions in real-time
./cli/rebear-cli monitor --duration 30

# Set a patch at address 0x001000
./cli/rebear-cli patch set --id 0 --address 0x001000 --data "0102030405060708"

# Press teddy bear button
./cli/rebear-cli button click

# Export transaction log
./cli/rebear-cli export --output transactions.csv
```

#### GUI Application

```bash
# Launch GUI
./gui/rebear-gui

# Or if installed:
rebear-gui
```

## Architecture

### System Overview

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│   Teddy     │  SPI    │    FPGA      │  SPI    │ Raspberry   │
│   Bear MCU  │◄───────►│  (Tap/Patch) │◄───────►│   Pi 3      │
└─────────────┘         └──────────────┘         └─────────────┘
       │                       │                        │
       │ SPI                   │ Monitors &             │ GPIO
       ▼                       │ Modifies               ▼
┌─────────────┐               │                  ┌──────────┐
│   Flash     │◄──────────────┘                  │  Button  │
│   Memory    │                                   │  Ready   │
└─────────────┘                                   └──────────┘
```

### Key Components

- **EscapeCodec**: Handles Avalon SPI escape sequences (0x4a, 0x4d)
- **Transaction**: Represents Flash read operations (address, count, timestamp)
- **Patch**: Virtual data modification at specific addresses
- **SPIProtocol**: Low-level FPGA communication
- **GPIOControl**: Button control and buffer ready monitoring
- **PatchManager**: High-level patch management with file I/O

## Documentation

Comprehensive documentation is available in the [`plans/`](plans/) directory:

- **[Project Architecture](plans/project-architecture.md)**: Overall system design and component structure
- **[Technical Details](plans/technical-details.md)**: SPI protocol specification and escape sequences
- **[GPIO Interface](plans/gpio-interface.md)**: Button control and buffer ready signal
- **[Implementation Guide](plans/implementation-guide.md)**: Development phases and best practices
- **[System Diagrams](plans/system-diagrams.md)**: Visual architecture and data flow diagrams

## SPI Protocol Summary

The FPGA implements four commands:

| Command | Function | Data |
|---------|----------|------|
| 0x00 | Clear transaction buffer | None |
| 0x01 | Read next transaction | Returns 8 bytes |
| 0x02 | Set virtual patch | Requires 12 bytes |
| 0x03 | Clear all patches | None |

**Important**: All data is subject to Avalon escape encoding:
- `0x4a` → `0x4d 0x6a`
- `0x4d` → `0x4d 0x6d`

## GPIO Interface Summary

- **GPIO 3 (Output)**: Button control - Write 1 to press, 0 to release
- **GPIO 4 (Input)**: Buffer ready - High when transactions available

## Example Workflow

### Analyzing Story Playback

```bash
# 1. Clear old data
rebear-cli clear

# 2. Press button to start playback
rebear-cli button click

# 3. Monitor transactions for 10 seconds
rebear-cli monitor --duration 10 --output story1.csv

# 4. Analyze which addresses were accessed
# (Use external tools or custom scripts)

# 5. Apply patch to modify behavior
rebear-cli patch set --id 0 --address 0x010000 --data "FFFFFFFFFFFFFFFF"

# 6. Press button again and observe changes
rebear-cli button click
rebear-cli monitor --duration 10 --output story1_patched.csv

# 7. Compare results
diff story1.csv story1_patched.csv
```

## Troubleshooting

### SPI Communication Issues

**Problem**: Cannot open `/dev/spidev0.0`
- Solution: Enable SPI in `raspi-config` and reboot
- Solution: Check permissions - add user to `spi` group

**Problem**: Receiving all 0xFF data
- Solution: Check buffer ready signal (GPIO 4) before reading
- Solution: Verify FPGA is powered and programmed
- Solution: Check SPI wiring and connections

### GPIO Issues

**Problem**: Button press has no effect
- Solution: Verify GPIO 3 is configured as output
- Solution: Check wiring to teddy bear button
- Solution: Increase press duration (try 200ms)

**Problem**: Buffer ready always low
- Solution: Verify GPIO 4 is configured as input
- Solution: Check FPGA connection
- Solution: Generate test transactions to verify signal

### Build Issues

**Problem**: Qt5 not found
- Solution: Install Qt5 development packages: `sudo apt-get install qt5-default libqt5charts5-dev`
- Solution: Or disable GUI build: `cmake -DBUILD_GUI=OFF ..`

## Development Status

### Completed (Phase 1) ✅
- ✅ **Phase 1.1**: Project setup and CMake configuration
- ✅ **Phase 1.2**: EscapeCodec implementation (free functions)
- ✅ **Phase 1.3**: Transaction class with big-endian parsing
- ✅ **Phase 1.4**: Patch class with validation
- ✅ **Phase 1.5**: SPIProtocol class with full FPGA communication
- ✅ **Phase 1.6**: PatchManager class with JSON persistence
- ✅ **Phase 1.7**: GPIOControl class for button and buffer monitoring

**Phase 1 is COMPLETE!** The core library is fully functional.

### Completed (Phase 2) ✅
- ✅ **Phase 2**: Command-line utility (rebear-cli)
  - ✅ Monitor command - Real-time transaction monitoring
  - ✅ Patch command - Patch management (set, list, clear, load, save)
  - ✅ Button command - GPIO button control (press, release, click, status)
  - ✅ Export command - Transaction export (CSV, JSON)
  - ✅ Clear command - Buffer management
  - ✅ Comprehensive documentation ([`docs/CLI_USAGE.md`](docs/CLI_USAGE.md))

**Phase 2 is COMPLETE!** The CLI utility is fully functional and documented.

### Current Implementation
The project now includes:

**Core Library ([`librebear`](lib/)):**
- SPI communication with FPGA (MODE 1, 100 kHz)
- Avalon escape sequence encoding/decoding
- Transaction monitoring and parsing
- GPIO control for button and buffer ready signal
- Patch management and application
- JSON-based patch configuration files
- Comprehensive test suite with hardware validation

**Command-Line Utility ([`rebear-cli`](cli/)):**
- Real-time transaction monitoring with multiple output formats
- Complete patch management (create, list, apply, clear, save, load)
- Button control for automated testing
- Transaction export to CSV and JSON
- Scriptable interface for automation
- Full documentation with examples

### In Progress
- Phase 3: Qt GUI application (rebear-gui)

See [`plans/implementation-guide.md`](plans/implementation-guide.md) for detailed development roadmap.

## Contributing

This is a personal reverse-engineering project. Contributions, suggestions, and improvements are welcome.

## License

To be determined - this is a research/educational project for reverse-engineering purposes.

## Safety and Legal Considerations

- This toolkit is designed for analyzing devices you own
- Reverse engineering may void warranties
- Ensure compliance with local laws regarding device modification
- Be careful with GPIO connections - incorrect wiring can damage hardware
- Always disconnect power when making hardware changes

## Acknowledgments

- FPGA design and implementation
- Altera/Avalon SPI IP core documentation
- Raspberry Pi community and documentation
- Qt framework for GUI development

## Contact

Project maintained by Robert - see repository for contact information.

---

**Note**: This README provides an overview. Refer to the detailed documentation in the [`plans/`](plans/) directory for comprehensive technical information, implementation guidelines, and architectural details.
