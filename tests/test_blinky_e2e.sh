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
sleep 3

# 2. Check GPIO shows PD12 activity
echo ""
echo "--- Checking GPIO state ---"
GPIO=$(curl -sf "$API/gpio")

if echo "$GPIO" | grep -q '"ports"'; then
  pass "GPIO endpoint responding"
else
  fail "GPIO endpoint broken: $GPIO"
fi

# Take multiple samples over 1.2s to detect PD12 toggling
# Blinky toggles at 500ms so we should see at least one HIGH
FOUND_HIGH=false
FOUND_LOW=false
for i in 1 2 3 4; do
  SAMPLE=$(curl -sf "$API/gpio/D")
  STATE=$(echo "$SAMPLE" | python3 -c "
import sys,json
data=json.load(sys.stdin)
pins=data.get('D',{}).get('pins',[])
p12=[p for p in pins if p['pin']==12]
print(p12[0]['output'] if p12 else 'MISSING')
" 2>/dev/null || echo "ERROR")
  if [ "$STATE" = "HIGH" ]; then FOUND_HIGH=true; fi
  if [ "$STATE" = "LOW" ] || [ "$STATE" = "MISSING" ]; then FOUND_LOW=true; fi
  sleep 0.3
done

if $FOUND_HIGH; then
  pass "PD12 detected HIGH"
else
  fail "PD12 never HIGH in 4 samples"
fi

if $FOUND_HIGH && $FOUND_LOW; then
  pass "PD12 toggling (seen HIGH and LOW/off)"
elif $FOUND_HIGH; then
  pass "PD12 active (always HIGH in samples)"
else
  fail "PD12 not active"
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
