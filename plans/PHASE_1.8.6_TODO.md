# Phase 1.8.6 TODO: CLI/GUI Network Integration

## Status: IN PROGRESS

Phase 1.8.6 aims to integrate network virtualization support into the CLI and GUI applications. The core infrastructure (phases 1.8.1-1.8.5) is complete. This document outlines the remaining work.

## Overview

The network classes ([`SPIProtocolNetwork`](lib/include/rebear/spi_protocol_network.h), [`GPIOControlNetwork`](lib/include/rebear/gpio_control_network.h)) provide the same interface as their local counterparts but don't inherit from them. This requires using templates or conditional logic to support both modes.

## CLI Integration

### Completed ✅

1. **Global `--remote` flag parsing** ([`main()`](cli/main.cpp:714))
   - Parses `--remote <host[:port]>` before command dispatch
   - Sets global variables: `g_use_network`, `g_remote_host`, `g_remote_port`

2. **Connection string parser** ([`parseRemoteString()`](cli/main.cpp:42))
   - Supports formats: `hostname:port`, `tcp://hostname:port`, `hostname`
   - Defaults to port 9876

3. **Updated help text** ([`printHelp()`](cli/main.cpp:697))
   - Documents `--remote` flag
   - Provides network mode examples

4. **Monitor command template** ([`cmdMonitorImpl<>`](cli/main.cpp:104))
   - Template function works with both `SPIProtocol` and `SPIProtocolNetwork`
   - [`cmdMonitor()`](cli/main.cpp:203) branches on `g_use_network` and calls template

### Remaining Work 🔨

The following commands need the same template treatment as `cmdMonitor`:

#### 1. `cmdPatch` - Patch Management

**Current state:** Uses `createSPIProtocol()` helper (removed, doesn't work)

**Required changes:**
```cpp
// Create template implementation
template<typename SPIType>
int cmdPatchImpl(SPIType& spi, const std::string& subcommand, /* other args */) {
    // Move existing logic here
    // Works with both SPIProtocol and SPIProtocolNetwork
}

// Update cmdPatch to branch
int cmdPatch(int argc, char* argv[]) {
    // Parse arguments
    if (g_use_network) {
        rebear::SPIProtocolNetwork spi(g_remote_host, g_remote_port);
        if (!spi.open(device, speed)) { /* error */ }
        return cmdPatchImpl(spi, subcommand, /* args */);
    } else {
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) { /* error */ }
        return cmdPatchImpl(spi, subcommand, /* args */);
    }
}
```

**Subcommands to update:**
- `patch set` - Apply new patch
- `patch list` - List active patches
- `patch clear` - Remove patches
- `patch load` - Load from JSON file
- `patch save` - Save to JSON file

**Files:** [`cli/main.cpp:334-490`](cli/main.cpp:334)

#### 2. `cmdButton` - GPIO Button Control

**Current state:** Uses `createGPIOControl()` helper (removed, doesn't work)

**Required changes:**
```cpp
// Create template implementation
template<typename GPIOType>
int cmdButtonImpl(GPIOType& gpio, const std::string& subcommand, int duration) {
    // Move existing logic here
    // Works with both GPIOControl and GPIOControlNetwork
}

// Update cmdButton to branch
int cmdButton(int argc, char* argv[]) {
    // Parse arguments
    if (g_use_network) {
        rebear::GPIOControlNetwork gpio(3, rebear::GPIOControl::Direction::Output,
                                        g_remote_host, g_remote_port);
        if (!gpio.init()) { /* error */ }
        return cmdButtonImpl(gpio, subcommand, duration);
    } else {
        rebear::GPIOControl gpio(3, rebear::GPIOControl::Direction::Output);
        if (!gpio.init()) { /* error */ }
        return cmdButtonImpl(gpio, subcommand, duration);
    }
}
```

**Subcommands to update:**
- `button press` - Set GPIO 3 HIGH
- `button release` - Set GPIO 3 LOW
- `button click` - Complete button click with duration
- `button status` - Check button state

**Files:** [`cli/main.cpp:492-553`](cli/main.cpp:492)

#### 3. `cmdExport` - Transaction Export

**Current state:** Uses `createSPIProtocol()` helper (removed, doesn't work)

**Required changes:**
```cpp
// Create template implementation
template<typename SPIType>
int cmdExportImpl(SPIType& spi, const std::string& output, 
                  const std::string& format, int duration) {
    // Move existing logic here
}

// Update cmdExport to branch
int cmdExport(int argc, char* argv[]) {
    // Parse arguments
    if (g_use_network) {
        rebear::SPIProtocolNetwork spi(g_remote_host, g_remote_port);
        if (!spi.open(device, speed)) { /* error */ }
        return cmdExportImpl(spi, output, format, duration);
    } else {
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) { /* error */ }
        return cmdExportImpl(spi, output, format, duration);
    }
}
```

**Files:** [`cli/main.cpp:555-664`](cli/main.cpp:555)

#### 4. `cmdClear` - Clear Transaction Buffer

**Current state:** Uses `createSPIProtocol()` helper (removed, doesn't work)

**Required changes:**
```cpp
// Create template implementation
template<typename SPIType>
int cmdClearImpl(SPIType& spi) {
    if (!spi.clearTransactions()) {
        std::cerr << "Error: " << spi.getLastError() << std::endl;
        return 1;
    }
    std::cout << "Transaction buffer cleared" << std::endl;
    spi.close();
    return 0;
}

// Update cmdClear to branch
int cmdClear(int argc, char* argv[]) {
    // Parse arguments
    if (g_use_network) {
        rebear::SPIProtocolNetwork spi(g_remote_host, g_remote_port);
        if (!spi.open(device, speed)) { /* error */ }
        return cmdClearImpl(spi);
    } else {
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) { /* error */ }
        return cmdClearImpl(spi);
    }
}
```

**Files:** [`cli/main.cpp:666-695`](cli/main.cpp:666)

### Testing Checklist

Once all commands are updated:

- [ ] Test `--remote` flag parsing
- [ ] Test local mode (no `--remote` flag)
- [ ] Test network mode with running rebear-server:
  - [ ] `rebear-cli --remote pi3:9876 monitor --duration 10`
  - [ ] `rebear-cli --remote tcp://192.168.1.100:9876 patch list`
  - [ ] `rebear-cli --remote raspberrypi.local button click`
  - [ ] `rebear-cli --remote pi3 export --output test.csv`
  - [ ] `rebear-cli --remote pi3 clear`
- [ ] Test error handling (server not running, invalid host, etc.)
- [ ] Test all subcommands in both modes

## GUI Integration

### Current State

The GUI ([`MainWindow`](gui/mainwindow.cpp)) currently uses local hardware access only:
- Direct instantiation of `SPIProtocol`, `ButtonControl`, `BufferReadyMonitor`
- No network mode support
- No connection dialog

### Required Changes

#### 1. Add Connection Dialog

**New files to create:**
- `gui/widgets/connection_dialog.h`
- `gui/widgets/connection_dialog.cpp`

**Features:**
- Radio buttons: "Local Hardware" / "Network (Remote Server)"
- Network fields: Hostname, Port (default 9876)
- "Remember last connection" checkbox
- Test connection button
- OK/Cancel buttons

**Example implementation:**
```cpp
class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode { Local, Network };
    
    ConnectionDialog(QWidget* parent = nullptr);
    
    Mode getMode() const;
    QString getHostname() const;
    uint16_t getPort() const;
    bool shouldRemember() const;
    
private slots:
    void onModeChanged();
    void onTestConnection();
    
private:
    QRadioButton* localRadio_;
    QRadioButton* networkRadio_;
    QLineEdit* hostnameEdit_;
    QSpinBox* portSpin_;
    QCheckBox* rememberCheck_;
    QPushButton* testButton_;
};
```

#### 2. Update MainWindow

**File:** [`gui/mainwindow.h`](gui/mainwindow.h)

**Add members:**
```cpp
private:
    // Connection mode
    bool useNetwork_;
    QString remoteHost_;
    uint16_t remotePort_;
    
    // Network objects (only used in network mode)
    std::unique_ptr<rebear::SPIProtocolNetwork> spiNetwork_;
    std::unique_ptr<rebear::GPIOControlNetwork> buttonNetwork_;
    std::unique_ptr<rebear::GPIOControlNetwork> bufferMonitorNetwork_;
```

**File:** [`gui/mainwindow.cpp`](gui/mainwindow.cpp)

**Update `onConnect()` method:**
```cpp
void MainWindow::onConnect() {
    // Show connection dialog
    ConnectionDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    useNetwork_ = (dialog.getMode() == ConnectionDialog::Mode::Network);
    
    if (useNetwork_) {
        remoteHost_ = dialog.getHostname();
        remotePort_ = dialog.getPort();
        
        // Create network objects
        spiNetwork_ = std::make_unique<rebear::SPIProtocolNetwork>(
            remoteHost_.toStdString(), remotePort_);
        
        if (!spiNetwork_->open("/dev/spidev0.0", 100000)) {
            QMessageBox::critical(this, "Connection Error",
                QString("Failed to connect: %1")
                .arg(QString::fromStdString(spiNetwork_->getLastError())));
            return;
        }
        
        // Initialize GPIO via network
        buttonNetwork_ = std::make_unique<rebear::GPIOControlNetwork>(
            3, rebear::GPIOControl::Direction::Output,
            remoteHost_.toStdString(), remotePort_);
        buttonNetwork_->init();
        
        bufferMonitorNetwork_ = std::make_unique<rebear::GPIOControlNetwork>(
            4, rebear::GPIOControl::Direction::Input,
            remoteHost_.toStdString(), remotePort_);
        bufferMonitorNetwork_->init();
        
    } else {
        // Existing local mode code
        spi_ = std::make_unique<rebear::SPIProtocol>();
        // ... etc
    }
    
    // Start polling
    pollTimer_->start(100);
    emit connected();
}
```

**Update `onPollTransactions()` method:**
```cpp
void MainWindow::onPollTransactions() {
    // Check buffer ready
    bool ready = false;
    if (useNetwork_) {
        ready = bufferMonitorNetwork_->read();
    } else {
        ready = bufferMonitor_->readInput();
    }
    
    if (!ready) {
        return;
    }
    
    // Read transaction
    std::optional<rebear::Transaction> trans;
    if (useNetwork_) {
        trans = spiNetwork_->readTransaction();
    } else {
        trans = spi_->readTransaction();
    }
    
    // Process transaction
    if (trans && trans->address != 0xFFFFFF) {
        transactionCount_++;
        emit transactionReceived(*trans);
        logMessage(QString("Transaction: 0x%1, %2 bytes")
            .arg(trans->address, 6, 16, QChar('0'))
            .arg(trans->count));
    }
}
```

**Update all other methods** that use `spi_`, `button_`, or `bufferMonitor_` to check `useNetwork_` and use the appropriate object.

#### 3. Update Menu Actions

**File:** [`gui/mainwindow.cpp`](gui/mainwindow.cpp)

**Update menu text:**
- "Connect" → "Connect..." (shows dialog)
- Add "Connection Mode" submenu with "Local" and "Network" options
- Show current mode in status bar

#### 4. Save/Restore Connection Settings

**Use QSettings to remember last connection:**
```cpp
void MainWindow::saveConnectionSettings() {
    QSettings settings("Rebear", "RebearGUI");
    settings.setValue("connection/mode", useNetwork_ ? "network" : "local");
    settings.setValue("connection/host", remoteHost_);
    settings.setValue("connection/port", remotePort_);
}

void MainWindow::loadConnectionSettings() {
    QSettings settings("Rebear", "RebearGUI");
    QString mode = settings.value("connection/mode", "local").toString();
    useNetwork_ = (mode == "network");
    remoteHost_ = settings.value("connection/host", "raspberrypi.local").toString();
    remotePort_ = settings.value("connection/port", 9876).toUInt();
}
```

### Testing Checklist

Once GUI is updated:

- [ ] Test connection dialog appearance and behavior
- [ ] Test local mode connection
- [ ] Test network mode connection with running rebear-server
- [ ] Test transaction monitoring in both modes
- [ ] Test patch management in both modes
- [ ] Test button control in both modes
- [ ] Test connection settings persistence
- [ ] Test error handling (server not running, connection lost, etc.)
- [ ] Test switching between local and network modes

## Documentation Updates

### Files to Update

1. **[`docs/CLI_USAGE.md`](docs/CLI_USAGE.md)**
   - Add `--remote` flag documentation
   - Add network mode examples
   - Add troubleshooting section for network issues

2. **[`docs/NETWORK_SETUP.md`](docs/NETWORK_SETUP.md)**
   - Add CLI usage examples
   - Add GUI usage examples
   - Add screenshots of connection dialog

3. **[`README.md`](README.md)**
   - Update usage examples to show network mode
   - Add "Remote Operation" section

4. **[`plans/PHASE_1.8_COMPLETE.md`](plans/PHASE_1.8_COMPLETE.md)**
   - Update status of phase 1.8.6
   - Mark as complete once all work is done

## Estimated Effort

- **CLI completion:** 2-3 hours
  - Template refactoring: 1-2 hours
  - Testing: 1 hour

- **GUI implementation:** 4-6 hours
  - Connection dialog: 1-2 hours
  - MainWindow updates: 2-3 hours
  - Testing: 1 hour

- **Documentation:** 1-2 hours

**Total:** 7-11 hours

## Priority

**HIGH** - This is the final piece of Phase 1.8 (Network Virtualization). Without it, the network infrastructure cannot be used by end users.

## Dependencies

- Phase 1.8.1-1.8.5 (COMPLETE) - Protocol, client library, server daemon
- Running rebear-server instance for testing

## Success Criteria

- [ ] All CLI commands work with `--remote` flag
- [ ] GUI has connection dialog for mode selection
- [ ] Both CLI and GUI work in local and network modes
- [ ] Connection settings are persisted
- [ ] Documentation is updated
- [ ] All tests pass
- [ ] Phase 1.8 marked as COMPLETE

## Notes

- The template approach is necessary because `SPIProtocolNetwork` and `GPIOControlNetwork` don't inherit from base classes
- Consider creating abstract base classes in future to simplify this
- The current approach (templates) is the cleanest solution without modifying the core library
- GUI changes are more extensive than CLI due to Qt signal/slot architecture
