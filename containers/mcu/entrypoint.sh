#!/bin/bash
set -e

# Clean exit: kill all children on signal
trap 'kill $(jobs -p) 2>/dev/null; wait' EXIT TERM INT

echo "=== Volta MCU Container ==="
echo "Renode version: $(renode --version 2>&1 | head -1 || echo 'unknown')"

# Start Renode in background
echo "Starting Renode with telnet monitor on port 1234..."
renode \
    --disable-xwt \
    --port 1234 \
    --execute "include @/opt/volta/platforms/stm32f4_discovery.resc" &
RENODE_PID=$!

# Wait for Renode telnet to be available
echo "Waiting for Renode telnet..."
for i in $(seq 1 30); do
    if echo | socat - TCP:localhost:1234,connect-timeout=1 >/dev/null 2>&1; then
        echo "Renode telnet is ready (attempt $i)"
        break
    fi
    sleep 1
done

# Start voltad
echo "Starting voltad..."
voltad \
    --shm-path /dev/shm/volta_peripheral_state \
    --zmq-pub-addr tcp://0.0.0.0:5555 \
    --grpc-addr 0.0.0.0:50051 \
    --renode-addr 127.0.0.1:1234 &
VOLTAD_PID=$!

echo "All services started (renode=$RENODE_PID, voltad=$VOLTAD_PID)"

# Wait for any child to exit
wait -n
echo "A child process exited, shutting down..."
