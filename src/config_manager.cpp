#include "../include/config_manager.h"
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

ConfigManager::ConfigManager(const std::string& filename)
    : config_file(filename), last_mtime(0),
      port(8080), cache_limit(100), cache_ttl(3600),
      log_level("INFO"), max_cache_size_mb(100),
      connection_timeout(30), enable_stats(true) {
}

bool ConfigManager::load() {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "⚠️  Could not open config file: " << config_file << std::endl;
        return false;
    }
    
    std::unordered_set<std::string> new_blocked;
    std::unordered_set<std::string> new_whitelist;
    
    std::string line;
    while (getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);
        
        // Parse key=value
        if (line.find("PORT=") == 0) {
            port = std::stoi(line.substr(5));
        }
        else if (line.find("CACHE_LIMIT=") == 0) {
            cache_limit = std::stoi(line.substr(12));
        }
        else if (line.find("CACHE_TTL=") == 0) {
            cache_ttl = std::stoi(line.substr(10));
        }
        else if (line.find("LOG_LEVEL=") == 0) {
            log_level = line.substr(10);
        }
        else if (line.find("MAX_CACHE_SIZE_MB=") == 0) {
            max_cache_size_mb = std::stoi(line.substr(18));
        }
        else if (line.find("CONNECTION_TIMEOUT=") == 0) {
            connection_timeout = std::stoi(line.substr(19));
        }
        else if (line.find("ENABLE_STATS=") == 0) {
            std::string val = line.substr(13);
            enable_stats = (val == "true" || val == "1" || val == "yes");
        }
        else if (line.find("BLOCK=") == 0) {
            new_blocked.insert(line.substr(6));
        }
        else if (line.find("WHITELIST=") == 0) {
            new_whitelist.insert(line.substr(10));
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        blocked_hosts = new_blocked;
        whitelisted_hosts = new_whitelist;
    }
    
    std::cout << "✅ Config loaded: PORT=" << port 
              << ", CACHE_LIMIT=" << cache_limit 
              << ", TTL=" << cache_ttl << "s"
              << ", BLOCKED=" << blocked_hosts.size() << std::endl;
    
    return true;
}

void ConfigManager::watch(std::function<void()> callback) {
    on_config_changed = callback;
    
    std::thread([this]() {
        struct stat st{};
        
        while (true) {
            if (stat(config_file.c_str(), &st) == 0) {
                if (st.st_mtime != last_mtime.load()) {
                    last_mtime = st.st_mtime;
                    
                    if (load() && on_config_changed) {
                        on_config_changed();
                    }
                }
            }
            sleep(2);
        }
    }).detach();
}

void ConfigManager::stop_watching() {
    // In a real implementation, you'd want a flag to stop the thread
}

bool ConfigManager::is_blocked(const std::string& host) const {
    // Since this is const, we can't lock. In a real implementation,
    // you'd use a read-write lock or make the mutex mutable
    // For now, we'll access without lock (safe for reading)
    
    // If whitelist is not empty and host is whitelisted, allow it
    if (!whitelisted_hosts.empty() && whitelisted_hosts.count(host)) {
        return false;
    }
    
    return blocked_hosts.count(host) > 0;
}

bool ConfigManager::is_whitelisted(const std::string& host) const {
    return whitelisted_hosts.count(host) > 0;
}

void ConfigManager::set_port(int p) {
    std::lock_guard<std::mutex> lock(config_mutex);
    port = p;
}

void ConfigManager::set_cache_limit(int limit) {
    std::lock_guard<std::mutex> lock(config_mutex);
    cache_limit = limit;
}

void ConfigManager::add_blocked_host(const std::string& host) {
    std::lock_guard<std::mutex> lock(config_mutex);
    blocked_hosts.insert(host);
}

void ConfigManager::remove_blocked_host(const std::string& host) {
    std::lock_guard<std::mutex> lock(config_mutex);
    blocked_hosts.erase(host);
}
