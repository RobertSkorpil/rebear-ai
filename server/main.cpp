#include "network_server.h"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> shutdown_requested(false);

void signalHandler(int signal) {
    std::cout << "\nShutdown signal received (" << signal << ")" << std::endl;
    shutdown_requested = true;
}

int main(int argc, char* argv[]) {
    std::cout << "Rebear Hardware Server v1.0" << std::endl;
    std::cout << "============================" << std::endl;
    
    // Parse command line arguments
    uint16_t port = 9876;
    int max_clients = 10;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" || arg == "-p") {
            if (i + 1 < argc) {
                port = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: --port requires an argument" << std::endl;
                return 1;
            }
        } else if (arg == "--max-clients" || arg == "-m") {
            if (i + 1 < argc) {
                max_clients = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: --max-clients requires an argument" << std::endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -p, --port PORT          Port to listen on (default: 9876)" << std::endl;
            std::cout << "  -m, --max-clients NUM    Maximum concurrent clients (default: 10)" << std::endl;
            std::cout << "  -h, --help               Show this help message" << std::endl;
            return 0;
        } else {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            std::cerr << "Use --help for usage information" << std::endl;
            return 1;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Create and start server
    rebear::server::NetworkServer server(port, max_clients);
    
    if (!server.start()) {
        std::cerr << "Failed to start server: " << server.getLastError() << std::endl;
        return 1;
    }
    
    std::cout << "Server listening on port " << port << std::endl;
    std::cout << "Maximum clients: " << max_clients << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    
    // Wait for shutdown signal
    while (!shutdown_requested && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Stopping server..." << std::endl;
    server.stop();
    
    std::cout << "Server stopped successfully" << std::endl;
    return 0;
}
