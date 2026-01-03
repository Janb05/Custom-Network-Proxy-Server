#ifndef STATISTICS_H
#define STATISTICS_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>

struct HostStats {
    unsigned long long requests;
    unsigned long long bytes_sent;
    unsigned long long bytes_received;
    std::chrono::milliseconds total_time;
};

class Statistics {
private:
    mutable std::mutex stats_mutex;  // Made mutable for const methods
    
    std::atomic<unsigned long long> total_requests;
    std::atomic<unsigned long long> total_cached;
    std::atomic<unsigned long long> total_blocked;
    std::atomic<unsigned long long> total_errors;
    std::atomic<unsigned long long> total_bytes_sent;
    std::atomic<unsigned long long> total_bytes_received;
    
    std::unordered_map<std::string, HostStats> per_host_stats;
    std::unordered_map<std::string, unsigned long long> ip_request_count;
    
    std::chrono::system_clock::time_point start_time;

public:
    Statistics();
    
    void record_request(const std::string& host, const std::string& client_ip);
    void record_cached_request();
    void record_blocked_request();
    void record_error();
    void record_bytes(const std::string& host, size_t sent, size_t received);
    void record_time(const std::string& host, std::chrono::milliseconds duration);
    
    // Getters
    unsigned long long get_total_requests() const { return total_requests.load(); }
    unsigned long long get_cached_requests() const { return total_cached.load(); }
    unsigned long long get_blocked_requests() const { return total_blocked.load(); }
    unsigned long long get_error_count() const { return total_errors.load(); }
    unsigned long long get_bytes_sent() const { return total_bytes_sent.load(); }
    unsigned long long get_bytes_received() const { return total_bytes_received.load(); }
    
    std::string get_summary() const;
    std::string get_json_stats() const;
    std::string get_top_hosts(int limit = 10) const;
    std::string get_client_stats() const;
    
    void reset();
    double get_uptime_seconds() const;
};

#endif // STATISTICS_H
