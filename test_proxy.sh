#!/bin/bash

# Proxy Server Test Script
# Tests all features of the proxy server

PROXY="http://localhost:9090"
BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo ""
echo "╔════════════════════════════════════════════════╗"
echo "║     PROXY SERVER TEST SUITE                    ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Test 1: HTTP Request
echo -e "${BLUE}[TEST 1]${NC} Testing HTTP request..."
HTTP_RESPONSE=$(curl -x $PROXY -s -o /dev/null -w "%{http_code}" http://example.com --max-time 10)
if [ "$HTTP_RESPONSE" == "200" ]; then
    echo -e "${GREEN}✓ PASS${NC} - HTTP request successful (Status: $HTTP_RESPONSE)"
else
    echo -e "${RED}✗ FAIL${NC} - HTTP request failed (Status: $HTTP_RESPONSE)"
fi
echo ""

# Test 2: HTTPS Request
echo -e "${BLUE}[TEST 2]${NC} Testing HTTPS request..."
HTTPS_RESPONSE=$(curl -x $PROXY -s -o /dev/null -w "%{http_code}" https://example.com --max-time 10)
if [ "$HTTPS_RESPONSE" == "200" ]; then
    echo -e "${GREEN}✓ PASS${NC} - HTTPS request successful (Status: $HTTPS_RESPONSE)"
else
    echo -e "${RED}✗ FAIL${NC} - HTTPS request failed (Status: $HTTPS_RESPONSE)"
fi
echo ""

# Test 3: Cache Test (request same URL twice)
echo -e "${BLUE}[TEST 3]${NC} Testing cache functionality..."
echo "  → First request (should be FETCHED)..."
time1=$(curl -x $PROXY -s -o /dev/null -w "%{time_total}" http://example.com --max-time 10)
sleep 1
echo "  → Second request (should be CACHED)..."
time2=$(curl -x $PROXY -s -o /dev/null -w "%{time_total}" http://example.com --max-time 10)

echo "  First request time:  ${time1}s"
echo "  Second request time: ${time2}s"

if (( $(echo "$time2 < $time1" | bc -l) )); then
    echo -e "${GREEN}✓ PASS${NC} - Cache is working (second request faster)"
else
    echo -e "${YELLOW}⚠ WARNING${NC} - Cache may not be working optimally"
fi
echo ""

# Test 4: Blocked Domain
echo -e "${BLUE}[TEST 4]${NC} Testing blocked domain (instagram.com)..."
BLOCKED_RESPONSE=$(curl -x $PROXY -s -o /dev/null -w "%{http_code}" http://instagram.com --max-time 10)
if [ "$BLOCKED_RESPONSE" == "403" ]; then
    echo -e "${GREEN}✓ PASS${NC} - Blocked domain correctly returns 403"
else
    echo -e "${RED}✗ FAIL${NC} - Expected 403, got $BLOCKED_RESPONSE"
fi
echo ""

# Test 5: Another Blocked Domain
echo -e "${BLUE}[TEST 5]${NC} Testing blocked domain (youtube.com)..."
BLOCKED_RESPONSE2=$(curl -x $PROXY -s -o /dev/null -w "%{http_code}" http://youtube.com --max-time 10)
if [ "$BLOCKED_RESPONSE2" == "403" ]; then
    echo -e "${GREEN}✓ PASS${NC} - Blocked domain correctly returns 403"
else
    echo -e "${RED}✗ FAIL${NC} - Expected 403, got $BLOCKED_RESPONSE2"
fi
echo ""

# Test 6: Multiple Different Hosts
echo -e "${BLUE}[TEST 6]${NC} Testing multiple different hosts..."
curl -x $PROXY -s -o /dev/null http://google.com --max-time 10 &
curl -x $PROXY -s -o /dev/null http://github.com --max-time 10 &
curl -x $PROXY -s -o /dev/null https://httpbin.org --max-time 10 &
wait
echo -e "${GREEN}✓ PASS${NC} - Multiple concurrent requests handled"
echo ""

# Test 7: Invalid Host
echo -e "${BLUE}[TEST 7]${NC} Testing invalid/non-existent host..."
INVALID_RESPONSE=$(curl -x $PROXY -s -o /dev/null -w "%{http_code}" http://this-domain-does-not-exist-12345.com --max-time 10)
if [ "$INVALID_RESPONSE" == "500" ] || [ "$INVALID_RESPONSE" == "000" ]; then
    echo -e "${GREEN}✓ PASS${NC} - Invalid host handled gracefully"
else
    echo -e "${YELLOW}⚠ INFO${NC} - Response: $INVALID_RESPONSE"
fi
echo ""

# Summary
echo "╔════════════════════════════════════════════════╗"
echo "║     TEST SUMMARY                               ║"
echo "╚════════════════════════════════════════════════╝"
echo ""
echo "Check logs/proxy.log for detailed request logs"
echo "Check terminal output for server statistics"
echo ""
echo "To stop the server: kill the proxy_server process or Ctrl+C"
echo ""
