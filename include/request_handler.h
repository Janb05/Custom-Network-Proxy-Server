#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <string>
#include "logger.h"
#include "cache_manager.h"
#include "config_manager.h"
#include "statistics.h"

class RequestHandler {
private:
    Logger* logger;
    CacheManager* cache;
    ConfigManager* config;
    Statistics* stats;
    
    void tunnel(int client, int remote);
    bool handle_https_connect(int client, const std::string& request, const std::string& client_ip);
    bool handle_http_request(int client, const std::string& request, const std::string& client_ip);
    
    std::string extract_host(const std::string& request);
    std::string extract_path(const std::string& request);
    int connect_to_host(const std::string& host, int port);
    
    void send_forbidden(int client);
    void send_error(int client, const std::string& message);

public:
    RequestHandler(Logger* log, CacheManager* cache_mgr, 
                   ConfigManager* config_mgr, Statistics* stats_mgr);
    
    void handle_client(int client);
};

#endif // REQUEST_HANDLER_H
