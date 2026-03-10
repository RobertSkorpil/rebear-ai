#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <memory>
#include <type_traits>
#include "rebear/spi_protocol.h"
#include "rebear/spi_protocol_network.h"
#include "rebear/patch_manager.h"
#include "rebear/gpio_control.h"
#include "rebear/gpio_control_network.h"
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

// Global network mode settings
std::string g_remote_host;
uint16_t g_remote_port = 9876;
bool g_use_network = false;

// Global debug settings
bool g_verbose = false;
bool g_dry_run = false;

void signalHandler(int /* signum */) {
    g_running.store(false, std::memory_order_relaxed);
}

// Helper to print hex data
void printHexData(const std::string& label, const std::vector<uint8_t>& data) {
    std::cout << label << " (" << data.size() << " bytes): ";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0 && i % 16 == 0) {
            std::cout << "\n" << std::string(label.length() + 2, ' ');
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

// Helper to build patch buffer (for verbose/dry-run display)
std::vector<uint8_t> buildPatchBuffer(const std::vector<rebear::Patch>& patches) {
    std::vector<uint8_t> buffer;
    
    if (patches.empty()) {
        return buffer;
    }
    
    // Command byte
    buffer.push_back(0x02); // CMD_SET_PATCH
    
    // Calculate data section start offset
    size_t dataOffset = (patches.size() * 8) + 1;
    size_t currentDataOffset = dataOffset;
    
    // Write patch headers
    for (const auto& patch : patches) {
        // STORED (1 byte)
        buffer.push_back(patch.enabled ? 0x80 : 0x00);
        
        // PATCH_ADDRESS (3 bytes, big-endian)
        buffer.push_back((patch.address >> 16) & 0xFF);
        buffer.push_back((patch.address >> 8) & 0xFF);
        buffer.push_back(patch.address & 0xFF);
        
        // PATCH_LENGTH (2 bytes, big-endian)
        uint16_t length = static_cast<uint16_t>(patch.data.size());
        buffer.push_back((length >> 8) & 0xFF);
        buffer.push_back(length & 0xFF);
        
        // BUFFER_DATA offset (2 bytes, big-endian)
        buffer.push_back((currentDataOffset >> 8) & 0xFF);
        buffer.push_back(currentDataOffset & 0xFF);
        
        currentDataOffset += patch.data.size();
    }
    
    // Terminator
    buffer.push_back(0x00);
    
    // Write patch data
    for (const auto& patch : patches) {
        buffer.insert(buffer.end(), patch.data.begin(), patch.data.end());
    }
    
    return buffer;
}

// Helper function to parse remote connection string
// Format: tcp://hostname:port or hostname:port
bool parseRemoteString(const std::string& remote, std::string& host, uint16_t& port) {
    std::string conn = remote;
    
    // Remove tcp:// prefix if present
    if (conn.substr(0, 6) == "tcp://") {
        conn = conn.substr(6);
    }
    
    // Find colon separator
    size_t colon_pos = conn.find(':');
    if (colon_pos == std::string::npos) {
        // No port specified, use default
        host = conn;
        port = 9876;
        return true;
    }
    
    host = conn.substr(0, colon_pos);
    try {
        port = static_cast<uint16_t>(std::stoul(conn.substr(colon_pos + 1)));
        return true;
    } catch (...) {
        return false;
    }
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

// Template implementation for monitor command (works with both local and network SPI)
template<typename SPIType>
int cmdMonitorImpl(SPIType& spi, int duration, const std::string& format) {
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
    
    // Connect to FPGA (local or network)
    if (g_use_network) {
        rebear::SPIProtocolNetwork spi(g_remote_host, g_remote_port);
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        std::cout << "Connected to FPGA via " << g_remote_host << ":" << g_remote_port << std::endl;
        return cmdMonitorImpl(spi, duration, format);
    } else {
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        std::cout << "Connected to FPGA on " << device << std::endl;
        return cmdMonitorImpl(spi, duration, format);
    }
}

// Template implementation for patch command (works with both local and network SPI)
template<typename SPIType>
int cmdPatchImpl(SPIType& spi, const std::string& subcommand, int argc, char* argv[]) {
    rebear::PatchManager patchMgr;
    
    // Check if this is local SPI (for MISO capture)
    constexpr bool isLocalSpi = std::is_same<SPIType, rebear::SPIProtocol>::value;
    
    if (subcommand == "set") {
        // Parse arguments - can have multiple patch definitions
        std::vector<rebear::Patch> patches;
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--address" && i + 1 < argc) {
                // Start new patch
                rebear::Patch patch;
                patch.address = parseAddress(argv[++i]);
                patch.enabled = true;
                patch.id = static_cast<uint8_t>(patches.size()); // Auto-assign ID
                
                // Next arg should be --data
                if (i + 1 < argc && std::string(argv[i + 1]) == "--data") {
                    ++i; // Skip --data
                    if (i + 1 < argc) {
                        std::string dataStr = argv[++i];
                        
                        if (dataStr.length() % 2 != 0) {
                            std::cerr << "Error: --data must be even number of hex characters" << std::endl;
                            return 1;
                        }
                        
                        auto dataBytes = parseHexString(dataStr);
                        if (dataBytes.empty()) {
                            std::cerr << "Error: Invalid hex data" << std::endl;
                            return 1;
                        }
                        
                        patch.data = dataBytes;
                        patches.push_back(patch);
                    } else {
                        std::cerr << "Error: --data requires hex string argument" << std::endl;
                        return 1;
                    }
                } else {
                    std::cerr << "Error: --address must be followed by --data" << std::endl;
                    return 1;
                }
            } else if (arg == "--device" && i + 1 < argc) {
                // Skip device argument, already handled
                ++i;
            }
        }
        
        if (patches.empty()) {
            std::cerr << "Error: No patches specified. Use --address 0xXXXXXX --data HEXBYTES" << std::endl;
            std::cerr << "Example: rebear-cli patch set --address 0x1000 --data FF00AA --address 0x2000 --data BBCCDD" << std::endl;
            return 1;
        }
        
        if (patches.size() > 8) {
            std::cerr << "Error: Too many patches (" << patches.size() << "). Hardware supports maximum 8 patches per buffer." << std::endl;
            return 1;
        }
        
        // Add all patches to manager
        for (const auto& patch : patches) {
            if (!patchMgr.addPatch(patch)) {
                std::cerr << "Error: " << patchMgr.getLastError() << std::endl;
                return 1;
            }
        }
        
        // Show verbose info if requested
        if (g_verbose || g_dry_run) {
            std::cout << "\n=== Patch Configuration ===" << std::endl;
            std::cout << "Number of patches: " << patches.size() << std::endl;
            
            for (size_t i = 0; i < patches.size(); ++i) {
                const auto& patch = patches[i];
                std::cout << "\nPatch " << (i+1) << ":" << std::endl;
                std::cout << "  ID: " << static_cast<int>(patch.id) << std::endl;
                std::cout << "  Address: 0x" << std::hex << patch.address << std::dec << std::endl;
                std::cout << "  Data length: " << patch.data.size() << " bytes" << std::endl;
                std::cout << "  Enabled: " << (patch.enabled ? "yes" : "no") << std::endl;
            }
            
            // Build and show the SPI buffer
            auto buffer = buildPatchBuffer(patches);
            
            std::cout << "\n=== SPI Buffer (before escape encoding) ===" << std::endl;
            printHexData("Raw buffer", buffer);
            std::cout << "Total buffer size: " << buffer.size() << " bytes" << std::endl;
        }
        
        // If dry-run, stop here
        if (g_dry_run) {
            std::cout << "\n[DRY RUN] Patches not sent to FPGA" << std::endl;
            return 0;
        }
        
        // Apply to FPGA using buffer upload
        bool success = false;
        
        if (g_verbose && isLocalSpi) {
            // Use verbose version to capture MISO data (only for local SPI)
            std::vector<uint8_t> misoData;
            
            if constexpr (std::is_same<SPIType, rebear::SPIProtocol>::value) {
                success = spi.uploadPatchBufferVerbose(patches, misoData);
                
                if (success && !misoData.empty()) {
                    std::cout << "\n=== FPGA Response (MISO) ===" << std::endl;
                    printHexData("Received data", misoData);
                }
            }
        } else {
            success = patchMgr.applyAllBuffer(spi);
            if (g_verbose && !isLocalSpi) {
                std::cout << "\n[Note: MISO data capture not available in network mode]" << std::endl;
            }
        }
        
        if (!success) {
            std::cerr << "Error: Failed to apply patches: " << patchMgr.getLastError() << std::endl;
            spi.close();
            return 1;
        }
        
        std::cout << "\n" << patches.size() << " patch(es) applied successfully" << std::endl;
        
        spi.close();
        
    } else if (subcommand == "list") {
        std::string format = "text";
        std::string filename;
        bool fromFile = false;
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--format" && i + 1 < argc) {
                format = argv[++i];
            } else if (arg == "--file" && i + 1 < argc) {
                filename = argv[++i];
                fromFile = true;
            } else if (arg == "--device" && i + 1 < argc) {
                // Skip device argument, already handled
                ++i;
            }
        }
        
        std::vector<rebear::Patch> patches;
        
        if (fromFile) {
            // Load from file
            if (!patchMgr.loadFromFile(filename)) {
                std::cerr << "Error: " << patchMgr.getLastError() << std::endl;
                return 1;
            }
            patches = patchMgr.getPatches();
        } else {
            // Dump from FPGA and parse the buffer
            std::vector<uint8_t> buffer;
            if (!spi.dumpPatchBuffer(buffer)) {
                std::cerr << "Error: Failed to dump patch buffer: " << spi.getLastError() << std::endl;
                spi.close();
                return 1;
            }
            
            if (buffer.empty()) {
                std::cout << "No patches loaded in FPGA" << std::endl;
                spi.close();
                return 0;
            }
            
            // Parse the buffer to extract patches
            // Buffer format: [HEADER_0, HEADER_1, ..., TERMINATOR, DATA_0, DATA_1, ...]
            // Each header: STORED(1) + ADDRESS(3) + LENGTH(2) + OFFSET(2) = 8 bytes
            
            size_t offset = 0;
            uint8_t patchId = 0;
            
            // Parse headers
            while (offset < buffer.size()) {
                // Read STORED byte
                uint8_t stored = buffer[offset++];
                
                // Check for terminator (0x00 with no more header data)
                if (stored == 0x00 && offset >= buffer.size()) {
                    break;  // Terminator
                }
                
                if (offset + 7 > buffer.size()) {
                    break;  // Not enough data for a full header
                }
                
                // Parse patch header
                uint32_t address = (static_cast<uint32_t>(buffer[offset]) << 16) |
                                  (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
                                  static_cast<uint32_t>(buffer[offset + 2]);
                offset += 3;
                
                uint16_t length = (static_cast<uint16_t>(buffer[offset]) << 8) |
                                 static_cast<uint16_t>(buffer[offset + 1]);
                offset += 2;
                
                uint16_t dataOffset = (static_cast<uint16_t>(buffer[offset]) << 8) |
                                     static_cast<uint16_t>(buffer[offset + 1]);
                offset += 2;
                
                // Check for terminator (stored == 0x00 means terminator or disabled)
                if (stored == 0x00) {
                    break;  // Terminator found
                }
                
                // Extract patch data
                if (dataOffset + length > buffer.size()) {
                    std::cerr << "Warning: Invalid patch data offset/length" << std::endl;
                    break;
                }
                
                rebear::Patch patch;
                patch.id = patchId++;
                patch.address = address;
                patch.enabled = (stored == 0x80);
                patch.data.assign(buffer.begin() + dataOffset, 
                                 buffer.begin() + dataOffset + length);
                
                patches.push_back(patch);
            }
        }
        
        // Display patches
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
            if (fromFile) {
                std::cout << "Patches from file:" << std::endl;
            } else {
                std::cout << "Patches loaded in FPGA:" << std::endl;
            }
            std::cout << std::left << std::setw(6) << "ID"
                      << std::setw(12) << "Address"
                      << std::setw(20) << "Data"
                      << "Status" << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            
            for (const auto& p : patches) {
                std::cout << std::left << std::setw(6) << static_cast<int>(p.id);
                
                // Format address
                std::stringstream addrStream;
                addrStream << "0x" << std::hex << std::setw(6) << std::setfill('0') << p.address;
                std::cout << std::left << std::setw(12) << addrStream.str() << std::dec;
                
                // Format data
                std::stringstream dataStream;
                for (const auto& byte : p.data) {
                    dataStream << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(byte);
                }
                std::cout << std::left << std::setw(20) << dataStream.str() << std::dec;
                
                std::cout << (p.enabled ? "Active" : "Disabled") << std::endl;
            }
            
            std::cout << "\nTotal: " << patches.size() << " patches" << std::endl;
        }
        
        spi.close();
        
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
                // Skip device argument, already handled
                ++i;
            }
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
                // Skip device argument, already handled
                ++i;
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
        
    } else if (subcommand == "dump") {
        std::string format = "hex";
        std::string output;
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--format" && i + 1 < argc) {
                format = argv[++i];
            } else if (arg == "--output" && i + 1 < argc) {
                output = argv[++i];
            } else if (arg == "--device" && i + 1 < argc) {
                // Skip device argument, already handled
                ++i;
            }
        }
        
        // Dump patch buffer from FPGA
        std::vector<uint8_t> buffer;
        if (!spi.dumpPatchBuffer(buffer)) {
            std::cerr << "Error: Failed to dump patch buffer: " << spi.getLastError() << std::endl;
            spi.close();
            return 1;
        }
        
        if (buffer.empty()) {
            std::cout << "Patch buffer is empty" << std::endl;
            spi.close();
            return 0;
        }
        
        // Output the buffer
        if (!output.empty()) {
            // Write to file
            std::ofstream file(output, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "Error: Failed to open output file: " << output << std::endl;
                spi.close();
                return 1;
            }
            
            if (format == "hex") {
                // Write as hex text
                for (size_t i = 0; i < buffer.size(); ++i) {
                    file << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(buffer[i]);
                    if ((i + 1) % 16 == 0) {
                        file << "\n";
                    } else {
                        file << " ";
                    }
                }
                file << std::dec << std::endl;
            } else if (format == "binary") {
                // Write as raw binary
                file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            } else {
                std::cerr << "Error: Unknown format: " << format << std::endl;
                spi.close();
                return 1;
            }
            
            file.close();
            std::cout << "Dumped " << buffer.size() << " bytes to " << output << std::endl;
        } else {
            // Print to stdout
            std::cout << "Patch buffer content (" << buffer.size() << " bytes):" << std::endl;
            printHexData("Buffer", buffer);
        }
        
        spi.close();
        
    } else {
        std::cerr << "Unknown subcommand: " << subcommand << std::endl;
        return 1;
    }
    
    return 0;
}

// Command: patch
int cmdPatch(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: rebear-cli patch <subcommand> [options]" << std::endl;
        std::cerr << "Subcommands: set, list, clear, load, save, dump" << std::endl;
        return 1;
    }
    
    std::string subcommand = argv[2];
    std::string device = "/dev/spidev0.0";
    uint32_t speed = 100000;
    
    // Parse device argument
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--device" && i + 1 < argc) {
            device = argv[i + 1];
            break;
        }
    }
    
    // Connect to FPGA (local or network)
    if (g_use_network) {
        rebear::SPIProtocolNetwork spi(g_remote_host, g_remote_port);
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        return cmdPatchImpl(spi, subcommand, argc, argv);
    } else {
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        return cmdPatchImpl(spi, subcommand, argc, argv);
    }
}

// Template implementation for button command (works with both local and network GPIO)
template<typename GPIOType>
int cmdButtonImpl(GPIOType& gpio, const std::string& subcommand, int argc, char* argv[]) {
    if (subcommand == "press") {
        if (!gpio.write(true)) {
            std::cerr << "Error: " << gpio.getLastError() << std::endl;
            return 1;
        }
        std::cout << "Button pressed" << std::endl;
        
    } else if (subcommand == "release") {
        if (!gpio.write(false)) {
            std::cerr << "Error: " << gpio.getLastError() << std::endl;
            return 1;
        }
        std::cout << "Button released" << std::endl;
        
    } else if (subcommand == "click") {
        int duration = 100; // Default 100ms
        
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--duration" && i + 1 < argc) {
                duration = std::stoi(argv[++i]);
            }
        }
        
        if (!gpio.write(true)) {
            std::cerr << "Error: " << gpio.getLastError() << std::endl;
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
        if (!gpio.write(false)) {
            std::cerr << "Error: " << gpio.getLastError() << std::endl;
            return 1;
        }
        std::cout << "Button clicked (" << duration << "ms)" << std::endl;
        
    } else if (subcommand == "status") {
        bool pressed = gpio.read();
        std::cout << "Button status: " << (pressed ? "Pressed" : "Released") << std::endl;
        
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
    
    // Connect to GPIO (local or network)
    if (g_use_network) {
        rebear::GPIOControlNetwork gpio(3, rebear::GPIOControl::Direction::Output,
                                        g_remote_host, g_remote_port);
        if (!gpio.init()) {
            std::cerr << "Error: Failed to initialize button control: " << gpio.getLastError() << std::endl;
            return 1;
        }
        return cmdButtonImpl(gpio, subcommand, argc, argv);
    } else {
        rebear::GPIOControl gpio(3, rebear::GPIOControl::Direction::Output);
        if (!gpio.init()) {
            std::cerr << "Error: Failed to initialize button control: " << gpio.getLastError() << std::endl;
            return 1;
        }
        return cmdButtonImpl(gpio, subcommand, argc, argv);
    }
}

// Template implementation for export command (works with both local and network SPI)
template<typename SPIType>
int cmdExportImpl(SPIType& spi, const std::string& output, const std::string& format, int duration) {
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
    
    // Connect to FPGA (local or network)
    if (g_use_network) {
        rebear::SPIProtocolNetwork spi(g_remote_host, g_remote_port);
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        return cmdExportImpl(spi, output, format, duration);
    } else {
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        return cmdExportImpl(spi, output, format, duration);
    }
}

// Template implementation for clear command (works with both local and network SPI)
template<typename SPIType>
int cmdClearImpl(SPIType& spi) {
    if (!spi.clearTransactions()) {
        std::cerr << "Error: Failed to clear transactions: " << spi.getLastError() << std::endl;
        spi.close();
        return 1;
    }
    
    std::cout << "Transaction buffer cleared" << std::endl;
    
    spi.close();
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
    
    // Connect to FPGA (local or network)
    if (g_use_network) {
        rebear::SPIProtocolNetwork spi(g_remote_host, g_remote_port);
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        return cmdClearImpl(spi);
    } else {
        rebear::SPIProtocol spi;
        if (!spi.open(device, speed)) {
            std::cerr << "Error: Failed to open SPI device: " << spi.getLastError() << std::endl;
            return 1;
        }
        return cmdClearImpl(spi);
    }
}

// Command: help
void printHelp() {
    std::cout << "Rebear CLI - Teddy Bear Reverse Engineering Tool\n" << std::endl;
    std::cout << "Usage: rebear-cli [options] <command> [command-options]\n" << std::endl;
    std::cout << "Global Options:" << std::endl;
    std::cout << "  --remote <host[:port]>  Connect to remote rebear-server (default port: 9876)" << std::endl;
    std::cout << "                          Format: hostname:port or tcp://hostname:port" << std::endl;
    std::cout << "  -v, --verbose           Show raw SPI data being sent (hex format)" << std::endl;
    std::cout << "  --dry, --dry-run        Show data to be sent but don't actually send it" << std::endl;
    std::cout << "\nCommands:" << std::endl;
    std::cout << "  monitor     Monitor transactions in real-time" << std::endl;
    std::cout << "  patch       Manage patches (set, list, clear, load, save, dump)" << std::endl;
    std::cout << "  button      Control teddy bear button (press, release, click, status)" << std::endl;
    std::cout << "  export      Export transaction log to file" << std::endl;
    std::cout << "  clear       Clear transaction buffer" << std::endl;
    std::cout << "  help        Show this help message" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  # Local mode (direct hardware access)" << std::endl;
    std::cout << "  rebear-cli monitor --duration 30" << std::endl;
    std::cout << "  rebear-cli patch set --address 0x001000 --data 0102030405060708" << std::endl;
    std::cout << "  rebear-cli patch set --address 0x001000 --data FF --address 0x002000 --data DEADBEEF" << std::endl;
    std::cout << "\n  # Network mode (remote hardware access)" << std::endl;
    std::cout << "  rebear-cli --remote raspberrypi.local monitor --duration 30" << std::endl;
    std::cout << "  rebear-cli --remote tcp://192.168.1.100:9876 button click" << std::endl;
    std::cout << "  rebear-cli --remote pi3:9876 export --output log.csv" << std::endl;
    std::cout << "\n  # Multiple patches (up to 8 per command)" << std::endl;
    std::cout << "  rebear-cli patch set --address 0x1000 --data FF --address 0x2000 --data AABB" << std::endl;
    std::cout << "  rebear-cli patch set --address 0x1000 --data DEADBEEF --address 0x2000 --data CAFEBABE" << std::endl;
    std::cout << "\n  # Verbose and dry-run modes" << std::endl;
    std::cout << "  rebear-cli -v patch set --address 0x1000 --data DEADBEEF" << std::endl;
    std::cout << "  rebear-cli --dry patch set --address 0x1000 --data DEADBEEF" << std::endl;
    std::cout << "  rebear-cli -v --dry patch set --address 0x1000 --data FF --address 0x2000 --data AA" << std::endl;
    std::cout << "\n  # Dump patch buffer from FPGA" << std::endl;
    std::cout << "  rebear-cli patch dump" << std::endl;
    std::cout << "  rebear-cli patch dump --output buffer.hex" << std::endl;
    std::cout << "  rebear-cli patch dump --output buffer.bin --format binary" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }
    
    // Parse global flags
    int cmd_start = 1;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--remote" && i + 1 < argc) {
            std::string remote = argv[i + 1];
            if (!parseRemoteString(remote, g_remote_host, g_remote_port)) {
                std::cerr << "Error: Invalid remote connection string: " << remote << std::endl;
                std::cerr << "Format: hostname:port or tcp://hostname:port" << std::endl;
                return 1;
            }
            g_use_network = true;
            i++; // Skip next arg
        } else if (arg == "-v" || arg == "--verbose") {
            g_verbose = true;
        } else if (arg == "--dry" || arg == "--dry-run") {
            g_dry_run = true;
        } else if (arg.substr(0, 2) != "--" && arg[0] != '-') {
            // Found command
            cmd_start = i;
            break;
        }
    }
    
    if (cmd_start >= argc) {
        std::cerr << "Error: No command specified" << std::endl;
        printHelp();
        return 1;
    }
    
    std::string command = argv[cmd_start];
    
    // Shift arguments for command functions
    int new_argc = argc - cmd_start + 1;
    char** new_argv = argv + cmd_start - 1;
    new_argv[0] = argv[0]; // Keep program name
    
    if (command == "monitor") {
        return cmdMonitor(new_argc, new_argv);
    } else if (command == "patch") {
        return cmdPatch(new_argc, new_argv);
    } else if (command == "button") {
        return cmdButton(new_argc, new_argv);
    } else if (command == "export") {
        return cmdExport(new_argc, new_argv);
    } else if (command == "clear") {
        return cmdClear(new_argc, new_argv);
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
