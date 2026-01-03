#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>

class ConfigManager {
private:
    std::string config_file;
    std::atomic<time_t> last_mtime;
    std::mutex config_mutex;
    
    // Configuration values
    int port;
    int cache_limit;
    int cache_ttl;
    std::string log_level;
    size_t max_cache_size_mb;
    int connection_timeout;
    bool enable_stats;
    
    std::unordered_set<std::string> blocked_hosts;
    std::unordered_set<std::string> whitelisted_hosts;
    
    // Callback for config changes
    std::function<void()> on_config_changed;

public:
    ConfigManager(const std::string& filename);
    
    bool load();
    void watch(std::function<void()> callback = nullptr);
    void stop_watching();
    
    // Getters
    int get_port() const { return port; }
    int get_cache_limit() const { return cache_limit; }
    int get_cache_ttl() const { return cache_ttl; }
    std::string get_log_level() const { return log_level; }
    size_t get_max_cache_size_mb() const { return max_cache_size_mb; }
    int get_connection_timeout() const { return connection_timeout; }
    bool is_stats_enabled() const { return enable_stats; }
    
    bool is_blocked(const std::string& host) const;
    bool is_whitelisted(const std::string& host) const;
    
    // Setters (thread-safe)
    void set_port(int p);
    void set_cache_limit(int limit);
    void add_blocked_host(const std::string& host);
    void remove_blocked_host(const std::string& host);
};

#endif // CONFIG_MANAGER_H
