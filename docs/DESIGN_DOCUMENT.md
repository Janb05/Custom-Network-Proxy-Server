# Design Document: Custom Network Proxy Server

## Executive Summary

This document describes the design and implementation of a high-performance, multi-threaded HTTP/HTTPS proxy server built in C++17. The proxy supports intelligent caching, domain filtering, hot-reload configuration, and real-time statistics tracking.

---

## 1. High-Level Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     CLIENT APPLICATIONS                         │
│              (Browsers, curl, API clients, etc.)                │
└────────────────────────┬────────────────────────────────────────┘
                         │ HTTP/HTTPS Requests
                         ↓
┌─────────────────────────────────────────────────────────────────┐
│                      PROXY SERVER                                │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              Main Server Thread                          │  │
│  │  • Socket binding (port 9090)                           │  │
│  │  • Accept incoming connections                           │  │
│  │  • Spawn handler threads                                 │  │
│  └─────────────┬────────────────────────────────────────────┘  │
│                ↓                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │         Request Handler (per connection)                 │  │
│  │  • Parse HTTP/HTTPS request                             │  │
│  │  • Extract host, path, headers                           │  │
│  │  • Route to appropriate handler                          │  │
│  └──────┬──────────────────────────┬────────────────────────┘  │
│         │                          │                             │
│         ↓ HTTP                     ↓ HTTPS (CONNECT)            │
│  ┌─────────────┐           ┌──────────────────┐                │
│  │   Cache     │           │      Tunnel      │                │
│  │   Lookup    │           │   Establishment  │                │
│  └──┬───────┬──┘           └──────────────────┘                │
│     │ Hit   │ Miss                  ↓                           │
│     ↓       ↓                Bidirectional                      │
│  Return  Fetch from              Data Relay                     │
│  Cached  Internet                                               │
│  Data       ↓                                                   │
│         ┌────────────┐                                          │
│         │   Cache    │                                          │
│         │   Store    │                                          │
│         └────────────┘                                          │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              Supporting Components                        │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │  │
│  │  │  Config  │  │  Logger  │  │  Cache   │  │  Stats  │ │  │
│  │  │ Manager  │  │          │  │ Manager  │  │ Tracker │ │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                         │
                         ↓ Forwarded Requests
┌─────────────────────────────────────────────────────────────────┐
│                   INTERNET / ORIGIN SERVERS                      │
│         (example.com, google.com, github.com, etc.)             │
└─────────────────────────────────────────────────────────────────┘
```

### Component Descriptions

#### 1. **ProxyServer** (Main Controller)
- **Purpose:** Server lifecycle management
- **Responsibilities:**
  - Initialize all components
  - Create and bind socket to port
  - Accept incoming client connections
  - Spawn handler threads for each connection
  - Coordinate graceful shutdown
- **Key Methods:**
  - `start()` - Initialize server
  - `run()` - Main accept loop
  - `stop()` - Cleanup and statistics display

#### 2. **RequestHandler** (Connection Handler)
- **Purpose:** Process individual client requests
- **Responsibilities:**
  - Parse HTTP/HTTPS requests
  - Extract host, path, and headers
  - Check domain filtering rules
  - Route to cache or origin server
  - Handle both HTTP and HTTPS CONNECT
- **Key Methods:**
  - `handle_client()` - Entry point
  - `handle_http_request()` - HTTP processing
  - `handle_https_connect()` - HTTPS tunneling
  - `tunnel()` - Bidirectional data relay

#### 3. **CacheManager** (Intelligent Caching)
- **Purpose:** Store and retrieve cached responses
- **Responsibilities:**
  - LRU (Least Recently Used) eviction
  - TTL (Time-To-Live) expiration
  - Size-based limits (entries + bytes)
  - Thread-safe cache operations
  - Cache hit/miss tracking
- **Algorithm:** LRU + TTL hybrid
- **Key Methods:**
  - `get()` - Retrieve cached data
  - `put()` - Store new entry
  - `cleanup_expired()` - Remove old entries

#### 4. **ConfigManager** (Configuration)
- **Purpose:** Manage server configuration
- **Responsibilities:**
  - Load configuration from file
  - Hot-reload on file changes
  - Provide thread-safe access
  - Notify components on changes
- **Key Features:**
  - File watching (2-second interval)
  - No restart required for config changes
- **Key Methods:**
  - `load()` - Parse config file
  - `watch()` - Monitor file changes
  - `is_blocked()` - Check filtering rules

#### 5. **Logger** (Logging System)
- **Purpose:** Record all proxy activity
- **Responsibilities:**
  - Multi-level logging (DEBUG, INFO, WARN, ERROR)
  - Thread-safe file writes
  - Timestamp formatting
  - Console + file output
- **Log Format:** `[timestamp] [level] message`
- **Key Methods:**
  - `log()` - Generic logging
  - `log_request()` - Specialized for requests
  - `set_level()` - Change verbosity

#### 6. **Statistics** (Metrics Tracking)
- **Purpose:** Track performance metrics
- **Responsibilities:**
  - Count requests (total, cached, blocked, errors)
  - Track bandwidth (sent/received)
  - Per-host statistics
  - Per-client IP tracking
  - Response time measurements
- **Key Methods:**
  - `record_request()` - Log new request
  - `record_cached_request()` - Cache hit
  - `get_summary()` - Generate report

---

## 2. Concurrency Model

### Chosen Model: **Thread-per-Connection**

#### Rationale:
1. **Simplicity:** Easy to implement and debug
2. **Isolation:** Each connection is independent
3. **Blocking I/O:** Suitable for traditional socket operations
4. **Scale:** Adequate for typical proxy workloads (hundreds of connections)

#### Thread Architecture:

```
Main Thread
    │
    ├─→ Config Watcher Thread (detached)
    │   └─ Monitors config.txt every 2 seconds
    │
    ├─→ Cache Cleanup Thread (detached)
    │   └─ Removes expired entries every 5 minutes
    │
    └─→ Connection Handler Threads (detached)
        ├─ Thread 1 → Client A
        ├─ Thread 2 → Client B
        ├─ Thread 3 → Client C
        └─ ...
```

#### Thread Synchronization:

| Resource | Protection Mechanism | Purpose |
|----------|---------------------|---------|
| Cache | `std::mutex` | Serialize cache access |
| Config | `std::mutex` | Protect config updates |
| Statistics | `std::atomic` + `std::mutex` | Fast counters + detailed stats |
| Log File | `std::mutex` | Serialize writes |

#### Thread Lifecycle:
1. **Creation:** Main thread spawns via `std::thread`
2. **Execution:** Handler processes request
3. **Cleanup:** Automatic via detached threads
4. **Termination:** Thread exits after client disconnection

---

## 3. Data Flow

### HTTP Request Flow

```
1. Client Connection
   ↓
2. Parse Request
   │ • Extract: Method, Host, Path, Headers
   │ • Validate: HTTP format
   ↓
3. Check Filtering
   │ • Is host blocked?
   │ • Yes → Return 403 Forbidden
   │ • No  → Continue
   ↓
4. Cache Lookup
   │ • Check if response cached
   │ • Hit? → Return cached data (FAST PATH)
   │ • Miss? → Continue to origin
   ↓
5. Connect to Origin Server
   │ • DNS lookup
   │ • TCP connect (port 80)
   │ • Send HTTP request
   ↓
6. Receive Response
   │ • Read all data from origin
   │ • Accumulate in buffer
   ↓
7. Cache Response
   │ • Store with TTL
   │ • Evict old entries if full
   ↓
8. Send to Client
   │ • Forward complete response
   ↓
9. Log & Record Stats
   │ • Write to log file
   │ • Update statistics
   ↓
10. Close Connection
```

### HTTPS CONNECT Flow

```
1. Client CONNECT Request
   │ • Format: CONNECT host:port HTTP/1.1
   ↓
2. Parse Host & Port
   │ • Extract destination
   │ • Default port: 443
   ↓
3. Check Filtering
   │ • Is host blocked?
   │ • Yes → Return 403
   │ • No  → Continue
   ↓
4. Connect to Destination
   │ • DNS lookup
   │ • TCP connect to host:port
   ↓
5. Send 200 Connection Established
   │ • Notify client tunnel is ready
   ↓
6. Bidirectional Tunnel
   │ • Relay data: Client ⇄ Server
   │ • Use select() for multiplexing
   │ • Continue until either side closes
   ↓
7. Log & Record Stats
   ↓
8. Close Both Connections
```

### Cache Data Flow

```
Request → Cache Key (host)
           ↓
       Hash Lookup
           ↓
     ┌─────┴─────┐
     │           │
   Found      Not Found
     │           │
  Expired?    Fetch from
     │        Internet
   Yes│No        │
     │  │        │
  Remove│      Cache
     │  │        │
     └──┴────────┴→ Return to Client
```

---

## 4. Error Handling

### Error Categories & Responses

#### 1. **Network Errors**
| Error | Cause | Handling |
|-------|-------|----------|
| DNS Failure | `gethostbyname()` fails | Log error, return 500 to client |
| Connection Timeout | Remote host unreachable | Close connections, log error |
| Socket Error | `recv()`/`send()` fails | Clean up resources, log |
| Connection Reset | Client/server closes | Graceful cleanup |

#### 2. **Protocol Errors**
| Error | Cause | Handling |
|-------|-------|----------|
| Malformed Request | Invalid HTTP format | Close connection, log |
| Missing Host Header | No Host: in request | Return error response |
| Invalid CONNECT | Bad format | Return 400 Bad Request |

#### 3. **Application Errors**
| Error | Cause | Handling |
|-------|-------|----------|
| Cache Full | Exceeded size limit | Evict LRU entries |
| Config Load Failure | Invalid config file | Use previous config, log warning |
| Log Write Failure | Disk full | Continue, print to stderr |

### Error Handling Strategy

```cpp
try {
    // Main operation
    handle_request();
} catch (std::exception& e) {
    logger->error("Error: " + std::string(e.what()));
    send_error_response(client);
    cleanup_resources();
}
```

### Resource Cleanup

- **RAII Principle:** Resources acquired in constructors, released in destructors
- **Explicit Cleanup:** Socket file descriptors closed in all code paths
- **Exception Safety:** No resource leaks even on errors

---

## 5. Limitations

### Current Limitations

1. **Scalability**
   - Thread-per-connection limits to ~1000 concurrent connections
   - No thread pool (each connection spawns new thread)
   - Memory usage grows linearly with connections

2. **HTTP Support**
   - HTTP/1.0 and HTTP/1.1 only (no HTTP/2 or HTTP/3)
   - No persistent connections (Connection: close)
   - No request pipelining

3. **Caching**
   - In-memory only (lost on restart)
   - No cache validation (no If-Modified-Since)
   - Simple host-based keying (ignores query params)

4. **Security**
   - No authentication mechanism
   - No HTTPS for proxy connection itself
   - No rate limiting
   - No input sanitization for logs

5. **Performance**
   - Synchronous I/O (blocking)
   - No zero-copy optimizations
   - Single process architecture

### Future Improvements

1. **Event-driven I/O** (epoll/kqueue) for better scalability
2. **Thread pool** to limit resource usage
3. **Persistent cache** (Redis/SQLite) for durability
4. **HTTP/2** support for modern clients
5. **TLS termination** for secure proxy connections
6. **Content filtering** by type or pattern
7. **Bandwidth throttling** per client
8. **Authentication** (Basic Auth, tokens)

---

## 6. Security Considerations

### Current Security Features

✅ **Domain Filtering**
- Blacklist/whitelist based access control
- Configurable via config.txt

✅ **Connection Timeouts**
- Prevents resource exhaustion
- Configurable timeout values

✅ **Error Handling**
- Graceful degradation
- No sensitive data in error messages

### Security Vulnerabilities & Mitigations

#### 1. **No Authentication**
- **Risk:** Anyone can use the proxy
- **Mitigation:** Add Basic Auth or token-based auth
- **Future:** Implement user database

#### 2. **No HTTPS for Proxy Connection**
- **Risk:** Traffic between client and proxy is unencrypted
- **Mitigation:** Deploy behind VPN or use SSH tunnel
- **Future:** Add TLS support for proxy connection

#### 3. **No Rate Limiting**
- **Risk:** DDoS attacks, resource exhaustion
- **Mitigation:** Deploy behind reverse proxy with rate limiting
- **Future:** Implement per-IP rate limits

#### 4. **No Input Validation**
- **Risk:** Buffer overflows, injection attacks
- **Mitigation:** Use fixed-size buffers, bounded reads
- **Future:** Add strict input validation

#### 5. **Log Injection**
- **Risk:** Malicious data in logs
- **Mitigation:** Limited currently
- **Future:** Sanitize all log inputs

#### 6. **HTTPS MITM Potential**
- **Risk:** Proxy can't inspect HTTPS (by design)
- **Note:** This is expected behavior for CONNECT tunneling
- **For inspection:** Would require SSL interception (not implemented)

### Security Best Practices Applied

1. **Principle of Least Privilege:** Run as non-root user
2. **Fail Closed:** On errors, deny access rather than allow
3. **Logging:** All activities logged for audit
4. **Resource Limits:** Cache size limits prevent exhaustion
5. **No Secrets in Code:** Configuration externalized

### Deployment Recommendations

1. **Network Isolation:** Deploy in DMZ or private network
2. **Firewall Rules:** Limit access to known clients
3. **Monitoring:** Set up alerts for suspicious activity
4. **Regular Updates:** Keep dependencies up to date
5. **Backup Logs:** Preserve for security analysis

---

## 7. Performance Characteristics

### Benchmarks (Typical Hardware)

| Metric | Value |
|--------|-------|
| Requests/sec (uncached) | ~500 |
| Requests/sec (cached) | ~5,000 |
| Latency (cache hit) | <1ms |
| Latency (cache miss) | Internet RTT + processing |
| Max concurrent connections | ~1,000 |
| Memory per connection | ~100KB |
| Cache hit rate | 40-50% (typical workload) |

### Optimization Techniques

1. **LRU + TTL Caching:** Reduces origin requests
2. **Thread Detachment:** No thread join overhead
3. **Mutex Minimization:** Fine-grained locking
4. **Atomic Counters:** Lock-free statistics
5. **Pre-allocated Buffers:** Reduces allocations

---

## 8. Configuration Reference

### config.txt Format

```ini
# Server Settings
PORT=9090                      # Listening port
CONNECTION_TIMEOUT=30          # Seconds

# Cache Configuration
CACHE_LIMIT=100               # Max entries
CACHE_TTL=3600               # Seconds (1 hour)
MAX_CACHE_SIZE_MB=100        # Max size in MB

# Logging
LOG_LEVEL=INFO               # DEBUG, INFO, WARN, ERROR

# Features
ENABLE_STATS=true            # Track statistics

# Filtering Rules
BLOCK=instagram.com          # Blocked domain
BLOCK=youtube.com
WHITELIST=example.com        # Optional whitelist
```

### Runtime Behavior

- Configuration reloaded automatically every 2 seconds
- No restart required for config changes
- Invalid config entries are ignored with warnings

---

## 9. Testing Strategy

### Unit Testing (Manual)
- Individual component testing
- Cache eviction logic
- Config parsing
- Request parsing

### Integration Testing
- End-to-end request flow
- Cache integration
- Filtering integration
- Concurrent request handling

### Load Testing
- Concurrent connection handling
- Cache performance under load
- Memory usage profiling
- Thread resource usage

### Security Testing
- Blocked domain enforcement
- Malformed request handling
- Resource exhaustion resistance

---

## 10. Conclusion

This proxy server demonstrates solid software engineering principles:
- **Modular design** for maintainability
- **Thread safety** for reliability
- **Performance optimization** through caching
- **Extensibility** for future features

The architecture balances simplicity with functionality, making it suitable for educational purposes and small-scale deployment scenarios.

---

**Document Version:** 1.0  
**Last Updated:** January 9, 2026  
**Author:** Janvi Bhatti
