#include "rebear/gpio_control.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <poll.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

namespace rebear {

// ============================================================================
// GPIOControl Implementation
// ============================================================================

GPIOControl::GPIOControl(int pin, Direction dir)
    : pin_(pin)
    , direction_(dir)
    , fd_(-1)
    , is_open_(false)
    , current_value_(false)
    , using_sysfs_(false)
{
}

GPIOControl::~GPIOControl() {
    close();
}

bool GPIOControl::init() {
    if (is_open_) {
        lastError_ = "GPIO already initialized";
        return false;
    }
    
    // Try character device first (modern method)
    if (initCharDevice()) {
        return true;
    }
    
    // Fall back to sysfs (legacy method)
    if (initSysfs()) {
        using_sysfs_ = true;
        return true;
    }
    
    return false;
}

bool GPIOControl::initCharDevice() {
    // Open GPIO chip
    int chip_fd = ::open("/dev/gpiochip0", O_RDONLY);
    if (chip_fd < 0) {
        lastError_ = "Failed to open /dev/gpiochip0: " + std::string(strerror(errno));
        return false;
    }
    
    // Request GPIO line
    struct gpiohandle_request req;
    memset(&req, 0, sizeof(req));
    req.lineoffsets[0] = pin_;
    req.lines = 1;
    
    if (direction_ == Direction::Output) {
        req.flags = GPIOHANDLE_REQUEST_OUTPUT;
        snprintf(req.consumer_label, sizeof(req.consumer_label), "rebear-gpio%d", pin_);
    } else {
        req.flags = GPIOHANDLE_REQUEST_INPUT;
        snprintf(req.consumer_label, sizeof(req.consumer_label), "rebear-gpio%d", pin_);
    }
    
    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        lastError_ = "Failed to request GPIO line: " + std::string(strerror(errno));
        ::close(chip_fd);
        return false;
    }
    
    ::close(chip_fd);
    fd_ = req.fd;
    is_open_ = true;
    return true;
}

bool GPIOControl::initSysfs() {
    // Export GPIO
    std::ofstream export_file("/sys/class/gpio/export");
    if (!export_file) {
        lastError_ = "Failed to open /sys/class/gpio/export";
        return false;
    }
    export_file << pin_;
    export_file.close();
    
    // Give system time to create files
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Set direction
    std::string gpio_path = "/sys/class/gpio/gpio" + std::to_string(pin_);
    std::ofstream dir_file(gpio_path + "/direction");
    if (!dir_file) {
        lastError_ = "Failed to set GPIO direction";
        return false;
    }
    
    if (direction_ == Direction::Output) {
        dir_file << "out";
    } else {
        dir_file << "in";
    }
    dir_file.close();
    
    // Open value file
    std::string value_path = gpio_path + "/value";
    int flags = (direction_ == Direction::Output) ? O_RDWR : O_RDONLY;
    fd_ = ::open(value_path.c_str(), flags);
    
    if (fd_ < 0) {
        lastError_ = "Failed to open GPIO value file: " + std::string(strerror(errno));
        return false;
    }
    
    is_open_ = true;
    return true;
}

void GPIOControl::close() {
    if (!is_open_) {
        return;
    }
    
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    
    if (using_sysfs_) {
        closeSysfs();
    }
    
    is_open_ = false;
}

void GPIOControl::closeSysfs() {
    // Unexport GPIO
    std::ofstream unexport_file("/sys/class/gpio/unexport");
    if (unexport_file) {
        unexport_file << pin_;
    }
}

bool GPIOControl::write(bool value) {
    if (!is_open_) {
        lastError_ = "GPIO not initialized";
        return false;
    }
    
    if (direction_ != Direction::Output) {
        lastError_ = "Cannot write to input GPIO";
        return false;
    }
    
    if (using_sysfs_) {
        // Sysfs method
        if (lseek(fd_, 0, SEEK_SET) < 0) {
            lastError_ = "Failed to seek: " + std::string(strerror(errno));
            return false;
        }
        
        const char* val_str = value ? "1" : "0";
        if (::write(fd_, val_str, 1) != 1) {
            lastError_ = "Failed to write GPIO value: " + std::string(strerror(errno));
            return false;
        }
    } else {
        // Character device method
        struct gpiohandle_data data;
        memset(&data, 0, sizeof(data));
        data.values[0] = value ? 1 : 0;
        
        if (ioctl(fd_, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0) {
            lastError_ = "Failed to set GPIO value: " + std::string(strerror(errno));
            return false;
        }
    }
    
    current_value_ = value;
    return true;
}

bool GPIOControl::read() const {
    return current_value_;
}

bool GPIOControl::readInput() const {
    if (!is_open_) {
        return false;
    }
    
    if (using_sysfs_) {
        // Sysfs method
        if (lseek(fd_, 0, SEEK_SET) < 0) {
            return false;
        }
        
        char buf[3];
        if (::read(fd_, buf, sizeof(buf)) < 1) {
            return false;
        }
        
        return (buf[0] == '1');
    } else {
        // Character device method
        struct gpiohandle_data data;
        memset(&data, 0, sizeof(data));
        
        if (ioctl(fd_, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
            return false;
        }
        
        return (data.values[0] != 0);
    }
}

bool GPIOControl::setEdge(Edge edge) {
    if (!is_open_ || !using_sysfs_) {
        lastError_ = "Edge detection only supported with sysfs interface";
        return false;
    }
    
    if (direction_ != Direction::Input) {
        lastError_ = "Edge detection only available for input pins";
        return false;
    }
    
    std::string gpio_path = "/sys/class/gpio/gpio" + std::to_string(pin_);
    std::ofstream edge_file(gpio_path + "/edge");
    if (!edge_file) {
        lastError_ = "Failed to open edge file";
        return false;
    }
    
    switch (edge) {
        case Edge::None:
            edge_file << "none";
            break;
        case Edge::Rising:
            edge_file << "rising";
            break;
        case Edge::Falling:
            edge_file << "falling";
            break;
        case Edge::Both:
            edge_file << "both";
            break;
    }
    
    return true;
}

bool GPIOControl::waitForEdge(int timeout_ms) {
    if (!is_open_) {
        lastError_ = "GPIO not initialized";
        return false;
    }
    
    if (direction_ != Direction::Input) {
        lastError_ = "waitForEdge only available for input pins";
        return false;
    }
    
    if (!using_sysfs_) {
        lastError_ = "waitForEdge only supported with sysfs interface";
        return false;
    }
    
    // Clear any pending events
    char buf[3];
    lseek(fd_, 0, SEEK_SET);
    ::read(fd_, buf, sizeof(buf));
    
    // Wait for edge event
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLPRI | POLLERR;
    pfd.revents = 0;
    
    int ret = poll(&pfd, 1, timeout_ms);
    
    if (ret < 0) {
        lastError_ = "poll() failed: " + std::string(strerror(errno));
        return false;
    }
    
    if (ret == 0) {
        // Timeout
        return false;
    }
    
    // Edge detected
    lseek(fd_, 0, SEEK_SET);
    ::read(fd_, buf, sizeof(buf));
    return true;
}

// ============================================================================
// ButtonControl Implementation
// ============================================================================

ButtonControl::ButtonControl(int gpio_pin)
    : gpio_(std::make_unique<GPIOControl>(gpio_pin, GPIOControl::Direction::Output))
    , pressed_(false)
{
}

ButtonControl::~ButtonControl() {
    if (pressed_) {
        release();
    }
}

bool ButtonControl::init() {
    if (!gpio_->init()) {
        return false;
    }
    
    // Ensure button starts in released state
    return release();
}

bool ButtonControl::press() {
    if (!gpio_->isOpen()) {
        return false;
    }
    
    if (gpio_->write(true)) {
        pressed_ = true;
        return true;
    }
    
    return false;
}

bool ButtonControl::release() {
    if (!gpio_->isOpen()) {
        return false;
    }
    
    if (gpio_->write(false)) {
        pressed_ = false;
        return true;
    }
    
    return false;
}

bool ButtonControl::click(int duration_ms) {
    if (!press()) {
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    
    return release();
}

std::string ButtonControl::getLastError() const {
    return gpio_->getLastError();
}

// ============================================================================
// BufferReadyMonitor Implementation
// ============================================================================

BufferReadyMonitor::BufferReadyMonitor(int gpio_pin)
    : gpio_(std::make_unique<GPIOControl>(gpio_pin, GPIOControl::Direction::Input))
{
}

BufferReadyMonitor::~BufferReadyMonitor() {
}

bool BufferReadyMonitor::init() {
    return gpio_->init();
}

bool BufferReadyMonitor::isReady() const {
    if (!gpio_->isOpen()) {
        return false;
    }
    
    return gpio_->readInput();
}

bool BufferReadyMonitor::waitReady(int timeout_ms) {
    if (!gpio_->isOpen()) {
        return false;
    }
    
    // Set edge detection for rising edge (buffer becomes ready)
    if (!gpio_->setEdge(GPIOControl::Edge::Rising)) {
        // If edge detection not available, fall back to polling
        auto start = std::chrono::steady_clock::now();
        while (true) {
            if (isReady()) {
                return true;
            }
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            
            if (timeout_ms > 0 && elapsed >= timeout_ms) {
                return false;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    return gpio_->waitForEdge(timeout_ms);
}

bool BufferReadyMonitor::setCallback(std::function<void()> callback) {
    callback_ = callback;
    
    if (!gpio_->isOpen()) {
        return false;
    }
    
    // Set edge detection for rising edge
    if (!gpio_->setEdge(GPIOControl::Edge::Rising)) {
        return false;
    }
    
    // Start monitoring thread
    std::thread([this]() {
        while (gpio_->isOpen() && callback_) {
            if (gpio_->waitForEdge(1000)) {
                if (callback_) {
                    callback_();
                }
            }
        }
    }).detach();
    
    return true;
}

std::string BufferReadyMonitor::getLastError() const {
    return gpio_->getLastError();
}

} // namespace rebear
