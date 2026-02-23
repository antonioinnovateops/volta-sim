#!/usr/bin/env bash
# Sprint 5 E2E test: PWM motor + ADC injection
# Requires: stack running (make up), pwm_motor firmware built
set -euo pipefail

API="http://localhost:8080/api/v1"
PASS=0
FAIL=0

pass() { echo "  PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "  FAIL: $1"; FAIL=$((FAIL + 1)); }

echo "=== Sprint 5 PWM/ADC E2E Test ==="

# 1. Build PWM firmware
echo ""
echo "--- Building PWM motor firmware ---"
docker exec volta-mcu bash -c "cp -r /opt/volta/firmware/examples/pwm_motor /tmp/pwm_motor 2>/dev/null; make -C /tmp/pwm_motor" 2>&1 | tail -1
pass "PWM firmware built"

# 2. Load PWM overlay (replaces timer1 with PWM bridge, adds ADC bridge)
echo ""
echo "--- Loading PWM/ADC overlay ---"
OVERLAY=$(curl -sf -X POST "$API/simulation/command" \
  -H "Content-Type: application/json" \
  -d '{"command": "include @/opt/volta/platforms/pwm_overlay.resc"}')

if echo "$OVERLAY" | grep -q '"success":true'; then
  pass "PWM/ADC overlay loaded"
else
  fail "PWM/ADC overlay failed: $OVERLAY"
fi

# 3. Flash PWM firmware
echo ""
echo "--- Flashing PWM motor firmware ---"
FLASH=$(curl -sf -X POST "$API/firmware/flash" \
  -F "elf_path=/tmp/pwm_motor/pwm_motor.elf" \
  -F "start=true")

if echo "$FLASH" | grep -q '"success":true'; then
  pass "Flash pwm_motor firmware"
else
  fail "Flash pwm_motor firmware: $FLASH"
fi

# Wait for firmware to start running and PWM to stabilize
sleep 3

# 2. Check PWM channel 0
echo ""
echo "--- Checking PWM state ---"
PWM=$(curl -sf "$API/pwm/0")

if echo "$PWM" | grep -q '"enabled":true'; then
  pass "PWM channel 0 enabled"
else
  fail "PWM channel 0 not enabled: $PWM"
fi

if echo "$PWM" | grep -q '"frequency"'; then
  FREQ=$(echo "$PWM" | python3 -c "import sys,json; print(json.load(sys.stdin).get('frequency',0))")
  if [ "$FREQ" -gt 0 ]; then
    pass "PWM frequency > 0 ($FREQ Hz)"
  else
    fail "PWM frequency is 0"
  fi
else
  fail "PWM response missing frequency: $PWM"
fi

# 3. Inject ADC value
echo ""
echo "--- Injecting ADC value ---"
INJECT=$(curl -sf -X POST "$API/adc/0/inject" \
  -H "Content-Type: application/json" \
  -d '{"voltage":1.65}')

if echo "$INJECT" | grep -q '"injected":true'; then
  pass "ADC injection accepted"
else
  fail "ADC injection failed: $INJECT"
fi

# 4. Read ADC value back
sleep 1
ADC=$(curl -sf "$API/adc/0")

if echo "$ADC" | grep -q '"raw_value"'; then
  RAW=$(echo "$ADC" | python3 -c "import sys,json; print(json.load(sys.stdin).get('raw_value',0))")
  if [ "$RAW" -gt 1900 ] && [ "$RAW" -lt 2200 ]; then
    pass "ADC raw value in expected range ($RAW ~ 2048)"
  else
    fail "ADC raw value out of range: $RAW (expected ~2048)"
  fi
else
  fail "ADC response missing raw_value: $ADC"
fi

VOLT=$(echo "$ADC" | python3 -c "import sys,json; print(json.load(sys.stdin).get('voltage',0))")
if python3 -c "import sys; sys.exit(0 if abs($VOLT - 1.65) < 0.01 else 1)"; then
  pass "ADC voltage matches injected ($VOLT V)"
else
  fail "ADC voltage mismatch: $VOLT (expected 1.65)"
fi

# 5. Backward compatibility: check GPIO still works
echo ""
echo "--- Backward compatibility ---"
GPIO=$(curl -sf "$API/gpio")

if echo "$GPIO" | grep -q '"ports"'; then
  pass "GPIO endpoint still responding"
else
  fail "GPIO endpoint broken: $GPIO"
fi

# Summary
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
