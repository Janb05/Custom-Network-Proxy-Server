# Testing Guide for Proxy Server

## 🚀 Quick Start

### 1. Start the Server
```bash
make run
# Or
./proxy_server
```

The server will start on port **9090** (configurable in `config.txt`)

---

## 🧪 Automated Testing

### Run the Test Suite
```bash
./test_proxy.sh
```

This will test:
- ✅ HTTP requests
- ✅ HTTPS requests
- ✅ Cache functionality
- ✅ Blocked domains
- ✅ Concurrent connections
- ✅ Error handling

---

## 🔧 Manual Testing

### Test 1: Basic HTTP Request
```bash
curl -x http://localhost:9090 http://example.com
```
**Expected:** HTML content from example.com  
**Log:** `[FETCHED]`

### Test 2: HTTPS Request
```bash
curl -x http://localhost:9090 https://example.com
```
**Expected:** HTML content  
**Log:** `[HTTPS_TUNNEL]`

### Test 3: Cache Test (Speed Comparison)
```bash
# First request (will be fetched)
time curl -x http://localhost:9090 http://example.com -o /dev/null -s

# Second request (will be cached - should be faster)
time curl -x http://localhost:9090 http://example.com -o /dev/null -s
```
**Expected:** Second request faster  
**Log:** First = `[FETCHED]`, Second = `[CACHED]`

### Test 4: Blocked Domain
```bash
curl -x http://localhost:9090 http://instagram.com
curl -x http://localhost:9090 http://youtube.com
```
**Expected:** `403 Forbidden`  
**Log:** `[BLOCKED_HTTP]`

### Test 5: Multiple Concurrent Requests
```bash
curl -x http://localhost:9090 http://google.com &
curl -x http://localhost:9090 http://github.com &
curl -x http://localhost:9090 https://httpbin.org &
wait
```
**Expected:** All requests complete successfully  
**Log:** Multiple entries in logs

### Test 6: View Response Headers
```bash
curl -I -x http://localhost:9090 http://example.com
```
**Expected:** HTTP headers displayed

### Test 7: Test with Browser

**Firefox:**
1. Settings → Network Settings → Manual proxy
2. HTTP Proxy: `localhost`, Port: `9090`
3. ✓ Also use this proxy for HTTPS
4. Browse to any website

**Chrome (Mac):**
```bash
# Launch Chrome with proxy
/Applications/Google\ Chrome.app/Contents/MacOS/Google\ Chrome \
  --proxy-server="localhost:9090" \
  --new-window
```

---

## 📊 Monitor Server Activity

### View Real-Time Logs
```bash
tail -f logs/proxy.log
```

### View All Logs
```bash
cat logs/proxy.log
```

### Filter Specific Log Types
```bash
# View only cached requests
grep "CACHED" logs/proxy.log

# View blocked requests
grep "BLOCKED" logs/proxy.log

# View errors
grep "ERROR" logs/proxy.log

# View HTTPS tunnels
grep "HTTPS_TUNNEL" logs/proxy.log
```

---

## 🔄 Test Configuration Hot-Reload

### Test 1: Change Blocked Domains
1. Edit `config.txt` - add a new blocked domain:
   ```
   BLOCK=twitter.com
   ```
2. Wait 2-3 seconds (config auto-reloads)
3. Test: `curl -x http://localhost:9090 http://twitter.com`
4. **Expected:** 403 Forbidden

### Test 2: Change Cache Settings
1. Edit `config.txt`:
   ```
   CACHE_LIMIT=50
   CACHE_TTL=1800
   ```
2. Wait 2-3 seconds
3. Watch console for: "🔁 Config reloaded"

### Test 3: Change Log Level
1. Edit `config.txt`:
   ```
   LOG_LEVEL=DEBUG
   ```
2. More detailed logs will appear

---

## 📈 Performance Testing

### Test Cache Performance
```bash
#!/bin/bash
echo "Testing cache performance..."

# First request (uncached)
echo "Request 1 (should be slow - fetching from internet):"
time curl -x http://localhost:9090 http://example.com -o /dev/null -s

# Subsequent requests (cached)
echo "Request 2 (should be fast - from cache):"
time curl -x http://localhost:9090 http://example.com -o /dev/null -s

echo "Request 3 (should be fast - from cache):"
time curl -x http://localhost:9090 http://example.com -o /dev/null -s
```

### Load Testing (Simple)
```bash
# Send 100 requests
for i in {1..100}; do
    curl -x http://localhost:9090 http://example.com -o /dev/null -s &
done
wait
echo "Completed 100 requests"
```

### Load Testing (Advanced - using Apache Bench)
```bash
# Install ab if needed: brew install httpd

# Test with 100 requests, 10 concurrent
ab -n 100 -c 10 -X localhost:9090 http://example.com/
```

---

## 🛠️ Debugging

### Check if Server is Running
```bash
ps aux | grep proxy_server
# Or
lsof -i :9090
```

### Kill Server
```bash
# Find process ID
ps aux | grep proxy_server

# Kill it
kill <PID>

# Or force kill
killall proxy_server
```

### View Statistics on Exit
When you stop the server (Ctrl+C), it displays:
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

---

## ✅ Test Checklist

Use this checklist to verify all features:

### Basic Functionality
- [ ] Server starts without errors
- [ ] HTTP requests work
- [ ] HTTPS requests work
- [ ] Server handles concurrent connections

### Caching
- [ ] First request is FETCHED
- [ ] Subsequent requests are CACHED
- [ ] Cached requests are faster
- [ ] Cache respects TTL
- [ ] Cache respects size limits

### Blocking
- [ ] Blocked domains return 403
- [ ] Blocked domains logged correctly
- [ ] Non-blocked domains work fine

### Configuration
- [ ] Config file loads at startup
- [ ] Config hot-reloads when modified
- [ ] All config options work
- [ ] Invalid config handled gracefully

### Logging
- [ ] Logs written to logs/proxy.log
- [ ] Log levels work (DEBUG, INFO, WARN, ERROR)
- [ ] Timestamps are correct
- [ ] All request types logged

### Error Handling
- [ ] Invalid hosts handled
- [ ] Connection timeouts work
- [ ] DNS failures handled
- [ ] Malformed requests handled

### Statistics
- [ ] Statistics displayed on exit
- [ ] Request counts accurate
- [ ] Cache hit rate calculated
- [ ] Bandwidth tracked

---

## 🎯 Demo Script for Presentations

```bash
#!/bin/bash
echo "=== PROXY SERVER DEMO ==="
echo ""

echo "1. Starting server..."
./proxy_server &
SERVER_PID=$!
sleep 2

echo ""
echo "2. Testing HTTP request..."
curl -x http://localhost:9090 http://example.com -o /dev/null -s
echo "   ✓ HTTP works"

echo ""
echo "3. Testing HTTPS request..."
curl -x http://localhost:9090 https://example.com -o /dev/null -s
echo "   ✓ HTTPS works"

echo ""
echo "4. Testing cache (2 requests)..."
curl -x http://localhost:9090 http://httpbin.org/html -o /dev/null -s
curl -x http://localhost:9090 http://httpbin.org/html -o /dev/null -s
echo "   ✓ Cache works"

echo ""
echo "5. Testing blocked domain..."
curl -x http://localhost:9090 http://instagram.com -o /dev/null -s
echo "   ✓ Blocking works"

echo ""
echo "6. Viewing logs..."
tail -10 logs/proxy.log

echo ""
echo "7. Stopping server..."
kill $SERVER_PID

echo ""
echo "=== DEMO COMPLETE ==="
```

---

## 📞 Troubleshooting Common Issues

### Issue: Port already in use
**Solution:**
```bash
lsof -i :9090
kill <PID>
```

### Issue: Permission denied
**Solution:**
```bash
chmod +x proxy_server
chmod +x test_proxy.sh
```

### Issue: Can't connect to internet through proxy
**Solution:**
- Check if server is running: `ps aux | grep proxy_server`
- Check logs for errors: `tail logs/proxy.log`
- Test without proxy first: `curl http://example.com`

### Issue: Cache not working
**Solution:**
- Check CACHE_TTL in config.txt
- Verify logs show CACHED entries
- Clear cache by restarting server

---

## 🎓 What to Show in Your Project Demo

1. **Start the server** - Show clean startup
2. **Run automated tests** - `./test_proxy.sh`
3. **Show logs** - `tail -f logs/proxy.log` in separate terminal
4. **Demonstrate caching** - Show speed difference
5. **Show blocking** - Try blocked site
6. **Hot-reload config** - Edit config while running
7. **Show statistics** - Stop server with Ctrl+C
8. **Show code structure** - Explain modular design

---

**Pro Tip:** Record a terminal session with `asciinema record demo.cast` to show your project working!
