#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <string>
#include <atomic>
#include <semaphore.h>
#include "logger.h"
#include "cache_manager.h"
#include "config_manager.h"
#include "statistics.h"
#include "request_handler.h"

class ProxyServer {
private:
    int server_socket;
    std::atomic<bool> running;
    
    Logger* logger;
    CacheManager* cache;
    ConfigManager* config;
    Statistics* stats;
    RequestHandler* handler;
    
    sem_t* connection_semaphore;  // Pointer for named semaphore (macOS compatible)
    int max_connections;
    
    bool setup_socket();
    void accept_connections();
    void handle_stats_request(int client);

public:
    ProxyServer(const std::string& config_file, int max_conn = 0);  // 0 = use config value
    ~ProxyServer();
    
    bool start();
    void stop();
    void run();
    
    Statistics* get_statistics() { return stats; }
};

#endif // PROXY_SERVER_H
