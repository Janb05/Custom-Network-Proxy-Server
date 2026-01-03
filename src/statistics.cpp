#include "../include/statistics.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>

Statistics::Statistics() 
    : total_requests(0), total_cached(0), total_blocked(0),
      total_errors(0), total_bytes_sent(0), total_bytes_received(0),
      start_time(std::chrono::system_clock::now()) {
}

void Statistics::record_request(const std::string& host, const std::string& client_ip) {
    total_requests++;
    
    std::lock_guard<std::mutex> lock(stats_mutex);
    per_host_stats[host].requests++;
    ip_request_count[client_ip]++;
}

void Statistics::record_cached_request() {
    total_cached++;
}

void Statistics::record_blocked_request() {
    total_blocked++;
}

void Statistics::record_error() {
    total_errors++;
}

void Statistics::record_bytes(const std::string& host, size_t sent, size_t received) {
    total_bytes_sent += sent;
    total_bytes_received += received;
    
    std::lock_guard<std::mutex> lock(stats_mutex);
    per_host_stats[host].bytes_sent += sent;
    per_host_stats[host].bytes_received += received;
}

void Statistics::record_time(const std::string& host, std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    per_host_stats[host].total_time += duration;
}

std::string Statistics::get_summary() const {
    std::ostringstream oss;
    double uptime = get_uptime_seconds();
    
    oss << "\n========== PROXY SERVER STATISTICS ==========\n";
    oss << "Uptime: " << std::fixed << std::setprecision(2) << uptime << " seconds\n";
    oss << "Total Requests: " << total_requests.load() << "\n";
    oss << "  - Cached: " << total_cached.load() << "\n";
    oss << "  - Blocked: " << total_blocked.load() << "\n";
    oss << "  - Errors: " << total_errors.load() << "\n";
    oss << "Bytes Sent: " << total_bytes_sent.load() << " bytes\n";
    oss << "Bytes Received: " << total_bytes_received.load() << " bytes\n";
    
    if (total_requests.load() > 0) {
        double cache_rate = (double)total_cached.load() / total_requests.load() * 100.0;
        oss << "Cache Hit Rate: " << std::fixed << std::setprecision(2) << cache_rate << "%\n";
    }
    
    oss << "============================================\n";
    return oss.str();
}

std::string Statistics::get_json_stats() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"uptime_seconds\": " << get_uptime_seconds() << ",\n";
    oss << "  \"total_requests\": " << total_requests.load() << ",\n";
    oss << "  \"cached_requests\": " << total_cached.load() << ",\n";
    oss << "  \"blocked_requests\": " << total_blocked.load() << ",\n";
    oss << "  \"errors\": " << total_errors.load() << ",\n";
    oss << "  \"bytes_sent\": " << total_bytes_sent.load() << ",\n";
    oss << "  \"bytes_received\": " << total_bytes_received.load() << "\n";
    oss << "}\n";
    return oss.str();
}

std::string Statistics::get_top_hosts(int limit) const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    
    std::vector<std::pair<std::string, unsigned long long>> hosts;
    for (const auto& pair : per_host_stats) {
        hosts.push_back({pair.first, pair.second.requests});
    }
    
    std::sort(hosts.begin(), hosts.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::ostringstream oss;
    oss << "\nTop " << std::min(limit, (int)hosts.size()) << " Hosts by Request Count:\n";
    oss << "----------------------------------------\n";
    
    int count = 0;
    for (const auto& host : hosts) {
        if (count++ >= limit) break;
        oss << count << ". " << host.first << ": " << host.second << " requests\n";
    }
    
    return oss.str();
}

std::string Statistics::get_client_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    
    std::ostringstream oss;
    oss << "\nClient IP Statistics:\n";
    oss << "----------------------------------------\n";
    
    for (const auto& pair : ip_request_count) {
        oss << pair.first << ": " << pair.second << " requests\n";
    }
    
    return oss.str();
}

void Statistics::reset() {
    total_requests = 0;
    total_cached = 0;
    total_blocked = 0;
    total_errors = 0;
    total_bytes_sent = 0;
    total_bytes_received = 0;
    
    std::lock_guard<std::mutex> lock(stats_mutex);
    per_host_stats.clear();
    ip_request_count.clear();
    
    start_time = std::chrono::system_clock::now();
}

double Statistics::get_uptime_seconds() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    return duration.count();
}
