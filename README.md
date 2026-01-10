# Advanced Proxy Server

A high-performance, multi-threaded HTTP/HTTPS proxy server with intelligent caching, request filtering, and real-time statistics.

## Demo Video

[![Proxy Server Demo](https://img.youtube.com/vi/mEdxokwJN7I/maxresdefault.jpg)](https://youtube.com/shorts/mEdxokwJN7I?feature=share)

**Click to watch the demonstration video**

## Features

### Core Functionality
- **HTTP & HTTPS Support** - Full proxy for both protocols with CONNECT tunneling
- **Multi-threaded** - Handles multiple concurrent connections efficiently
- **Intelligent Caching** - LRU cache with TTL (Time-To-Live) expiration
- **Domain Blocking** - Block/whitelist specific domains
- **Hot Reload Config** - Configuration changes apply without restart
- **Advanced Logging** - Multiple log levels (DEBUG, INFO, WARN, ERROR)
- **Statistics Tracking** - Real-time performance and usage metrics

### Cache Features
- LRU (Least Recently Used) eviction policy
- Configurable cache size limit (entries + bytes)
- Per-entry TTL with automatic cleanup
- Cache hit/miss statistics

### Security & Control
- Domain blacklisting
- Domain whitelisting (allow-only mode)
- Connection timeouts
- Request validation

### Monitoring
- Total requests, cached, blocked
- Per-host statistics
- Per-client IP tracking
- Bandwidth usage tracking
- Response time measurements
- Cache hit rate

## Project Structure

```
proxy-server/
├── src/
│   ├── main.cpp              # Entry point
│   ├── proxy_server.cpp      # Main server logic
│   ├── request_handler.cpp   # HTTP/HTTPS handling
│   ├── cache_manager.cpp     # Intelligent caching
│   ├── config_manager.cpp    # Configuration management
│   ├── logger.cpp            # Multi-level logging
│   └── statistics.cpp        # Stats tracking
├── include/
│   ├── proxy_server.h
│   ├── request_handler.h
│   ├── cache_manager.h
│   ├── config_manager.h
│   ├── logger.h
│   └── statistics.h
├── logs/
│   └── proxy.log
├── config.txt                # Configuration file
├── Makefile                  # Build system
└── README.md                 # This file
```

## Building

### Requirements
- C++17 compatible compiler (g++, clang++)
- POSIX-compliant system (Linux, macOS)
- pthread library

### Compile

```bash
# Standard build
make

# Debug build (with symbols)
make debug

# Optimized release build
make release

# Clean build artifacts
make clean
```

## Usage

### Starting the Server

```bash
# Run with default config (config.txt)
make run

# Or directly
./proxy_server

# Run with custom config
./proxy_server my_config.txt
```

### Configuration

Edit `config.txt` to customize behavior:

```ini
# Server settings
PORT=9090
CONNECTION_TIMEOUT=30

# Cache configuration
CACHE_LIMIT=100              # Max cached entries
CACHE_TTL=3600              # Time-to-live in seconds
MAX_CACHE_SIZE_MB=100       # Max cache size in MB

# Logging
LOG_LEVEL=INFO              # DEBUG, INFO, WARN, ERROR

# Statistics
ENABLE_STATS=true

# Block specific domains
BLOCK=instagram.com
BLOCK=youtube.com
BLOCK=ads.example.com

# Whitelist mode (optional)
# WHITELIST=google.com
# WHITELIST=github.com
```

**Note:** Configuration changes are detected automatically and applied without restart!

### Using the Proxy

Configure your application or system to use the proxy:

```bash
# HTTP proxy
curl -x http://localhost:9090 http://example.com

# HTTPS proxy
curl -x http://localhost:9090 https://example.com

# Set system-wide proxy (macOS/Linux)
export http_proxy=http://localhost:9090
export https_proxy=http://localhost:9090
```

### Browser Configuration

**Firefox:**
1. Preferences → Network Settings
2. Manual proxy configuration
3. HTTP Proxy: `localhost`, Port: `9090`
4. Also use this proxy for HTTPS

**Chrome/Safari:**
- Use system proxy settings or extensions

## Monitoring

The server logs all activity to `logs/proxy.log` with timestamps and status codes.

### Log Entry Format
```
[2026-01-03 12:34:56] [INFO ] 127.0.0.1 -> example.com [CACHED] (4096 bytes)
[2026-01-03 12:34:57] [INFO ] 127.0.0.1 -> blocked.com [BLOCKED_HTTP]
[2026-01-03 12:34:58] [INFO ] 127.0.0.1 -> google.com [FETCHED] (8192 bytes)
```

### Statistics

When the server stops (Ctrl+C), it displays comprehensive statistics:

```
========== PROXY SERVER STATISTICS ==========
Uptime: 3600.00 seconds
Total Requests: 1000
  - Cached: 450
  - Blocked: 50
  - Errors: 5
Bytes Sent: 10485760 bytes
Bytes Received: 5242880 bytes
Cache Hit Rate: 45.00%
============================================
```

## Testing

Test the proxy functionality:

```bash
# Test HTTP
curl -v -x http://localhost:9090 http://example.com

# Test HTTPS
curl -v -x http://localhost:9090 https://example.com

# Test blocking
curl -v -x http://localhost:9090 http://instagram.com
# Should return: 403 Forbidden

# Test caching (run twice, second should be faster)
time curl -x http://localhost:9090 http://example.com
time curl -x http://localhost:9090 http://example.com
```

## Architecture

### Modular Design

The project follows a clean, modular architecture:

1. **ProxyServer** - Main server class, manages lifecycle
2. **RequestHandler** - Processes HTTP/HTTPS requests
3. **CacheManager** - Intelligent caching with LRU + TTL
4. **ConfigManager** - Hot-reload configuration
5. **Logger** - Multi-level logging system
6. **Statistics** - Real-time metrics tracking

### Threading Model

- Main thread accepts connections
- Each client spawns a new thread
- Thread-safe data structures with mutexes
- Background threads for:
  - Config file watching
  - Cache cleanup
  - Statistics aggregation

### Cache Strategy

- **LRU (Least Recently Used)** eviction
- **TTL (Time-To-Live)** expiration
- **Size-based** limits (entries + bytes)
- **Automatic cleanup** every 5 minutes

## Security Considerations

**Important:** This is an educational proxy server. For production use:

- Add authentication (Basic Auth, Token-based)
- Implement SSL/TLS for proxy connection
- Add rate limiting per IP
- Validate and sanitize all inputs
- Add MITM detection
- Implement proper access controls

## 🐛 Troubleshooting

### Port Already in Use
```bash
# Find process using port
lsof -i :9090

# Kill process
kill -9 <PID>
```

### Permission Denied (ports < 1024)
```bash
# Run with sudo for privileged ports
sudo ./proxy_server
```

### Connection Timeout
- Increase `CONNECTION_TIMEOUT` in config
- Check firewall settings
- Verify DNS resolution

## Advanced Features

### Custom Cache TTL per Domain
Modify `cache_manager.cpp` to set different TTLs based on hostname.

### Content Filtering
Add filtering logic in `request_handler.cpp` to block by content-type or inject headers.

### Parent Proxy Chaining
Modify connection logic to route through another proxy.

### Statistics API
Add an HTTP endpoint in `proxy_server.cpp` to serve JSON statistics.

## Contributing

This is an educational project. Feel free to:
- Report bugs
- Suggest features
- Submit improvements
- Fork and modify

## License

This project is open-source and available for educational purposes.

## Authors

Created as a learning project for understanding network programming, proxy servers, and modern C++ development.

## Learning Resources

- [HTTP/1.1 Specification](https://tools.ietf.org/html/rfc7230)
- [CONNECT Method](https://tools.ietf.org/html/rfc7231#section-4.3.6)
- [C++ Network Programming](https://beej.us/guide/bgnet/)
- [Proxy Server Basics](https://en.wikipedia.org/wiki/Proxy_server)

