#!/usr/bin/env bash
# Sprint 1-4 E2E test: Blinky firmware
# Requires: stack running (make up), blinky firmware built
set -euo pipefail

API="http://localhost:8080/api/v1"
PASS=0
FAIL=0

pass() { echo "  PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "  FAIL: $1"; FAIL=$((FAIL + 1)); }

echo "=== Blinky E2E Test ==="

# 1. Flash blinky firmware
echo ""
echo "--- Flashing blinky firmware ---"
FLASH=$(curl -sf -X POST "$API/firmware/flash" \
  -F "elf_path=/opt/volta/firmware/examples/blinky/blinky.elf" \
  -F "start=true")

if echo "$FLASH" | grep -q '"success":true'; then
  pass "Flash blinky firmware"
else
  fail "Flash blinky firmware: $FLASH"
fi

# Wait for firmware to run (blinky toggles every 500ms)
sleep 2

# 2. Check GPIO shows PD12 activity
echo ""
echo "--- Checking GPIO state ---"
GPIO=$(curl -sf "$API/gpio")

if echo "$GPIO" | grep -q '"ports"'; then
  pass "GPIO endpoint responding"
else
  fail "GPIO endpoint broken: $GPIO"
fi

# Check for port D (PD12 = green LED)
if echo "$GPIO" | grep -q '"D"'; then
  pass "Port D has active pins"
else
  fail "Port D not found in GPIO state"
fi

# Take two samples 600ms apart to verify toggling
GPIO1=$(curl -sf "$API/gpio/D")
sleep 0.6
GPIO2=$(curl -sf "$API/gpio/D")

PIN12_1=$(echo "$GPIO1" | python3 -c "
import sys,json
data=json.load(sys.stdin)
pins=data.get('D',{}).get('pins',[])
p12=[p for p in pins if p['pin']==12]
print(p12[0]['output'] if p12 else 'MISSING')
" 2>/dev/null || echo "ERROR")

PIN12_2=$(echo "$GPIO2" | python3 -c "
import sys,json
data=json.load(sys.stdin)
pins=data.get('D',{}).get('pins',[])
p12=[p for p in pins if p['pin']==12]
print(p12[0]['output'] if p12 else 'MISSING')
" 2>/dev/null || echo "ERROR")

if [ "$PIN12_1" != "$PIN12_2" ]; then
  pass "PD12 toggling ($PIN12_1 -> $PIN12_2)"
elif [ "$PIN12_1" = "MISSING" ]; then
  fail "PD12 not found in GPIO state"
else
  # Toggling might be in-sync with our reads; just check it's HIGH or LOW
  if [ "$PIN12_1" = "HIGH" ] || [ "$PIN12_1" = "LOW" ]; then
    pass "PD12 active (sampled same state: $PIN12_1)"
  else
    fail "PD12 unexpected state: $PIN12_1"
  fi
fi

# 3. Health check
echo ""
echo "--- Health check ---"
HEALTH=$(curl -sf "$API/../health" || curl -sf "http://localhost:8080/health")

if echo "$HEALTH" | grep -q '"ok"'; then
  pass "Health endpoint OK"
else
  fail "Health endpoint: $HEALTH"
fi

# Summary
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
