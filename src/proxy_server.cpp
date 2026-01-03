#include "../include/proxy_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <csignal>

ProxyServer::ProxyServer(const std::string& config_file)
    : server_socket(-1), running(false) {
    
    config = new ConfigManager(config_file);
    config->load();
    
    // Initialize logger with appropriate level
    LogLevel level = INFO;
    std::string log_level_str = config->get_log_level();
    if (log_level_str == "DEBUG") level = DEBUG;
    else if (log_level_str == "WARN") level = WARN;
    else if (log_level_str == "ERROR") level = ERROR;
    
    logger = new Logger("logs/proxy.log", level);
    
    cache = new CacheManager(config->get_cache_limit(), config->get_cache_ttl());
    cache->set_max_size(config->get_max_cache_size_mb() * 1024 * 1024);
    
    stats = config->is_stats_enabled() ? new Statistics() : nullptr;
    
    handler = new RequestHandler(logger, cache, config, stats);
    
    logger->info("Proxy server initialized");
}

ProxyServer::~ProxyServer() {
    stop();
    
    delete handler;
    delete stats;
    delete cache;
    delete logger;
    delete config;
}

bool ProxyServer::setup_socket() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        logger->error("Failed to create socket");
        return false;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger->warn("Failed to set SO_REUSEADDR");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config->get_port());

    if (bind(server_socket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        logger->error("Failed to bind to port " + std::to_string(config->get_port()));
        close(server_socket);
        return false;
    }

    if (listen(server_socket, 50) < 0) {
        logger->error("Failed to listen on socket");
        close(server_socket);
        return false;
    }

    return true;
}

void ProxyServer::handle_stats_request(int client) {
    if (!stats) {
        std::string response = "HTTP/1.1 404 Not Found\r\n\r\nStats not enabled";
        send(client, response.c_str(), response.size(), 0);
        return;
    }
    
    std::string stats_json = stats->get_json_stats();
    std::string response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: " + std::to_string(stats_json.size()) + "\r\n"
                          "\r\n" + stats_json;
    
    send(client, response.c_str(), response.size(), 0);
    close(client);
}

void ProxyServer::accept_connections() {
    while (running) {
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        
        int client = accept(server_socket, (sockaddr*)&client_addr, &len);
        if (client < 0) {
            if (running) {
                logger->error("Failed to accept connection");
            }
            continue;
        }

        // Launch handler in new thread
        std::thread([this, client]() {
            handler->handle_client(client);
        }).detach();
    }
}

bool ProxyServer::start() {
    if (running) {
        logger->warn("Server is already running");
        return false;
    }

    if (!setup_socket()) {
        return false;
    }

    running = true;
    
    // Start config watcher
    config->watch([this]() {
        logger->info("Configuration reloaded");
        cache->set_max_entries(config->get_cache_limit());
        cache->set_default_ttl(config->get_cache_ttl());
        cache->set_max_size(config->get_max_cache_size_mb() * 1024 * 1024);
    });
    
    // Start cache cleanup thread
    std::thread([this]() {
        while (running) {
            sleep(300); // Clean every 5 minutes
            cache->cleanup_expired();
            logger->debug("Cache cleanup completed");
        }
    }).detach();

    logger->info("🚀 Proxy server started on port " + std::to_string(config->get_port()));
    std::cout << "🚀 Proxy server running on port " << config->get_port() << std::endl;
    std::cout << "📊 Cache limit: " << config->get_cache_limit() 
              << " entries, TTL: " << config->get_cache_ttl() << "s" << std::endl;
    std::cout << "🔒 Blocked hosts: " << (config->is_blocked("test") ? "Enabled" : "0") << std::endl;
    std::cout << "Press Ctrl+C to stop\n" << std::endl;

    return true;
}

void ProxyServer::stop() {
    if (!running) return;
    
    running = false;
    
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    
    logger->info("Proxy server stopped");
    
    if (stats) {
        std::cout << stats->get_summary();
    }
}

void ProxyServer::run() {
    accept_connections();
}
