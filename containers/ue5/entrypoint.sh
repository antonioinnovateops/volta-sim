#!/usr/bin/env bash
set -euo pipefail

# Volta-Sim UE5 Pixel Streaming entrypoint
# Launches Xvfb (virtual display) + signaling server + UE5 packaged game

DISPLAY_NUM="${DISPLAY_NUM:-99}"
SCREEN_RES="${SCREEN_RES:-1920x1080x24}"
SIGNAL_PORT="${SIGNAL_PORT:-8888}"
WEBRTC_PORT="${WEBRTC_PORT:-8889}"
SHM_PATH="${SHM_PATH:-/dev/shm/volta_peripheral_state}"

cleanup() {
    echo "[entrypoint] Shutting down..."
    kill "$SIGNAL_PID" "$UE5_PID" "$XVFB_PID" 2>/dev/null || true
    wait 2>/dev/null || true
    echo "[entrypoint] Done."
}
trap cleanup EXIT INT TERM

# --- Wait for SHM to be initialized by Renode ---
echo "[entrypoint] Waiting for SHM at ${SHM_PATH}..."
RETRIES=0
MAX_RETRIES=120
while [ ! -f "$SHM_PATH" ] && [ "$RETRIES" -lt "$MAX_RETRIES" ]; do
    sleep 1
    RETRIES=$((RETRIES + 1))
done

if [ ! -f "$SHM_PATH" ]; then
    echo "[entrypoint] WARNING: SHM not found after ${MAX_RETRIES}s, continuing anyway (BoardActor has retry logic)"
fi

# --- Start Xvfb (virtual framebuffer) ---
echo "[entrypoint] Starting Xvfb on :${DISPLAY_NUM} (${SCREEN_RES})"
Xvfb ":${DISPLAY_NUM}" -screen 0 "${SCREEN_RES}" -ac +extension GLX &
XVFB_PID=$!
export DISPLAY=":${DISPLAY_NUM}"
sleep 2

# --- Start Pixel Streaming signaling server ---
echo "[entrypoint] Starting signaling server on port ${SIGNAL_PORT}"
cd /opt/volta/signaling
node cirrus.js \
    --HttpPort="${SIGNAL_PORT}" \
    --StreamerPort="${WEBRTC_PORT}" \
    &
SIGNAL_PID=$!
sleep 2

# --- Start UE5 packaged game ---
echo "[entrypoint] Starting UE5 VoltaSim"

# Find the packaged binary
UE5_BIN=$(find /opt/volta/game -name "VoltaSim" -o -name "VoltaSim-Linux-Shipping" | head -1)

if [ -z "$UE5_BIN" ]; then
    # Fallback: look for the shell script launcher
    UE5_BIN=$(find /opt/volta/game -name "VoltaSim.sh" | head -1)
fi

if [ -z "$UE5_BIN" ]; then
    echo "[entrypoint] ERROR: Could not find VoltaSim binary in /opt/volta/game"
    ls -la /opt/volta/game/ || true
    exit 1
fi

chmod +x "$UE5_BIN"

"$UE5_BIN" \
    -RenderOffScreen \
    -Windowed \
    -ResX=1920 -ResY=1080 \
    -PixelStreamingIP=127.0.0.1 \
    -PixelStreamingPort="${WEBRTC_PORT}" \
    -AllowPixelStreamingCommands \
    -log \
    &
UE5_PID=$!

echo "[entrypoint] All services started (Xvfb=$XVFB_PID, Signaling=$SIGNAL_PID, UE5=$UE5_PID)"
echo "[entrypoint] Pixel Streaming available at http://0.0.0.0:${SIGNAL_PORT}"

# Wait for any child to exit
wait -n
echo "[entrypoint] A child process exited, shutting down..."
