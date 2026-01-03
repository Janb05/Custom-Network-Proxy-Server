#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>
#include <fstream>
#include <ctime>
#include <sys/select.h>
#include <sys/stat.h>
#include <atomic>

#define BUFFER_SIZE 8192
int PORT = 8080;
int CACHE_LIMIT = 50;


// ================= STRUCTS =================
struct CacheEntry {
    std::string data;
    time_t timestamp;
};
std::atomic<time_t> last_config_mtime(0);
std::mutex config_mutex;

// cache[host] = { CacheEntry, iterator_in_lru }
std::unordered_map<std::string, std::pair<CacheEntry, std::list<std::string>::iterator>> cache;
std::list<std::string> lru;
std::mutex cache_mutex;
std::unordered_set<std::string> blocked;


// ================= LOGGING =================
std::string now() {
    time_t t = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return std::string(buf);
}

void log_request(const std::string &ip, const std::string &host, const std::string &status) {
    std::ofstream log("proxy.log", std::ios::app);
    log << "[" << now() << "] " << ip << " -> " << host << " [" << status << "]\n";
}

// ================= TUNNEL =================
void tunnel(int client, int remote) {
    char buffer[BUFFER_SIZE];
    fd_set fds;

    while (true) {
        FD_ZERO(&fds);
        FD_SET(client, &fds);
        FD_SET(remote, &fds);

        if (select(std::max(client, remote) + 1, &fds, nullptr, nullptr, nullptr) <= 0)
            break;

        if (FD_ISSET(client, &fds)) {
            int n = recv(client, buffer, BUFFER_SIZE, 0);
            if (n <= 0) break;
            send(remote, buffer, n, 0);
        }

        if (FD_ISSET(remote, &fds)) {
            int n = recv(remote, buffer, BUFFER_SIZE, 0);
            if (n <= 0) break;
            send(client, buffer, n, 0);
        }
    }
}

// ================= CLIENT HANDLER =================
void handle_client(int client) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(client, (sockaddr*)&addr, &len);
    std::string client_ip = inet_ntoa(addr.sin_addr);

    int bytes = recv(client, buffer, BUFFER_SIZE, 0);
    if (bytes <= 0) {
        close(client);
        return;
    }

    std::string request(buffer);

    // ---------------- HTTPS ----------------
    if (request.find("CONNECT") == 0) {
        size_t p1 = request.find(" ");
        size_t p2 = request.find(" ", p1 + 1);
        std::string hostport = request.substr(p1 + 1, p2 - p1 - 1);
        std::string host = hostport.substr(0, hostport.find(":"));
        int port = std::stoi(hostport.substr(hostport.find(":") + 1));

        if (blocked.count(host)) {
            send(client, "HTTP/1.1 403 Forbidden\r\n\r\n", 26, 0);
            close(client);
            return;
        }

        struct hostent *server = gethostbyname(host.c_str());
        if (!server) return;

        int remote = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        memcpy(&addr.sin_addr, server->h_addr, server->h_length);

        connect(remote, (sockaddr*)&addr, sizeof(addr));
        send(client, "HTTP/1.1 200 Connection Established\r\n\r\n", 39, 0);

        log_request(client_ip, host, "HTTPS_TUNNEL");

        tunnel(client, remote);
        close(remote);
        close(client);
        return;
    }

    // ---------------- HTTP ----------------
    std::string host;
    size_t hpos = request.find("Host:");
    if (hpos != std::string::npos) {
        size_t s = hpos + 6;
        size_t e = request.find("\r\n", s);
        host = request.substr(s, e - s);
    }

    if (blocked.count(host)) {
        send(client, "HTTP/1.1 403 Forbidden\r\n\r\n", 26, 0);
        close(client);
        return;
    }

    // -------- CACHE HIT --------
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (cache.count(host)) {
            auto &entry = cache[host];

            lru.erase(entry.second);
            lru.push_front(host);
            entry.second = lru.begin();

            send(client, entry.first.data.c_str(), entry.first.data.size(), 0);
            log_request(client_ip, host, "CACHED");
            close(client);
            return;
        }
    }

    // -------- FETCH FROM INTERNET --------
    struct hostent *server = gethostbyname(host.c_str());
    if (!server) return;

    int remote = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in raddr{};
    raddr.sin_family = AF_INET;
    raddr.sin_port = htons(80);
    memcpy(&raddr.sin_addr, server->h_addr, server->h_length);

    connect(remote, (sockaddr*)&raddr, sizeof(raddr));

    size_t line_end = request.find("\r\n");
    std::string first = request.substr(0, line_end);
    size_t p1 = first.find(" ");
    size_t p2 = first.find(" ", p1 + 1);
    std::string path = first.substr(p1 + 1, p2 - p1 - 1);

    if (path.find("http://") == 0) {
        size_t slash = path.find('/', 7);
        path = slash == std::string::npos ? "/" : path.substr(slash);
    }

    std::string new_req =
        "GET " + path + " HTTP/1.0\r\n"
        "Host: " + host + "\r\n"
        "Connection: close\r\n\r\n";

    send(remote, new_req.c_str(), new_req.size(), 0);

    std::string response;
    int n;
    while ((n = recv(remote, buffer, BUFFER_SIZE, 0)) > 0)
        response.append(buffer, n);

    {
        std::lock_guard<std::mutex> lock(cache_mutex);

        if (cache.size() >= CACHE_LIMIT) {
            std::string old = lru.back();
            lru.pop_back();
            cache.erase(old);
        }

        lru.push_front(host);
        cache[host] = { { response, time(nullptr) }, lru.begin() };
    }

    send(client, response.c_str(), response.size(), 0);
    log_request(client_ip, host, "FETCHED");

    close(remote);
    close(client);
}
void load_config(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "⚠️ Could not open config file\n";
        return;
    }

    std::unordered_set<std::string> new_blocked;
    int new_port = PORT;
    int new_cache_limit = CACHE_LIMIT;

    std::string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find("PORT=") == 0) {
            new_port = std::stoi(line.substr(5));
        }
        else if (line.find("CACHE_LIMIT=") == 0) {
            new_cache_limit = std::stoi(line.substr(12));
        }
        else if (line.find("BLOCK=") == 0) {
            new_blocked.insert(line.substr(6));
        }
    }

    {
        std::lock_guard<std::mutex> lock(config_mutex);
        PORT = new_port;
        CACHE_LIMIT = new_cache_limit;
        blocked = new_blocked;
    }

    std::cout << "🔁 Config reloaded\n";
}
void watch_config(const std::string &filename) {
    struct stat st{};
    
    while (true) {
        if (stat(filename.c_str(), &st) == 0) {
            if (st.st_mtime != last_config_mtime.load()) {
                last_config_mtime = st.st_mtime;
                load_config(filename);
            }
        }
        sleep(2); // check every 2 seconds
    }
}


// ================= MAIN =================
int main() {
    load_config("config.txt");

    std::thread(watch_config, "config.txt").detach();

    int server = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server, (sockaddr*)&addr, sizeof(addr));
    listen(server, 10);

    std::cout << "🚀 Proxy running on port " << PORT << std::endl;

    while (true) {
        sockaddr_in client;
        socklen_t len = sizeof(client);
        int sock = accept(server, (sockaddr*)&client, &len);
        if (sock >= 0)
            std::thread(handle_client, sock).detach();
    }
}


