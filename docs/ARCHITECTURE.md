# Proxy Server Architecture

## Overview

This document describes the architecture and design decisions of the Advanced Proxy Server.

## System Architecture

```
┌─────────────┐
│   Client    │
└──────┬──────┘
       │ HTTP/HTTPS Request
       ▼
┌─────────────────────────────────────┐
│         Proxy Server                │
│  ┌─────────────────────────────┐   │
│  │   Request Handler           │   │
│  │  - Parse request            │   │
│  │  - Extract host/path        │   │
│  │  - Check blocklist          │   │
│  └───────┬─────────────────────┘   │
│          │                          │
│          ▼                          │
│  ┌─────────────┐   No Hit          │
│  │   Cache     ├─────────┐         │
│  │  Manager    │         │         │
│  └──────┬──────┘         │         │
│         │ Hit            │         │
│         ▼                ▼         │
│  ┌────────────┐   ┌──────────┐    │
│  │  Return    │   │  Fetch   │    │
│  │  Cached    │   │  from    │    │
│  │  Data      │   │  Internet│    │
│  └────────────┘   └─────┬────┘    │
│                          │         │
│                          ▼         │
│                   ┌─────────────┐  │
│                   │   Cache &   │  │
│                   │   Return    │  │
│                   └─────────────┘  │
│                                    │
│  ┌─────────────────────────────┐  │
│  │      Statistics             │  │
│  │   - Track all requests      │  │
│  │   - Record timing           │  │
│  │   - Calculate metrics       │  │
│  └─────────────────────────────┘  │
└────────────────────────────────────┘
```

## Component Details

### 1. ProxyServer
**Responsibility:** Main server lifecycle management

- Creates and binds socket
- Accepts incoming connections
- Spawns handler threads
- Manages graceful shutdown
- Coordinates all components

### 2. RequestHandler
**Responsibility:** Process individual client requests

**HTTP Handling:**
1. Parse HTTP request
2. Extract Host header
3. Check cache
4. If miss: fetch from origin
5. Cache response
6. Return to client

**HTTPS Handling:**
1. Parse CONNECT request
2. Establish tunnel to destination
3. Return "200 Connection Established"
4. Bidirectional data forwarding

### 3. CacheManager
**Responsibility:** Intelligent response caching

**Features:**
- LRU eviction policy
- TTL-based expiration
- Size-based limits
- Thread-safe operations
- Cache statistics

**Algorithm:**
```cpp
if (cache.contains(key) && !expired(entry)):
    move_to_front(lru)
    return cached_data
else:
    fetch_data()
    if (cache.full() || size_exceeded()):
        evict_oldest()
    cache.add(key, data)
    return data
```

### 4. ConfigManager
**Responsibility:** Configuration management

**Features:**
- File-based configuration
- Hot-reload detection
- Thread-safe access
- Callback notifications

**Watch Algorithm:**
```cpp
while (running):
    current_mtime = stat(config_file).mtime
    if (current_mtime != last_mtime):
        reload_config()
        notify_callbacks()
    sleep(2)
```

### 5. Logger
**Responsibility:** Multi-level logging

**Log Levels:**
- DEBUG: Detailed diagnostic info
- INFO: General information
- WARN: Warning messages
- ERROR: Error conditions

**Thread Safety:** Mutex-protected file writes

### 6. Statistics
**Responsibility:** Real-time metrics

**Tracked Metrics:**
- Request count (total, cached, blocked, errors)
- Bandwidth (sent/received)
- Per-host statistics
- Per-IP statistics
- Response times
- Cache hit rate

## Threading Model

### Main Thread
- Accepts connections
- Spawns handler threads

### Handler Threads
- One per client connection
- Short-lived (request duration)
- Detached (fire-and-forget)

### Background Threads
1. **Config Watcher** - Monitors config file
2. **Cache Cleaner** - Removes expired entries

### Thread Safety

**Shared Resources:**
- Cache (mutex-protected)
- Config (mutex-protected)
- Statistics (atomic counters + mutex)
- Log file (mutex-protected)

## Data Flow

### HTTP Request Flow
```
Client Request
    ↓
Extract Host
    ↓
Check Blocked? → Yes → Return 403
    ↓ No
Check Cache? → Yes → Return Cached
    ↓ No
Connect to Host
    ↓
Send Request
    ↓
Receive Response
    ↓
Cache Response
    ↓
Send to Client
    ↓
Log & Record Stats
```

### HTTPS CONNECT Flow
```
Client CONNECT
    ↓
Parse Host:Port
    ↓
Check Blocked? → Yes → Return 403
    ↓ No
Connect to Host
    ↓
Return 200 Connection Established
    ↓
Bidirectional Tunnel
    ↓
Log & Record Stats
```

## Design Patterns

### 1. Singleton (Statistics, Logger)
- Global access to shared resources
- Thread-safe initialization

### 2. Strategy (Cache Eviction)
- LRU eviction strategy
- Easily extensible to LFU, Random, etc.

### 3. Observer (Config Changes)
- Config manager notifies on changes
- Components react to updates

### 4. Factory (Future Extension)
- Can create different handler types
- Extensible for WebSocket, HTTP/2, etc.

## Performance Considerations

### Memory
- **Cache Size Limit:** Prevents unbounded growth
- **Thread Pool:** Could be added to limit threads
- **Buffer Reuse:** Fixed-size buffers per request

### Network
- **Keep-Alive:** Not implemented (connection: close)
- **Pipelining:** Not supported
- **Compression:** Not implemented

### Concurrency
- **Lock Granularity:** Fine-grained locks
- **Atomic Counters:** For simple statistics
- **Thread-Local Storage:** Could reduce contention

## Scalability

### Current Limits
- Thread-per-connection model
- Single-process architecture
- In-memory cache only

### Potential Improvements
1. **Thread Pool** - Limit max threads
2. **Event-Driven** - epoll/kqueue for I/O
3. **Distributed Cache** - Redis/Memcached
4. **Multi-Process** - Fork workers
5. **Load Balancing** - Multiple instances

## Security

### Current Security Features
- Domain blocking
- Connection timeouts
- Request validation

### Missing Security Features
- No authentication
- No HTTPS for proxy connection
- No rate limiting
- No input sanitization
- No MITM protection

## Error Handling

### Network Errors
- Connection timeouts
- DNS resolution failures
- Socket errors
- Disconnections

### Application Errors
- Malformed requests
- Missing headers
- Invalid configuration
- Resource exhaustion

**Strategy:** Defensive programming with early returns and resource cleanup.

## Testing Strategy

### Unit Tests (Not Implemented)
- Cache eviction logic
- Config parsing
- Request parsing

### Integration Tests (Manual)
- HTTP requests via curl
- HTTPS requests via curl
- Blocking verification
- Cache verification

### Load Tests (Future)
- Apache Bench (ab)
- wrk
- Custom scripts

## Future Enhancements

1. **HTTP/2 Support**
2. **WebSocket Proxying**
3. **SSL/TLS Interception**
4. **Content Filtering**
5. **Bandwidth Throttling**
6. **Parent Proxy Support**
7. **Authentication**
8. **Web Dashboard**
9. **REST API**
10. **Persistent Cache**

---

*Last Updated: January 3, 2026*
