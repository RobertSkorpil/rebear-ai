#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "rebear/spi_protocol.h"
#include "rebear/patch_manager.h"
#include "rebear/gpio_control.h"
#include "rebear/transaction.h"
#include <chrono>
#include <thread>
#include <csignal>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <atomic>

// Global flag for signal handling
std::atomic<bool> g_running{true};

void signalHandler(int /* signum */) {
    g_running.store(false, std::memory_order_relaxed);
}

// Helper function to parse hex string to bytes
std::vector<uint8_t> parseHexString(const std::string& hex) {
    std::vector<uint8_t> bytes;
    if (hex.length() % 2 != 0) {
        return bytes; // Invalid hex string
    }
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
        bytes.push_back(byte);
    }
    
    return bytes;
}

// Helper function to parse hex address
uint32_t parseAddress(const std::string& addr) {
    std::string cleanAddr = addr;
    if (cleanAddr.substr(0, 2) == "0x" || cleanAddr.substr(0, 2) == "0X") {
        cleanAddr = cleanAddr.substr(2);
    }
    return std::stoul(cleanAddr, nullptr, 16);
}

// Command: monitor
int cmdMonitor(int argc, char* argv[]) {
    std::string device = "/dev/spidev0.0";
    uint32_t speed = 100000;
    int duration = -1; // -1 = continuous
    std::string format = "text";
    
    // Parse arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--device" && i + 1 < argc) {
            device = argv[++i];
        } else if (arg == "--speed" && i + 1 < argc) {
            speed = std::stoul(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration = std::stoi(argv[++i]);
        } else if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        }
    }
    
    // Connect to FPGA
    rebear::SPIProtocol spi;
    if (!spi.open(device, speed)) {
        std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
        return 1;
    }
    
    std::cout << "Connected to FPGA on " << device << std::endl;
    
    // Setup signal handler
    signal(SIGINT, signalHandler);
    
    // Statistics
    std::vector<rebear::Transaction> transactions;
    auto startTime = std::chrono::steady_clock::now();
    
    if (format == "text") {
        std::cout << "\nMonitoring transactions (Press CTRL+C to stop)...\n" << std::endl;
        std::cout << std::left << std::setw(12) << "Time(ms)" 
                  << std::setw(12) << "Address" 
                  << std::setw(10) << "Count" << std::endl;
        std::cout << std::string(34, '-') << std::endl;
    }
    
    auto monitorStart = std::chrono::steady_clock::now();
    
    while (g_running) {
        auto trans = spi.readTransaction();
        
        if (trans && trans->address != 0xFFFFFF) {
            transactions.push_back(*trans);
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - monitorStart).count();
            
            if (format == "text") {
                std::cout << std::left << std::setw(12) << elapsed
                          << "0x" << std::hex << std::setw(10) << trans->address << std::dec;
                
                // Check if count is 0xFFFFFF (FPGA actively patching)
                if (trans->count == 0xFFFFFF) {
                    std::cout << std::setw(10) << "PATCHING" << std::endl;
                } else {
                    std::cout << std::setw(10) << trans->count << std::endl;
                }
            }
        }
        
        // Check duration
        if (duration > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= duration) {
                break;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Output results
    if (format == "json") {
        std::cout << "{" << std::endl;
        std::cout << "  \"transactions\": [" << std::endl;
        
        for (size_t i = 0; i < transactions.size(); i++) {
            const auto& t = transactions[i];
            std::cout << "    {\"timestamp\": " << t.timestamp
                      << ", \"address\": \"0x" << std::hex << t.address << std::dec
                      << "\", \"count\": ";
            
            // Check if count is 0xFFFFFF (FPGA actively patching)
            if (t.count == 0xFFFFFF) {
                std::cout << "\"PATCHING\"";
            } else {
                std::cout << t.count;
            }
            
            std::cout << "}";
            if (i < transactions.size() - 1) {
                std::cout << ",";
            }
            std::cout << std::endl;
        }
        
        std::cout << "  ]," << std::endl;
        std::cout << "  \"statistics\": {" << std::endl;
        std::cout << "    \"total\": " << transactions.size() << "," << std::endl;
        
        if (!transactions.empty()) {
            uint32_t minAddr = transactions[0].address;
            uint32_t maxAddr = transactions[0].address;
            for (const auto& t : transactions) {
                if (t.address < minAddr) minAddr = t.address;
                if (t.address > maxAddr) maxAddr = t.address;
            }
            std::cout << "    \"address_range\": {\"min\": \"0x" << std::hex << minAddr 
                      << "\", \"max\": \"0x" << maxAddr << std::dec << "\"}" << std::endl;
        }
        
        std::cout << "  }" << std::endl;
        std::cout << "}" << std::endl;
    } else {
        std::cout << "\n" << std::string(34, '-') << std::endl;
        std::cout << "Total transactions: " << transactions.size() << std::endl;
    }
    
    spi.close();
    return 0;
}

// Command: patch
int cmdPatch(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: rebear-cli patch <subcommand> [options]" << std::endl;
        std::cerr << "Subcommands: set, list, clear, load, save" << std::endl;
        return 1;
    }
    
    std::string subcommand = argv[2];
    std::string device = "/dev/spidev0.0";
    uint32_t speed = 100000;
    
    rebear::PatchManager patchMgr;
    
    if (subcommand == "set") {
        // Parse arguments
        uint8_t id = 0;
        uint32_t address = 0;
        std::string dataStr;
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--id" && i + 1 < argc) {
                id = std::stoi(argv[++i]);
            } else if (arg == "--address" && i + 1 < argc) {
                address = parseAddress(argv[++i]);
            } else if (arg == "--data" && i + 1 < argc) {
                dataStr = argv[++i];
            } else if (arg == "--device" && i + 1 < argc) {
                device = argv[++i];
            }
        }
        
        if (dataStr.empty() || dataStr.length() != 16) {
            std::cerr << "Error: --data must be 16 hex characters (8 bytes)" << std::endl;
            return 1;
        }
        
        auto dataBytes = parseHexString(dataStr);
        if (dataBytes.size() != 8) {
            std::cerr << "Error: Invalid hex data" << std::endl;
            return 1;
        }
        
        rebear::Patch patch;
        patch.id = id;
        patch.address = address;
        std::copy(dataBytes.begin(), dataBytes.end(), patch.data.begin());
        patch.enabled = true;
        
        if (!patchMgr.addPatch(patch)) {
            std::cerr << "Error: " << patchMgr.getLastError() << std::endl;
            return 1;
        }
        
        // Apply to FPGA
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        
        if (!patchMgr.applyAll(spi)) {
            std::cerr << "Error: Failed to apply patch: " << patchMgr.getLastError() << std::endl;
            spi.close();
            return 1;
        }
        
        std::cout << "Patch " << static_cast<int>(id) << " set at address 0x" 
                  << std::hex << address << std::dec << std::endl;
        
        spi.close();
        
    } else if (subcommand == "list") {
        std::string format = "text";
        std::string filename;
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--format" && i + 1 < argc) {
                format = argv[++i];
            } else if (arg == "--file" && i + 1 < argc) {
                filename = argv[++i];
            }
        }
        
        if (!filename.empty()) {
            if (!patchMgr.loadFromFile(filename)) {
                std::cerr << "Error: " << patchMgr.getLastError() << std::endl;
                return 1;
            }
        }
        
        auto patches = patchMgr.getPatches();
        
        if (format == "json") {
            std::cout << "{" << std::endl;
            std::cout << "  \"patches\": [" << std::endl;
            
            for (size_t i = 0; i < patches.size(); i++) {
                const auto& p = patches[i];
                std::cout << "    {\"id\": " << static_cast<int>(p.id)
                          << ", \"address\": \"0x" << std::hex << p.address << std::dec
                          << "\", \"data\": \"";
                for (const auto& byte : p.data) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                              << static_cast<int>(byte);
                }
                std::cout << std::dec << "\", \"enabled\": " 
                          << (p.enabled ? "true" : "false") << "}";
                if (i < patches.size() - 1) {
                    std::cout << ",";
                }
                std::cout << std::endl;
            }
            
            std::cout << "  ]" << std::endl;
            std::cout << "}" << std::endl;
        } else {
            std::cout << "Active patches:" << std::endl;
            std::cout << std::left << std::setw(6) << "ID" 
                      << std::setw(12) << "Address" 
                      << std::setw(20) << "Data" 
                      << "Status" << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            
            for (const auto& p : patches) {
                std::cout << std::left << std::setw(6) << static_cast<int>(p.id)
                          << "0x" << std::hex << std::setw(10) << p.address << std::dec;
                for (const auto& byte : p.data) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                              << static_cast<int>(byte);
                }
                std::cout << std::dec << "  " << (p.enabled ? "Active" : "Disabled") << std::endl;
            }
            
            std::cout << "\nTotal: " << patches.size() << " patches" << std::endl;
        }
        
    } else if (subcommand == "clear") {
        bool clearAll = false;
        uint8_t id = 0;
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--all") {
                clearAll = true;
            } else if (arg == "--id" && i + 1 < argc) {
                id = std::stoi(argv[++i]);
            } else if (arg == "--device" && i + 1 < argc) {
                device = argv[++i];
            }
        }
        
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        
        if (clearAll) {
            if (!patchMgr.clearAll(spi)) {
                std::cerr << "Error: " << patchMgr.getLastError() << std::endl;
                spi.close();
                return 1;
            }
            std::cout << "All patches cleared" << std::endl;
        } else {
            if (!patchMgr.removePatch(id)) {
                std::cerr << "Error: " << patchMgr.getLastError() << std::endl;
                spi.close();
                return 1;
            }
            
            if (!patchMgr.applyAll(spi)) {
                std::cerr << "Error: Failed to update FPGA: " << patchMgr.getLastError() << std::endl;
                spi.close();
                return 1;
            }
            
            std::cout << "Patch " << static_cast<int>(id) << " cleared" << std::endl;
        }
        
        spi.close();
        
    } else if (subcommand == "load") {
        std::string filename;
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--file" && i + 1 < argc) {
                filename = argv[++i];
            } else if (arg == "--device" && i + 1 < argc) {
                device = argv[++i];
            }
        }
        
        if (filename.empty()) {
            std::cerr << "Error: --file required" << std::endl;
            return 1;
        }
        
        if (!patchMgr.loadFromFile(filename)) {
            std::cerr << "Error: " << patchMgr.getLastError() << std::endl;
            return 1;
        }
        
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        
        if (!patchMgr.applyAll(spi)) {
            std::cerr << "Error: Failed to apply patches: " << patchMgr.getLastError() << std::endl;
            spi.close();
            return 1;
        }
        
        std::cout << "Loaded and applied " << patchMgr.count() << " patches from " << filename << std::endl;
        
        spi.close();
        
    } else if (subcommand == "save") {
        std::string filename;
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--file" && i + 1 < argc) {
                filename = argv[++i];
            }
        }
        
        if (filename.empty()) {
            std::cerr << "Error: --file required" << std::endl;
            return 1;
        }
        
        if (!patchMgr.saveToFile(filename)) {
            std::cerr << "Error: " << patchMgr.getLastError() << std::endl;
            return 1;
        }
        
        std::cout << "Saved " << patchMgr.count() << " patches to " << filename << std::endl;
        
    } else {
        std::cerr << "Unknown subcommand: " << subcommand << std::endl;
        return 1;
    }
    
    return 0;
}

// Command: button
int cmdButton(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: rebear-cli button <subcommand> [options]" << std::endl;
        std::cerr << "Subcommands: press, release, click, status" << std::endl;
        return 1;
    }
    
    std::string subcommand = argv[2];
    int duration = 100; // Default 100ms
    
    rebear::ButtonControl button(3);
    
    if (!button.init()) {
        std::cerr << "Error: Failed to initialize button control: " << button.getLastError() << std::endl;
        return 1;
    }
    
    if (subcommand == "press") {
        if (!button.press()) {
            std::cerr << "Error: " << button.getLastError() << std::endl;
            return 1;
        }
        std::cout << "Button pressed" << std::endl;
        
    } else if (subcommand == "release") {
        if (!button.release()) {
            std::cerr << "Error: " << button.getLastError() << std::endl;
            return 1;
        }
        std::cout << "Button released" << std::endl;
        
    } else if (subcommand == "click") {
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--duration" && i + 1 < argc) {
                duration = std::stoi(argv[++i]);
            }
        }
        
        if (!button.click(duration)) {
            std::cerr << "Error: " << button.getLastError() << std::endl;
            return 1;
        }
        std::cout << "Button clicked (" << duration << "ms)" << std::endl;
        
    } else if (subcommand == "status") {
        std::cout << "Button status: " << (button.isPressed() ? "Pressed" : "Released") << std::endl;
        
    } else {
        std::cerr << "Unknown subcommand: " << subcommand << std::endl;
        return 1;
    }
    
    return 0;
}

// Command: export
int cmdExport(int argc, char* argv[]) {
    std::string device = "/dev/spidev0.0";
    uint32_t speed = 100000;
    std::string output;
    std::string format = "csv";
    int duration = 10; // Default 10 seconds
    
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        } else if (arg == "--device" && i + 1 < argc) {
            device = argv[++i];
        } else if (arg == "--duration" && i + 1 < argc) {
            duration = std::stoi(argv[++i]);
        }
    }
    
    if (output.empty()) {
        std::cerr << "Error: --output required" << std::endl;
        return 1;
    }
    
    // Connect and collect transactions
    rebear::SPIProtocol spi;
    if (!spi.open(device, speed)) {
        std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
        return 1;
    }
    
    std::cout << "Collecting transactions for " << duration << " seconds..." << std::endl;
    
    std::vector<rebear::Transaction> transactions;
    auto startTime = std::chrono::steady_clock::now();
    
    while (true) {
        auto trans = spi.readTransaction();
        
        if (trans && trans->address != 0xFFFFFF) {
            transactions.push_back(*trans);
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= duration) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    spi.close();
    
    // Write to file
    std::ofstream file(output);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open output file" << std::endl;
        return 1;
    }
    
    if (format == "csv") {
        file << "timestamp_ms,address,count" << std::endl;
        for (const auto& t : transactions) {
            file << t.timestamp << ",0x" << std::hex << t.address << std::dec << ",";
            
            // Check if count is 0xFFFFFF (FPGA actively patching)
            if (t.count == 0xFFFFFF) {
                file << "PATCHING";
            } else {
                file << t.count;
            }
            file << std::endl;
        }
    } else if (format == "json") {
        file << "{" << std::endl;
        file << "  \"transactions\": [" << std::endl;
        
        for (size_t i = 0; i < transactions.size(); i++) {
            const auto& t = transactions[i];
            file << "    {\"timestamp\": " << t.timestamp
                 << ", \"address\": \"0x" << std::hex << t.address << std::dec
                 << "\", \"count\": ";
            
            // Check if count is 0xFFFFFF (FPGA actively patching)
            if (t.count == 0xFFFFFF) {
                file << "\"PATCHING\"";
            } else {
                file << t.count;
            }
            
            file << "}";
            if (i < transactions.size() - 1) {
                file << ",";
            }
            file << std::endl;
        }
        
        file << "  ]" << std::endl;
        file << "}" << std::endl;
    }
    
    file.close();
    
    std::cout << "Exported " << transactions.size() << " transactions to " << output << std::endl;
    
    return 0;
}

// Command: clear
int cmdClear(int argc, char* argv[]) {
    std::string device = "/dev/spidev0.0";
    uint32_t speed = 100000;
    
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--device" && i + 1 < argc) {
            device = argv[++i];
        }
    }
    
    rebear::SPIProtocol spi;
    if (!spi.open(device, speed)) {
        std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
        return 1;
    }
    
    if (!spi.clearTransactions()) {
        std::cerr << "Error: Failed to clear transactions: " << spi.getLastError() << std::endl;
        spi.close();
        return 1;
    }
    
    std::cout << "Transaction buffer cleared" << std::endl;
    
    spi.close();
    return 0;
}

// Command: help
void printHelp() {
    std::cout << "Rebear CLI - Teddy Bear Reverse Engineering Tool\n" << std::endl;
    std::cout << "Usage: rebear-cli <command> [options]\n" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  monitor     Monitor transactions in real-time" << std::endl;
    std::cout << "  patch       Manage patches (set, list, clear, load, save)" << std::endl;
    std::cout << "  button      Control teddy bear button (press, release, click, status)" << std::endl;
    std::cout << "  export      Export transaction log to file" << std::endl;
    std::cout << "  clear       Clear transaction buffer" << std::endl;
    std::cout << "  help        Show this help message" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  rebear-cli monitor --duration 30" << std::endl;
    std::cout << "  rebear-cli patch set --id 0 --address 0x001000 --data 0102030405060708" << std::endl;
    std::cout << "  rebear-cli button click --duration 100" << std::endl;
    std::cout << "  rebear-cli export --output log.csv --format csv" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "monitor") {
        return cmdMonitor(argc, argv);
    } else if (command == "patch") {
        return cmdPatch(argc, argv);
    } else if (command == "button") {
        return cmdButton(argc, argv);
    } else if (command == "export") {
        return cmdExport(argc, argv);
    } else if (command == "clear") {
        return cmdClear(argc, argv);
    } else if (command == "help" || command == "--help" || command == "-h") {
        printHelp();
        return 0;
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        std::cerr << "Run 'rebear-cli help' for usage information" << std::endl;
        return 1;
    }
    
    return 0;
}
