#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

enum LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
private:
    std::string log_file;
    LogLevel min_level;
    std::mutex log_mutex;
    std::ofstream file;

    std::string get_timestamp();
    std::string level_to_string(LogLevel level);

public:
    Logger(const std::string& filename, LogLevel level = INFO);
    ~Logger();

    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    
    void log_request(const std::string& ip, const std::string& host, 
                     const std::string& status, size_t bytes = 0);
    
    void set_level(LogLevel level);
};

#endif // LOGGER_H
