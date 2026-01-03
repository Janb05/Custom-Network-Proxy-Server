#include "../include/cache_manager.h"
#include <algorithm>

CacheManager::CacheManager(size_t max_entries, int default_ttl)
    : max_entries(max_entries), default_ttl(default_ttl), 
      total_size(0), max_size_bytes(100 * 1024 * 1024), // 100 MB default
      cache_hits(0), cache_misses(0) {
}

bool CacheManager::is_expired(const CacheEntry& entry) {
    time_t now = time(nullptr);
    return (now - entry.timestamp) > entry.ttl_seconds;
}

void CacheManager::evict_oldest() {
    if (lru.empty()) return;
    
    std::string oldest = lru.back();
    lru.pop_back();
    
    auto it = cache.find(oldest);
    if (it != cache.end()) {
        total_size -= it->second.first.size;
        cache.erase(it);
    }
}

void CacheManager::evict_if_needed(size_t new_size) {
    while ((cache.size() >= max_entries || total_size + new_size > max_size_bytes) 
           && !lru.empty()) {
        evict_oldest();
    }
}

bool CacheManager::get(const std::string& key, std::string& data) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    auto it = cache.find(key);
    if (it == cache.end()) {
        cache_misses++;
        return false;
    }
    
    // Check if expired
    if (is_expired(it->second.first)) {
        lru.erase(it->second.second);
        total_size -= it->second.first.size;
        cache.erase(it);
        cache_misses++;
        return false;
    }
    
    // Move to front (most recently used)
    lru.erase(it->second.second);
    lru.push_front(key);
    it->second.second = lru.begin();
    
    data = it->second.first.data;
    cache_hits++;
    return true;
}

void CacheManager::put(const std::string& key, const std::string& data, int ttl) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    int actual_ttl = (ttl < 0) ? default_ttl : ttl;
    size_t data_size = data.size();
    
    // Remove existing entry if present
    auto it = cache.find(key);
    if (it != cache.end()) {
        lru.erase(it->second.second);
        total_size -= it->second.first.size;
        cache.erase(it);
    }
    
    // Evict if necessary
    evict_if_needed(data_size);
    
    // Add new entry
    lru.push_front(key);
    CacheEntry entry{data, time(nullptr), actual_ttl, data_size};
    cache[key] = {entry, lru.begin()};
    total_size += data_size;
}

void CacheManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    auto it = cache.find(key);
    if (it != cache.end()) {
        lru.erase(it->second.second);
        total_size -= it->second.first.size;
        cache.erase(it);
    }
}

void CacheManager::clear() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    cache.clear();
    lru.clear();
    total_size = 0;
}

void CacheManager::set_max_entries(size_t max) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    max_entries = max;
    while (cache.size() > max_entries && !lru.empty()) {
        evict_oldest();
    }
}

void CacheManager::set_default_ttl(int seconds) {
    default_ttl = seconds;
}

void CacheManager::set_max_size(size_t bytes) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    max_size_bytes = bytes;
    while (total_size > max_size_bytes && !lru.empty()) {
        evict_oldest();
    }
}

size_t CacheManager::size() const {
    return cache.size();
}

double CacheManager::get_hit_rate() const {
    unsigned long long total = cache_hits + cache_misses;
    if (total == 0) return 0.0;
    return (double)cache_hits / total * 100.0;
}

void CacheManager::cleanup_expired() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    auto it = lru.rbegin();
    while (it != lru.rend()) {
        auto cache_it = cache.find(*it);
        if (cache_it != cache.end() && is_expired(cache_it->second.first)) {
            std::string key = *it;
            ++it;
            
            auto list_it = cache_it->second.second;
            total_size -= cache_it->second.first.size;
            cache.erase(cache_it);
            lru.erase(list_it);
        } else {
            ++it;
        }
    }
}
