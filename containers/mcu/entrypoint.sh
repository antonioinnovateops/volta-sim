#!/bin/bash
set -e

# Clean exit: kill all children on signal
trap 'kill $(jobs -p) 2>/dev/null; wait' EXIT TERM INT

echo "=== Volta MCU Container ==="
echo "Renode version: $(renode --version 2>&1 | head -1 || echo 'unknown')"

# Pre-create SHM file with correct size and header
# This eliminates the race condition between Renode peripheral initialization and voltad
SHM_PATH=/dev/shm/volta_peripheral_state
SHM_SIZE=65536
echo "Pre-creating SHM file ($SHM_SIZE bytes) at $SHM_PATH..."
dd if=/dev/zero of=$SHM_PATH bs=$SHM_SIZE count=1 2>/dev/null
# Write VOLT magic (0x564F4C54) at offset 0 — little-endian: 54 4C 4F 56
printf '\x54\x4c\x4f\x56' | dd of=$SHM_PATH bs=1 seek=0 conv=notrunc 2>/dev/null
# Write version (1) at offset 4
printf '\x01\x00\x00\x00' | dd of=$SHM_PATH bs=1 seek=4 conv=notrunc 2>/dev/null
# Write initialized flag (1) at offset 16
printf '\x01\x00\x00\x00' | dd of=$SHM_PATH bs=1 seek=16 conv=notrunc 2>/dev/null
echo "SHM file created and initialized"

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
