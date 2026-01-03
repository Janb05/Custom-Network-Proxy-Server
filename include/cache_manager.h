#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <string>
#include <unordered_map>
#include <list>
#include <mutex>
#include <ctime>

struct CacheEntry {
    std::string data;
    time_t timestamp;
    int ttl_seconds;
    size_t size;
};

class CacheManager {
private:
    std::unordered_map<std::string, std::pair<CacheEntry, std::list<std::string>::iterator>> cache;
    std::list<std::string> lru;
    std::mutex cache_mutex;
    
    size_t max_entries;
    int default_ttl;
    size_t total_size;
    size_t max_size_bytes;
    
    // Statistics
    unsigned long long cache_hits;
    unsigned long long cache_misses;

    bool is_expired(const CacheEntry& entry);
    void evict_oldest();
    void evict_if_needed(size_t new_size);

public:
    CacheManager(size_t max_entries = 100, int default_ttl = 3600);
    
    bool get(const std::string& key, std::string& data);
    void put(const std::string& key, const std::string& data, int ttl = -1);
    void remove(const std::string& key);
    void clear();
    
    void set_max_entries(size_t max);
    void set_default_ttl(int seconds);
    void set_max_size(size_t bytes);
    
    size_t size() const;
    double get_hit_rate() const;
    unsigned long long get_hits() const { return cache_hits; }
    unsigned long long get_misses() const { return cache_misses; }
    size_t get_total_size() const { return total_size; }
    
    void cleanup_expired();
};

#endif // CACHE_MANAGER_H
