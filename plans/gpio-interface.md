# GPIO Interface Documentation

## Overview

In addition to the SPI communication with the FPGA, the system uses two GPIO pins on the Raspberry Pi for additional control and status monitoring:

- **GPIO 3**: Button control (output from Pi)
- **GPIO 4**: Buffer ready signal (input to Pi)

## GPIO Pin Assignments

| GPIO Pin | Direction | Function | Active State |
|----------|-----------|----------|--------------|
| GPIO 3 | Output | Teddy bear button control | High = Pressed |
| GPIO 4 | Input | FPGA buffer ready signal | High = Data available |

## GPIO 3: Button Control

### Purpose

GPIO 3 is hardwired to the teddy bear's playback control button. This allows the Raspberry Pi to programmatically control the teddy bear's behavior without physical button presses.

### Behavior

**Button Functionality:**
- First press: Stops current playback
- Second press: Resumes with next story

**GPIO Control:**
- Write `1` (HIGH): Simulates button press
- Write `0` (LOW): Simulates button release

### Usage Pattern

To simulate a button press:
```
1. Set GPIO 3 to HIGH (1)
2. Wait 100ms (recommended duration)
3. Set GPIO 3 to LOW (0)
4. Wait for MCU to respond
```

**Note**: 100ms press duration is generally sufficient. Adjust if needed based on testing.

### Implementation Example

```cpp
class ButtonControl {
public:
    ButtonControl(int gpio_pin = 3);
    ~ButtonControl();
    
    // Initialize GPIO
    bool init();
    
    // Simulate button press
    bool press();
    
    // Simulate button release
    bool release();
    
    // Press and release (complete button click)
    // Default 100ms - adjust if needed
    bool click(int duration_ms = 100);
    
private:
    int gpio_pin_;
    int fd_;
    bool is_open_;
};
```

### Use Cases

1. **Automated Testing**
   - Apply patch
   - Trigger button press
   - Observe MCU behavior change
   - Verify patch effectiveness

2. **Synchronized Analysis**
   - Press button to start playback
   - Monitor Flash transactions
   - Identify which addresses are accessed during playback

3. **Story Navigation**
   - Programmatically skip through stories
   - Map story boundaries in Flash memory
   - Build story index

### Timing Considerations

- **Recommended Press Duration**: 100ms (default, adjust if needed)
- **Minimum Press Duration**: 50ms (may work, but 100ms is safer)
- **Debounce Time**: Not applicable (handled by teddy bear MCU)
- **Inter-press Delay**: 200-500ms between consecutive presses (allow MCU to process)
- **MCU Response Time**: Variable (10-500ms depending on current state)

### Safety Notes

- Do not press button while MCU is in critical operation
- Excessive rapid pressing may confuse MCU state machine
- Always release button (set to LOW) after press

## GPIO 4: Buffer Ready Signal

### Purpose

GPIO 4 is connected to the FPGA's buffer ready signal. This signal indicates when the FPGA's transaction buffer contains data ready to be read.

### Behavior

**Signal States:**
- `HIGH (1)`: Transaction buffer has data available
- `LOW (0)`: Transaction buffer is empty

### Advantages

**Efficient Polling:**
- Eliminates unnecessary SPI reads when buffer is empty
- Reduces CPU usage
- Prevents reading dummy data (0xFF 0xFF ...)

**Event-Driven Architecture:**
- Can use GPIO interrupt to trigger reads
- More responsive than fixed-rate polling
- Better for real-time monitoring

### Implementation Example

```cpp
class BufferReadyMonitor {
public:
    BufferReadyMonitor(int gpio_pin = 4);
    ~BufferReadyMonitor();
    
    // Initialize GPIO as input
    bool init();
    
    // Check if buffer has data
    bool isReady() const;
    
    // Wait for buffer ready (blocking)
    bool waitReady(int timeout_ms = 1000);
    
    // Set up interrupt-driven callback
    bool setCallback(std::function<void()> callback);
    
private:
    int gpio_pin_;
    int fd_;
    bool is_open_;
    std::function<void()> callback_;
};
```

### Usage Patterns

#### Pattern 1: Polling with Ready Check

```cpp
SPIProtocol spi;
BufferReadyMonitor ready(4);

while (monitoring) {
    if (ready.isReady()) {
        auto trans = spi.readTransaction();
        if (trans) {
            processTransaction(*trans);
        }
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
```

#### Pattern 2: Interrupt-Driven

```cpp
BufferReadyMonitor ready(4);
SPIProtocol spi;

ready.setCallback([&spi]() {
    // Called when GPIO 4 goes HIGH
    auto trans = spi.readTransaction();
    if (trans) {
        processTransaction(*trans);
    }
});

// Main thread can do other work
while (monitoring) {
    // Handle other tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

#### Pattern 3: Blocking Wait

```cpp
SPIProtocol spi;
BufferReadyMonitor ready(4);

while (monitoring) {
    // Block until data available or timeout
    if (ready.waitReady(1000)) {
        auto trans = spi.readTransaction();
        if (trans) {
            processTransaction(*trans);
        }
    }
}
```

### Dummy Read Behavior

When buffer is empty and you read anyway:
```
Command: 0x01 (Read Transaction)
Response: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF

Decoded Transaction:
  Address: 0xFFFFFF (invalid)
  Count: 0xFFFFFF (invalid)
  Timestamp: 0xFFFF (invalid)
```

**Detection:**
```cpp
auto trans = spi.readTransaction();
if (trans && trans->address == 0xFFFFFF) {
    // Buffer was empty, ignore this transaction
    continue;
}
```

### Performance Impact

**Without GPIO 4 monitoring:**
- Poll at fixed rate (e.g., 100 Hz)
- Many wasted SPI reads when buffer empty
- Higher CPU usage
- Increased SPI bus traffic

**With GPIO 4 monitoring:**
- Read only when data available
- Minimal CPU usage when idle
- Reduced SPI bus traffic
- Faster response to new transactions

### Timing Characteristics

- **Signal Propagation Delay**: < 1μs
- **GPIO Read Latency**: ~1-10μs
- **Interrupt Latency**: ~10-100μs (Linux kernel dependent)

## Linux GPIO Interface

### sysfs Method (Simple)

```bash
# Export GPIO pins
echo 3 > /sys/class/gpio/export
echo 4 > /sys/class/gpio/export

# Set GPIO 3 as output
echo out > /sys/class/gpio/gpio3/direction

# Set GPIO 4 as input
echo in > /sys/class/gpio/gpio4/direction

# Write to GPIO 3
echo 1 > /sys/class/gpio/gpio3/value  # Press button
echo 0 > /sys/class/gpio/gpio3/value  # Release button

# Read from GPIO 4
cat /sys/class/gpio/gpio4/value  # Returns 0 or 1
```

### Character Device Method (Recommended)

Modern Linux kernels use `/dev/gpiochip*` character devices:

```cpp
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

// Open GPIO chip
int chip_fd = open("/dev/gpiochip0", O_RDONLY);

// Request GPIO line
struct gpiohandle_request req;
req.lineoffsets[0] = 3;  // GPIO 3
req.flags = GPIOHANDLE_REQUEST_OUTPUT;
strcpy(req.consumer_label, "rebear-button");
req.lines = 1;

ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
int line_fd = req.fd;

// Set value
struct gpiohandle_data data;
data.values[0] = 1;  // HIGH
ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
```

### Library Integration

The core library should provide a `GPIOControl` class:

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
    
    bool init();
    void close();
    
    // For output pins
    bool write(bool value);
    
    // For input pins
    bool read() const;
    
    // For interrupt-driven input
    bool setEdge(Edge edge);
    bool waitForEdge(int timeout_ms);
    
private:
    int pin_;
    Direction direction_;
    int fd_;
    bool is_open_;
};
```

## Integration with Main Application

### SPIProtocol Enhancement

Add GPIO support to [`SPIProtocol`](lib/include/rebear/spi_protocol.h):

```cpp
class SPIProtocol {
public:
    // ... existing methods ...
    
    // GPIO control
    bool initGPIO();
    bool pressButton();
    bool releaseButton();
    bool clickButton(int duration_ms = 100);
    bool isBufferReady() const;
    
private:
    std::unique_ptr<GPIOControl> button_;      // GPIO 3
    std::unique_ptr<GPIOControl> bufferReady_; // GPIO 4
};
```

### CLI Commands

Add GPIO commands to CLI utility:

```bash
# Button control
rebear-cli button press
rebear-cli button release
rebear-cli button click [--duration 100]

# Buffer status
rebear-cli status buffer
```

### GUI Integration

Add button control to GUI:

```cpp
class MainWindow : public QMainWindow {
    // ...
    
private slots:
    void onButtonPress();
    void onButtonRelease();
    void onButtonClick();
    void onBufferReadyChanged(bool ready);
    
private:
    QPushButton* btnPress_;
    QPushButton* btnRelease_;
    QPushButton* btnClick_;
    QLabel* lblBufferStatus_;
    QTimer* bufferPollTimer_;
};
```

**GUI Elements:**
- Button group with Press/Release/Click buttons
- Buffer status indicator (LED-style widget)
- Auto-read checkbox (read when buffer ready)

## Testing Procedures

### Test 1: Button Control

```
1. Connect to FPGA
2. Press button via GPIO 3
3. Observe teddy bear stops playback
4. Press button again
5. Observe teddy bear starts next story
6. Monitor Flash transactions during playback
```

### Test 2: Buffer Ready Signal

```
1. Clear transaction buffer
2. Verify GPIO 4 is LOW
3. Trigger MCU activity (button press)
4. Verify GPIO 4 goes HIGH
5. Read transaction via SPI
6. Verify GPIO 4 goes LOW (if buffer now empty)
```

### Test 3: Efficient Monitoring

```
1. Start monitoring with GPIO 4 check
2. Measure CPU usage
3. Compare with fixed-rate polling
4. Verify no transactions missed
5. Verify no dummy reads (0xFF...)
```

## Troubleshooting

### GPIO 3 (Button) Issues

**Problem**: Button press has no effect
- Check GPIO direction (should be output)
- Verify wiring to teddy bear button
- Check voltage levels (3.3V logic)
- Ensure sufficient press duration (>50ms)

**Problem**: MCU behaves erratically
- Reduce press frequency
- Increase inter-press delay
- Check for electrical noise
- Verify proper button release

### GPIO 4 (Buffer Ready) Issues

**Problem**: Signal always LOW
- Check GPIO direction (should be input)
- Verify FPGA connection
- Check FPGA firmware (ready signal implementation)
- Test with known transaction generation

**Problem**: Signal always HIGH
- Check if buffer is actually full
- Verify SPI reads are working
- Check FPGA buffer clear logic
- Test with manual buffer clear command

**Problem**: Missed transactions
- Reduce polling interval
- Use interrupt-driven approach
- Check for race conditions
- Verify FPGA buffer size

## Hardware Wiring

### Raspberry Pi GPIO Pinout

```
Pi GPIO Header (partial):
┌─────┬─────┐
│ 3V3 │ 5V  │
├─────┼─────┤
│ GP2 │ 5V  │
├─────┼─────┤
│ GP3 │ GND │  ← GPIO 3 (Button Control)
├─────┼─────┤
│ GP4 │ GP14│  ← GPIO 4 (Buffer Ready)
├─────┼─────┤
│ GND │ GP15│
└─────┴─────┘
```

### Connection Diagram

```
Raspberry Pi GPIO 3 ──────► Teddy Bear Button Input
                             (Parallel with physical button)

FPGA Buffer Ready ──────► Raspberry Pi GPIO 4
                          (3.3V logic level)
```

### Electrical Specifications

- **Logic Level**: 3.3V (Raspberry Pi GPIO standard)
- **Maximum Current**: 16mA per pin
- **Input Impedance**: ~50kΩ (with pull-up/pull-down)
- **Output Drive**: Push-pull or open-drain

## Best Practices

1. **Always initialize GPIOs before use**
   - Set direction correctly
   - Configure pull-up/pull-down if needed

2. **Clean up on exit**
   - Release button (set GPIO 3 to LOW)
   - Unexport GPIOs
   - Close file descriptors

3. **Handle errors gracefully**
   - Check return values
   - Log GPIO errors
   - Provide user feedback

4. **Use buffer ready signal**
   - Reduces unnecessary SPI traffic
   - Improves efficiency
   - Enables event-driven architecture

5. **Debounce button presses**
   - Minimum 50ms press duration
   - Wait for MCU to process
   - Avoid rapid consecutive presses

6. **Monitor buffer ready in real-time**
   - Use interrupts for best responsiveness
   - Fall back to polling if interrupts unavailable
   - Check signal before every SPI read

## Security Considerations

- GPIO access requires appropriate permissions
- Add user to `gpio` group: `sudo usermod -a -G gpio username`
- Or use `sudo` (not recommended for production)
- Protect against accidental button presses during critical operations
- Validate GPIO state before operations

## Performance Metrics

**Without GPIO 4 monitoring:**
- SPI reads per second: ~100 (fixed polling)
- Dummy reads: ~90% (when idle)
- CPU usage: ~5-10%

**With GPIO 4 monitoring:**
- SPI reads per second: Variable (only when needed)
- Dummy reads: 0%
- CPU usage: ~1-2%
- Response time: <10ms (interrupt-driven)

## Future Enhancements

1. **Additional GPIO Signals**
   - FPGA reset line
   - Status LEDs
   - Additional button inputs

2. **Advanced Button Control**
   - Long press detection
   - Double-click support
   - Button state machine

3. **Buffer Management**
   - Buffer overflow detection
   - Buffer usage statistics
   - Automatic buffer clear on overflow

4. **Power Management**
   - GPIO-based power control
   - Sleep mode coordination
   - Wake-on-interrupt
