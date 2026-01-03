#include "../include/proxy_server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

ProxyServer* server_instance = nullptr;

void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down..." << std::endl;
    if (server_instance) {
        server_instance->stop();
        exit(0);
    }
}

void print_banner() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════╗\n";
    std::cout << "║     ADVANCED PROXY SERVER v2.0           ║\n";
    std::cout << "║     Multi-threaded HTTP/HTTPS Proxy      ║\n";
    std::cout << "╚═══════════════════════════════════════════╝\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    print_banner();
    
    std::string config_file = "config.txt";
    
    // Parse command line arguments
    if (argc > 1) {
        config_file = argv[1];
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        ProxyServer server(config_file);
        server_instance = &server;
        
        if (!server.start()) {
            std::cerr << "❌ Failed to start proxy server" << std::endl;
            return 1;
        }
        
        server.run();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
