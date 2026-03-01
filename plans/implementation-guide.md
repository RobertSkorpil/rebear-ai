# Implementation Guide

## Development Phases

This guide outlines the recommended order for implementing the teddy bear reverse-engineering project components.

## Important Notes

- **Phase Completion Documents**: All `PHASE_X.Y_COMPLETE.md` files should be placed in the [`plans/`](plans/) directory, NOT in the project root
- **Documentation Location**: Keep all planning and completion documents organized in the plans folder

## Coding Standards

- **Header Guards**: Always use `#pragma once` instead of traditional `#ifndef`/`#define` guards
- **C++ Standard**: C++17
- **Naming**: Use snake_case for files, PascalCase for classes, camelCase for methods

## Phase 1: Core Library Foundation

### 1.1 Project Setup
- Create directory structure
- Set up root CMakeLists.txt
- Configure C++17 standard and compiler flags
- Set up library subdirectory

### 1.2 EscapeCodec Implementation

**Priority**: CRITICAL - Required for all SPI communication

**Files**:
- [`lib/include/rebear/escape_codec.h`](lib/include/rebear/escape_codec.h)
- [`lib/src/escape_codec.cpp`](lib/src/escape_codec.cpp)

**Implementation Note**: EscapeCodec is implemented as free functions in the `rebear` namespace, not as a class.

**Key Functions**:
```cpp
namespace rebear {
    // Constants
    constexpr uint8_t ESCAPE_CHAR = 0x4d;
    constexpr uint8_t IDLE_CHAR = 0x4a;
    constexpr uint8_t XOR_MASK = 0x20;
    
    // Check if a byte needs escaping
    bool needsEscape(uint8_t byte);
    
    // Encode data for transmission to FPGA
    std::vector<uint8_t> encode(const std::vector<uint8_t>& data);
    
    // Decode data received from FPGA
    std::vector<uint8_t> decode(const std::vector<uint8_t>& data);
}
```

**Test Cases**:
- Empty vector
- Single byte (no escape needed)
- Single 0x4a (should become 0x4d 0x6a)
- Single 0x4d (should become 0x4d 0x6d)
- Mixed data with multiple escape sequences
- Round-trip encode/decode verification

### 1.3 Transaction Class

**Priority**: HIGH - Core data structure

**Files**:
- [`lib/include/rebear/transaction.h`](lib/include/rebear/transaction.h)
- [`lib/src/transaction.cpp`](lib/src/transaction.cpp)

**Key Features**:
```cpp
class Transaction {
public:
    uint32_t address;   // 24-bit address (stored as 32-bit, big-endian from FPGA)
    uint32_t count;     // 24-bit byte count (stored as 32-bit, big-endian from FPGA)
    uint16_t timestamp; // 16-bit milliseconds (big-endian from FPGA)
    std::chrono::system_clock::time_point captureTime;
    
    // Parse from 8-byte FPGA response (all values big-endian)
    static Transaction fromBytes(const uint8_t* data);
    
    // Serialize to bytes (for export/storage)
    std::vector<uint8_t> toBytes() const;
    
    // Human-readable string
    std::string toString() const;
    
    // Validate data
    bool isValid() const;
};
```

**Validation Rules**:
- Address should be < 0x1000000 (24-bit max)
- Count should be reasonable (< 1MB), OR 0xFFFFFF (special value indicating patch was applied)
- Timestamp can be any 16-bit value (0-65535 ms)

**Special Values**:
- Count = 0xFFFFFF: Indicates the FPGA actively patched this transaction. The actual byte count is unknown because the FPGA doesn't count data when patching.

**Note**: All multi-byte values from FPGA are transmitted **big-endian** (MSB first).

### 1.4 Patch Class

**Priority**: HIGH - Core data structure

**Files**:
- [`lib/include/rebear/patch.h`](lib/include/rebear/patch.h)
- [`lib/src/patch.cpp`](lib/src/patch.cpp)

**Key Features**:
```cpp
class Patch {
public:
    uint8_t id;        // 0-15
    uint32_t address;  // 24-bit address
    std::array<uint8_t, 8> data;
    bool enabled;
    
    // Serialize for SPI transmission (12 bytes)
    std::vector<uint8_t> toBytes() const;
    
    // Parse from bytes
    static Patch fromBytes(const uint8_t* data);
    
    // Human-readable string
    std::string toString() const;
    
    // Validate
    bool isValid() const;
};
```

**Validation Rules**:
- ID must be 0-15
- Address must be < 0x1000000
- Data can be any 8 bytes

### 1.5 SPIProtocol Class

**Priority**: CRITICAL - Hardware interface

**Files**:
- [`lib/include/rebear/spi_protocol.h`](lib/include/rebear/spi_protocol.h)
- [`lib/src/spi_protocol.cpp`](lib/src/spi_protocol.cpp)

**Key Features**:
```cpp
class SPIProtocol {
public:
    SPIProtocol();
    ~SPIProtocol();
    
    // Open SPI device
    bool open(const std::string& device = "/dev/spidev0.0",
              uint32_t speed = 100000);  // 100 kHz - REQUIRED for FPGA
    
    // Close device
    void close();
    
    // Command 0x00: Clear transaction buffer
    bool clearTransactions();
    
    // Command 0x01: Read next transaction
    std::optional<Transaction> readTransaction();
    
    // Command 0x02: Set patch
    bool setPatch(const Patch& patch);
    
    // Command 0x03: Clear all patches
    bool clearPatches();
    
    // Status
    bool isConnected() const;
    std::string getLastError() const;
    
private:
    int fd_;
    EscapeCodec codec_;
    std::string lastError_;
    
    // Low-level SPI transfer
    bool transfer(const std::vector<uint8_t>& tx, 
                  std::vector<uint8_t>& rx, 
                  size_t rxLen);
};
```

**Implementation Notes**:
- Use Linux `spidev` interface (`<linux/spi/spidev.h>`)
- **CRITICAL**: Configure SPI MODE 1 (CPOL=0, CPHA=1) - Required for FPGA
- **CRITICAL**: Maximum speed 100 kHz (100000 Hz) - Do not exceed
- **CRITICAL**: SPI full-duplex behavior - first byte from slave is dummy (concurrent with command)
- Handle `ioctl()` errors gracefully
- Apply escape encoding to all transmitted data using `rebear::encode()`
- Apply escape decoding to all received data using `rebear::decode()`
- Skip dummy first byte when reading responses (actual data starts after command transmission)
- Note: EscapeCodec is implemented as free functions, not a class member

### 1.6 PatchManager Class

**Priority**: MEDIUM - High-level convenience

**Files**:
- [`lib/include/rebear/patch_manager.h`](lib/include/rebear/patch_manager.h)
- [`lib/src/patch_manager.cpp`](lib/src/patch_manager.cpp)

**Key Features**:
```cpp
class PatchManager {
public:
    // Add patch (validates ID uniqueness)
    bool addPatch(const Patch& patch);
    
    // Remove patch by ID
    bool removePatch(uint8_t id);
    
    // Get all patches
    std::vector<Patch> getPatches() const;
    
    // Apply all patches to FPGA
    bool applyAll(SPIProtocol& spi);
    
    // Clear all patches (local and FPGA)
    bool clearAll(SPIProtocol& spi);
    
    // Save/load from JSON file
    bool saveToFile(const std::string& filename) const;
    bool loadFromFile(const std::string& filename);
    
private:
    std::map<uint8_t, Patch> patches_;
};
```

**File Format** (JSON):
```json
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
```

### 1.7 GPIOControl Class

**Priority**: HIGH - Required for button control and efficient monitoring

**Files**:
- [`lib/include/rebear/gpio_control.h`](lib/include/rebear/gpio_control.h)
- [`lib/src/gpio_control.cpp`](lib/src/gpio_control.cpp)

**Key Features**:
```cpp
class GPIOControl {
public:
    enum class Direction {
        Input,
        Output
    };
    
    enum class Edge {
        None,
        Rising,
        Falling,
        Both
    };
    
    GPIOControl(int pin, Direction dir);
    ~GPIOControl();
    
    // Initialize GPIO
    bool init();
    
    // Close GPIO
    void close();
    
    // For output pins (GPIO 3 - Button Control)
    bool write(bool value);
    bool read() const;  // Read back output state
    
    // For input pins (GPIO 4 - Buffer Ready)
    bool readInput() const;
    
    // Interrupt-driven input
    bool setEdge(Edge edge);
    bool waitForEdge(int timeout_ms);
    
    // Status
    bool isOpen() const;
    std::string getLastError() const;
    
private:
    int pin_;
    Direction direction_;
    int fd_;
    bool is_open_;
    std::string lastError_;
};
```

**Button Control Helper Class**:
```cpp
class ButtonControl {
public:
    ButtonControl(int gpio_pin = 3);
    ~ButtonControl();
    
    // Initialize button control
    bool init();
    
    // Press button (set GPIO high)
    bool press();
    
    // Release button (set GPIO low)
    bool release();
    
    // Complete button click with configurable duration
    // Default 100ms - adjust if needed
    bool click(int duration_ms = 100);
    
    // Get current button state
    bool isPressed() const;
    
private:
    std::unique_ptr<GPIOControl> gpio_;
    bool pressed_;
};
```

**Buffer Ready Monitor Class**:
```cpp
class BufferReadyMonitor {
public:
    BufferReadyMonitor(int gpio_pin = 4);
    ~BufferReadyMonitor();
    
    // Initialize buffer ready monitoring
    bool init();
    
    // Check if buffer has data
    bool isReady() const;
    
    // Wait for buffer ready (blocking)
    bool waitReady(int timeout_ms = 1000);
    
    // Set up interrupt-driven callback
    bool setCallback(std::function<void()> callback);
    
private:
    std::unique_ptr<GPIOControl> gpio_;
    std::function<void()> callback_;
};
```

**Implementation Notes**:
- Use Linux GPIO character device interface (`/dev/gpiochip0`)
- Fall back to sysfs interface (`/sys/class/gpio`) if character device unavailable
- Button press duration: 100ms default (configurable if needed)
- Buffer ready signal eliminates need for dummy SPI reads (0xFF responses)
- Handle GPIO permissions (user must be in `gpio` group)

**Test Cases**:
- Initialize GPIO 3 as output
- Press and release button
- Complete button click with 100ms duration
- Initialize GPIO 4 as input
- Read buffer ready state
- Wait for buffer ready with timeout
- Test interrupt-driven buffer ready callback

## Phase 2: Command-Line Utility

**IMPORTANT NOTE**: The CLI implementation uses `std::atomic<bool>` for signal handling. While `volatile sig_atomic_t` is the traditional C approach, C++ provides better alternatives. Use `std::atomic` with appropriate memory ordering for signal handlers and any shared state between threads or signal handlers.

### 2.1 CLI Framework

**Priority**: HIGH - Enables testing without GUI

**Files**:
- [`cli/main.cpp`](cli/main.cpp)
- [`cli/commands/monitor.cpp`](cli/commands/monitor.cpp)
- [`cli/commands/patch.cpp`](cli/commands/patch.cpp)
- [`cli/commands/export.cpp`](cli/commands/export.cpp)

**Command Structure**:
```bash
rebear-cli <command> [options]

Commands:
  monitor     Monitor transactions in real-time
  patch       Manage patches (set, list, clear)
  button      Control teddy bear button (press, release, click)
  export      Export transaction log
  clear       Clear transaction buffer
  help        Show help
```

### 2.2 Monitor Command

**Usage**:
```bash
rebear-cli monitor [--device /dev/spidev0.0] [--duration 30] [--format text|json]
```

**Features**:
- Real-time transaction display
- Configurable duration or continuous (CTRL+C to stop)
- Text or JSON output
- Statistics summary (total transactions, address ranges)

**Output Example** (text):
```
Time(ms)  Address    Count  
--------  ---------  -----
0.000     0x001000   256
0.015     0x001100   128
0.032     0x002000   512
...
```

**Output Example** (JSON):
```json
{
  "transactions": [
    {"timestamp": 0, "address": "0x001000", "count": 256},
    {"timestamp": 15, "address": "0x001100", "count": 128}
  ],
  "statistics": {
    "total": 2,
    "duration_ms": 32,
    "address_range": {"min": "0x001000", "max": "0x002000"}
  }
}
```

### 2.3 Patch Command

**Usage**:
```bash
# Set a patch
rebear-cli patch set --id 0 --address 0x001000 --data "0102030405060708"

# List patches
rebear-cli patch list [--format text|json]

# Clear specific patch
rebear-cli patch clear --id 0

# Clear all patches
rebear-cli patch clear --all

# Load from file
rebear-cli patch load --file patches.json

# Save to file
rebear-cli patch save --file patches.json
```

### 2.4 Button Command

**Usage**:
```bash
# Press button (set GPIO 3 high)
rebear-cli button press

# Release button (set GPIO 3 low)
rebear-cli button release

# Complete button click (press, wait, release)
rebear-cli button click [--duration 100]

# Check button status
rebear-cli button status
```

**Features**:
- Direct control of teddy bear button via GPIO 3
- Configurable press duration (default 100ms)
- Status check to verify GPIO state
- Useful for automated testing and scripting

**Example Workflow**:
```bash
# Clear old transactions
rebear-cli clear

# Press button to start playback
rebear-cli button click

# Monitor what the MCU reads
rebear-cli monitor --duration 10

# Press button again to skip to next story
rebear-cli button click
rebear-cli monitor --duration 10
```

### 2.5 Export Command

**Usage**:
```bash
rebear-cli export --output transactions.csv [--format csv|json]
```

**CSV Format**:
```csv
timestamp_ms,address,count
0,0x001000,256
15,0x001100,128
```

## Phase 3: Qt GUI Application

### 3.1 Main Window

**Priority**: HIGH - Core UI framework

**Files**:
- [`gui/main.cpp`](gui/main.cpp)
- [`gui/mainwindow.h`](gui/mainwindow.h)
- [`gui/mainwindow.cpp`](gui/mainwindow.cpp)
- [`gui/mainwindow.ui`](gui/mainwindow.ui)

**Layout**:
- Menu bar (File, Edit, View, Tools, Help)
- Toolbar (Connect, Disconnect, Clear, Export)
- Status bar (connection status, transaction count)
- Central widget with splitters for flexible layout

**Key Signals/Slots**:
```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
    
public slots:
    void onConnect();
    void onDisconnect();
    void onClearTransactions();
    void onExport();
    void onNewTransaction(const Transaction& trans);
    void onPatchApplied(uint8_t id);
    
    // Button control slots
    void onButtonPress();
    void onButtonRelease();
    void onButtonClick();
    
    // Buffer ready monitoring
    void onBufferReadyChanged(bool ready);
    
signals:
    void connected();
    void disconnected();
    void transactionReceived(const Transaction& trans);
    void patchApplied(uint8_t id);
    void buttonPressed();
    void buttonReleased();
    void bufferReadyChanged(bool ready);
    
private:
    SPIProtocol* spi_;
    ButtonControl* buttonControl_;
    BufferReadyMonitor* bufferMonitor_;
    QTimer* pollTimer_;
    TransactionViewer* transactionViewer_;
    PatchEditor* patchEditor_;
    AddressVisualizer* addressVisualizer_;
    ButtonControlWidget* buttonWidget_;
};
```

### 3.2 TransactionViewer Widget

**Priority**: HIGH - Core monitoring feature

**Files**:
- [`gui/widgets/transaction_viewer.h`](gui/widgets/transaction_viewer.h)
- [`gui/widgets/transaction_viewer.cpp`](gui/widgets/transaction_viewer.cpp)

**Features**:
- QTableView with custom model
- Columns: Timestamp, Address, Count
- Sortable columns
- Color coding by address range
- Auto-scroll option
- Search/filter
- Export selection

**Model**:
```cpp
class TransactionModel : public QAbstractTableModel {
    Q_OBJECT
    
public:
    void addTransaction(const Transaction& trans);
    void clear();
    
    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, 
                       int role) const override;
    
private:
    std::vector<Transaction> transactions_;
};
```

### 3.3 PatchEditor Widget

**Priority**: HIGH - Core patching feature

**Files**:
- [`gui/widgets/patch_editor.h`](gui/widgets/patch_editor.h)
- [`gui/widgets/patch_editor.cpp`](gui/widgets/patch_editor.cpp)

**Features**:
- QTableView with custom model
- Columns: ID, Address, Data (hex), Status
- Add/Edit/Remove buttons
- Hex editor dialog for patch data
- Enable/disable individual patches
- Apply all patches button
- Import/export patch sets

**Dialogs**:
- Add/Edit Patch Dialog:
  - ID spin box (0-15)
  - Address line edit (hex input with validation)
  - Data hex editor (8 bytes)
  - OK/Cancel buttons

### 3.4 AddressVisualizer Widget

**Priority**: MEDIUM - Enhanced analysis feature

**Files**:
- [`gui/widgets/address_visualizer.h`](gui/widgets/address_visualizer.h)
- [`gui/widgets/address_visualizer.cpp`](gui/widgets/address_visualizer.cpp)

**Features**:
- Custom QWidget with paintEvent
- Heat map of Flash memory access
- Configurable address range (zoom)
- Click to zoom into region
- Highlight patched addresses
- Statistics overlay

**Visualization**:
- Divide Flash into blocks (e.g., 4KB each)
- Color intensity based on access frequency
- Red markers for patched addresses
- Tooltip showing address and access count

**Implementation**:
```cpp
class AddressVisualizer : public QWidget {
    Q_OBJECT
    
public:
    void addTransaction(const Transaction& trans);
    void addPatch(const Patch& patch);
    void setAddressRange(uint32_t start, uint32_t end);
    void clear();
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    
private:
    struct Block {
        uint32_t address;
        uint32_t accessCount;
        bool hasPatches;
    };
    
    std::vector<Block> blocks_;
    uint32_t rangeStart_;
    uint32_t rangeEnd_;
    uint32_t blockSize_;
};
```

### 3.5 ButtonControlWidget

**Priority**: HIGH - Essential for teddy bear control

**Files**:
- [`gui/widgets/button_control_widget.h`](gui/widgets/button_control_widget.h)
- [`gui/widgets/button_control_widget.cpp`](gui/widgets/button_control_widget.cpp)

**Features**:
- Press button (set GPIO 3 high)
- Release button (set GPIO 3 low)
- Click button (press, wait, release)
- Configurable press duration
- Visual feedback (button state indicator)
- Status display (pressed/released)

**Implementation**:
```cpp
class ButtonControlWidget : public QWidget {
    Q_OBJECT
    
public:
    ButtonControlWidget(ButtonControl* buttonControl, QWidget* parent = nullptr);
    
public slots:
    void onPressClicked();
    void onReleaseClicked();
    void onClickClicked();
    void onDurationChanged(int duration);
    void updateButtonState(bool pressed);
    
signals:
    void buttonPressed();
    void buttonReleased();
    void buttonClicked(int duration);
    
private:
    ButtonControl* buttonControl_;
    QPushButton* btnPress_;
    QPushButton* btnRelease_;
    QPushButton* btnClick_;
    QSpinBox* spinDuration_;
    QLabel* lblStatus_;
    QLabel* lblIndicator_;  // Visual LED-style indicator
};
```

**Layout**:
```
┌─────────────────────────────────┐
│ Button Control                  │
├─────────────────────────────────┤
│ [Press] [Release] [Click]       │
│                                 │
│ Duration: [100] ms              │
│                                 │
│ Status: ● Released              │
└─────────────────────────────────┘
```

### 3.6 BufferReadyIndicator

**Priority**: MEDIUM - Useful for monitoring

**Files**:
- [`gui/widgets/buffer_ready_indicator.h`](gui/widgets/buffer_ready_indicator.h)
- [`gui/widgets/buffer_ready_indicator.cpp`](gui/widgets/buffer_ready_indicator.cpp)

**Features**:
- LED-style visual indicator
- Shows buffer ready state (GPIO 4)
- Green when data available
- Gray when buffer empty
- Tooltip with status text

**Implementation**:
```cpp
class BufferReadyIndicator : public QWidget {
    Q_OBJECT
    
public:
    BufferReadyIndicator(QWidget* parent = nullptr);
    
public slots:
    void setReady(bool ready);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    
private:
    bool ready_;
};
```

## Phase 4: Documentation

### 4.1 README.md

**Content**:
- Project overview
- Hardware requirements
- Software dependencies
- Quick start guide
- Building instructions
- Basic usage examples
- License information

### 4.2 Protocol Documentation

**File**: [`docs/protocol.md`](docs/protocol.md)

**Content**:
- Detailed SPI protocol specification
- Escape sequence explanation
- Command reference
- Timing diagrams
- Example transactions

### 4.3 Usage Guide

**File**: [`docs/usage.md`](docs/usage.md)

**Content**:
- CLI command reference with examples
- GUI user guide with screenshots
- Common workflows
- Troubleshooting
- Tips and tricks

### 4.4 API Documentation

**File**: [`docs/api.md`](docs/api.md)

**Content**:
- Library API reference
- Class descriptions
- Method signatures
- Usage examples
- Integration guide

## Phase 5: Testing and Validation

### 5.1 Unit Tests

**Test Framework**: Google Test or Catch2

**Test Files**:
- [`tests/test_escape_codec.cpp`](tests/test_escape_codec.cpp)
- [`tests/test_transaction.cpp`](tests/test_transaction.cpp)
- [`tests/test_patch.cpp`](tests/test_patch.cpp)

**Key Test Cases**:
- EscapeCodec: All edge cases (0x4a, 0x4d, empty, large data)
- Transaction: Parsing, validation, serialization
- Patch: Validation, serialization, JSON I/O

### 5.2 Integration Tests

**Test Scenarios**:
- Mock SPI device (loopback or simulated FPGA)
- Full workflow: connect, monitor, patch, clear
- Error handling: device not found, permission denied
- Performance: high transaction rate handling

### 5.3 Hardware Testing

**Test Plan**:
1. Connect to actual FPGA
2. Verify transaction capture
3. Apply simple patch (e.g., change one byte)
4. Observe MCU behavior change
5. Clear patch and verify restoration
6. Test multiple simultaneous patches
7. Stress test with high transaction rate

## Phase 6: Deployment

### 6.1 Raspberry Pi Setup Script

**File**: [`scripts/setup_rpi.sh`](scripts/setup_rpi.sh)

**Actions**:
- Enable SPI interface
- Install dependencies
- Add user to spi group
- Configure permissions
- Build and install software

### 6.2 Cross-Compilation

**Toolchain**: Raspberry Pi cross-compiler

**CMake Configuration**:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/rpi-toolchain.cmake ..
```

### 6.3 Packaging

**Formats**:
- Debian package (.deb) for easy installation
- Tarball with install script
- Docker image (optional)

## Implementation Checklist

### Core Library
- [ ] EscapeCodec implementation and tests
- [ ] Transaction class implementation
- [ ] Patch class implementation
- [ ] SPIProtocol implementation
- [ ] PatchManager implementation
- [ ] Library CMakeLists.txt
- [ ] Install targets and pkg-config

### CLI Utility
- [ ] Main program structure
- [ ] Monitor command
- [ ] Patch commands
- [ ] Export command
- [ ] CLI CMakeLists.txt

### GUI Application
- [ ] Main window and UI
- [ ] TransactionViewer widget
- [ ] PatchEditor widget
- [ ] AddressVisualizer widget
- [ ] GUI CMakeLists.txt

### Documentation
- [ ] README.md
- [ ] docs/protocol.md
- [ ] docs/usage.md
- [ ] docs/api.md
- [ ] Code comments

### Testing
- [ ] Unit tests
- [ ] Integration tests
- [ ] Hardware validation

### Deployment
- [ ] Raspberry Pi setup script
- [ ] Cross-compilation support
- [ ] Packaging

## Estimated Complexity

**Core Library**: Medium complexity
- EscapeCodec: Simple
- Transaction/Patch: Simple
- SPIProtocol: Medium (Linux spidev interface)
- PatchManager: Simple

**CLI Utility**: Low complexity
- Straightforward command parsing and execution

**GUI Application**: Medium-High complexity
- Qt framework learning curve
- Custom widgets require careful design
- Real-time updates need proper threading

**Documentation**: Medium effort
- Comprehensive documentation takes time
- Screenshots and diagrams needed

## Development Tips

1. **Start with EscapeCodec**: Get this right first, as everything depends on it
2. **Test incrementally**: Don't wait until everything is done to test
3. **Use mock data**: Create test data files to simulate FPGA responses
4. **Log everything**: Add verbose logging for debugging
5. **Handle errors gracefully**: SPI communication can be flaky
6. **Keep GUI responsive**: Use QTimer for polling, not blocking calls
7. **Document as you go**: Don't leave documentation for the end
8. **Version control**: Commit frequently with clear messages
9. **Code review**: Have someone review the SPI protocol implementation
10. **Test on target**: Test on actual Raspberry Pi early and often
