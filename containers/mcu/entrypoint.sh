#!/bin/bash
set -e

echo "=== Volta MCU Container ==="
echo "Renode version: $(renode --version 2>&1 | head -1 || echo 'unknown')"
echo "Starting Renode with telnet monitor on port 1234..."

# Start Renode in headless mode with telnet monitor
exec renode \
    --disable-xwt \
    --port 1234 \
    --execute "include @/opt/volta/platforms/stm32f4_discovery.resc"
