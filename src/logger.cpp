#include "../include/logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger::Logger(const std::string& filename, LogLevel level) 
    : log_file(filename), min_level(level) {
    file.open(log_file, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open log file: " << log_file << std::endl;
    }
}

Logger::~Logger() {
    if (file.is_open()) {
        file.close();
    }
}

std::string Logger::get_timestamp() {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buf);
}

std::string Logger::level_to_string(LogLevel level) {
    switch(level) {
        case DEBUG: return "DEBUG";
        case INFO:  return "INFO ";
        case WARN:  return "WARN ";
        case ERROR: return "ERROR";
        default:    return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < min_level) return;
    
    std::lock_guard<std::mutex> lock(log_mutex);
    
    std::string entry = "[" + get_timestamp() + "] [" + 
                       level_to_string(level) + "] " + message;
    
    if (file.is_open()) {
        file << entry << std::endl;
        file.flush();
    }
    
    // Also print to console for important messages
    if (level >= WARN) {
        std::cerr << entry << std::endl;
    } else if (level == INFO) {
        std::cout << entry << std::endl;
    }
}

void Logger::debug(const std::string& message) {
    log(DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(INFO, message);
}

void Logger::warn(const std::string& message) {
    log(WARN, message);
}

void Logger::error(const std::string& message) {
    log(ERROR, message);
}

void Logger::log_request(const std::string& ip, const std::string& host, 
                        const std::string& status, size_t bytes) {
    std::ostringstream oss;
    oss << ip << " -> " << host << " [" << status << "]";
    if (bytes > 0) {
        oss << " (" << bytes << " bytes)";
    }
    info(oss.str());
}

void Logger::log_url(const std::string& ip, const std::string& url, const std::string& method) {
    std::ostringstream oss;
    oss << "URL_LOG: " << ip << " " << method << " " << url;
    info(oss.str());
}

void Logger::set_level(LogLevel level) {
    min_level = level;
}
