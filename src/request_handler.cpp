#include "../include/request_handler.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <chrono>

#define BUFFER_SIZE 8192

RequestHandler::RequestHandler(Logger* log, CacheManager* cache_mgr, 
                               ConfigManager* config_mgr, Statistics* stats_mgr)
    : logger(log), cache(cache_mgr), config(config_mgr), stats(stats_mgr) {
}

void RequestHandler::tunnel(int client, int remote) {
    char buffer[BUFFER_SIZE];
    fd_set fds;
    struct timeval timeout;

    while (true) {
        FD_ZERO(&fds);
        FD_SET(client, &fds);
        FD_SET(remote, &fds);
        
        timeout.tv_sec = config->get_connection_timeout();
        timeout.tv_usec = 0;

        int result = select(std::max(client, remote) + 1, &fds, nullptr, nullptr, &timeout);
        if (result <= 0) break;

        if (FD_ISSET(client, &fds)) {
            int n = recv(client, buffer, BUFFER_SIZE, 0);
            if (n <= 0) break;
            if (send(remote, buffer, n, 0) <= 0) break;
        }

        if (FD_ISSET(remote, &fds)) {
            int n = recv(remote, buffer, BUFFER_SIZE, 0);
            if (n <= 0) break;
            if (send(client, buffer, n, 0) <= 0) break;
        }
    }
}

std::string RequestHandler::extract_host(const std::string& request) {
    size_t hpos = request.find("Host:");
    if (hpos != std::string::npos) {
        size_t s = hpos + 6;
        while (s < request.size() && request[s] == ' ') s++;
        size_t e = request.find("\r\n", s);
        if (e != std::string::npos) {
            return request.substr(s, e - s);
        }
    }
    return "";
}

std::string RequestHandler::extract_path(const std::string& request) {
    size_t line_end = request.find("\r\n");
    if (line_end == std::string::npos) return "/";
    
    std::string first = request.substr(0, line_end);
    size_t p1 = first.find(" ");
    size_t p2 = first.find(" ", p1 + 1);
    
    if (p1 == std::string::npos || p2 == std::string::npos) return "/";
    
    std::string path = first.substr(p1 + 1, p2 - p1 - 1);
    
    if (path.find("http://") == 0) {
        size_t slash = path.find('/', 7);
        path = (slash == std::string::npos) ? "/" : path.substr(slash);
    }
    
    return path;
}

int RequestHandler::connect_to_host(const std::string& host, int port) {
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        logger->error("DNS lookup failed for: " + host);
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logger->error("Socket creation failed");
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, server->h_addr, server->h_length);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        logger->error("Connection failed to: " + host);
        close(sock);
        return -1;
    }

    return sock;
}

void RequestHandler::send_forbidden(int client) {
    const char* response = "HTTP/1.1 403 Forbidden\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: 45\r\n"
                          "\r\n"
                          "<html><body><h1>403 Forbidden</h1></body></html>";
    send(client, response, strlen(response), 0);
}

void RequestHandler::send_error(int client, const std::string& message) {
    std::string response = "HTTP/1.1 500 Internal Server Error\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: " + std::to_string(message.size()) + "\r\n"
                          "\r\n" + message;
    send(client, response.c_str(), response.size(), 0);
}

bool RequestHandler::handle_https_connect(int client, const std::string& request, 
                                          const std::string& client_ip) {
    size_t p1 = request.find(" ");
    size_t p2 = request.find(" ", p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos) {
        send_error(client, "Malformed CONNECT request");
        return false;
    }
    
    std::string hostport = request.substr(p1 + 1, p2 - p1 - 1);
    size_t colon = hostport.find(":");
    std::string host = hostport.substr(0, colon);
    int port = (colon != std::string::npos) ? std::stoi(hostport.substr(colon + 1)) : 443;

    if (config->is_blocked(host)) {
        logger->log_request(client_ip, host, "BLOCKED_HTTPS");
        stats->record_blocked_request();
        send_forbidden(client);
        return false;
    }

    auto start_time = std::chrono::steady_clock::now();
    
    int remote = connect_to_host(host, port);
    if (remote < 0) {
        send_error(client, "Failed to connect to remote host");
        stats->record_error();
        return false;
    }

    const char* established = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(client, established, strlen(established), 0);

    logger->log_request(client_ip, host, "HTTPS_TUNNEL");
    logger->log_url(client_ip, "https://" + host, "CONNECT");
    stats->record_request(host, client_ip);

    tunnel(client, remote);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    stats->record_time(host, duration);
    
    close(remote);
    return true;
}

bool RequestHandler::handle_http_request(int client, const std::string& request, 
                                        const std::string& client_ip) {
    std::string host = extract_host(request);
    if (host.empty()) {
        send_error(client, "No Host header found");
        return false;
    }
    
    std::string path = extract_path(request);
    std::string full_url = "http://" + host + path;
    std::string method = request.substr(0, request.find(" "));
    
    // Log the complete URL
    logger->log_url(client_ip, full_url, method);

    if (config->is_blocked(host)) {
        logger->log_request(client_ip, host, "BLOCKED_HTTP");
        stats->record_blocked_request();
        send_forbidden(client);
        return false;
    }

    // Check cache
    std::string cached_data;
    if (cache->get(host, cached_data)) {
        send(client, cached_data.c_str(), cached_data.size(), 0);
        logger->log_request(client_ip, host, "CACHED", cached_data.size());
        stats->record_request(host, client_ip);
        stats->record_cached_request();
        stats->record_bytes(host, cached_data.size(), 0);
        return true;
    }

    // Fetch from internet
    auto start_time = std::chrono::steady_clock::now();
    
    int remote = connect_to_host(host, 80);
    if (remote < 0) {
        send_error(client, "Failed to connect to remote host");
        stats->record_error();
        return false;
    }

    std::string new_req = "GET " + path + " HTTP/1.0\r\n"
                         "Host: " + host + "\r\n"
                         "Connection: close\r\n"
                         "\r\n";

    if (send(remote, new_req.c_str(), new_req.size(), 0) <= 0) {
        logger->error("Failed to send request to remote host");
        close(remote);
        stats->record_error();
        return false;
    }

    std::string response;
    char buffer[BUFFER_SIZE];
    int n;
    
    while ((n = recv(remote, buffer, BUFFER_SIZE, 0)) > 0) {
        response.append(buffer, n);
    }
    
    close(remote);

    if (response.empty()) {
        send_error(client, "Empty response from server");
        stats->record_error();
        return false;
    }

    // Cache the response
    cache->put(host, response, config->get_cache_ttl());

    // Send to client
    send(client, response.c_str(), response.size(), 0);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    logger->log_request(client_ip, host, "FETCHED", response.size());
    stats->record_request(host, client_ip);
    stats->record_bytes(host, response.size(), new_req.size());
    stats->record_time(host, duration);

    return true;
}

void RequestHandler::handle_client(int client) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(client, (sockaddr*)&addr, &len) < 0) {
        logger->error("Failed to get client address");
        close(client);
        return;
    }
    
    std::string client_ip = inet_ntoa(addr.sin_addr);

    int bytes = recv(client, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) {
        close(client);
        return;
    }

    std::string request(buffer, bytes);

    // Check for /stats endpoint (direct access without proxy)
    if (request.find("GET /stats") == 0 || request.find("GET /stats ") != std::string::npos) {
        if (!stats) {
            std::string response = "HTTP/1.1 404 Not Found\r\n\r\nStats not enabled";
            send(client, response.c_str(), response.size(), 0);
        } else {
            std::string stats_json = stats->get_json_stats();
            std::string response = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: " + std::to_string(stats_json.size()) + "\r\n"
                                  "\r\n" + stats_json;
            send(client, response.c_str(), response.size(), 0);
        }
    }
    // Handle HTTPS CONNECT
    else if (request.find("CONNECT") == 0) {
        handle_https_connect(client, request, client_ip);
    }
    // Handle HTTP
    else {
        handle_http_request(client, request, client_ip);
    }

    close(client);
}
