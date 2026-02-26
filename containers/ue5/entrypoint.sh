#!/usr/bin/env bash
set -euo pipefail

# Volta-Sim UE5 Pixel Streaming entrypoint
# Launches Xvfb + signaling server + UE5 packaged game

DISPLAY_NUM="${DISPLAY_NUM:-99}"
SCREEN_RES="${SCREEN_RES:-1280x720x24}"
SIGNAL_HOST="${SIGNAL_HOST:-volta-ue5-signaling}"
SIGNAL_PORT="${SIGNAL_PORT:-8888}"
WEBRTC_PORT="${WEBRTC_PORT:-8889}"
SHM_PATH="${SHM_PATH:-/dev/shm/volta_peripheral_state}"

cleanup() {
    echo "[entrypoint] Shutting down..."
    kill "$SIGNAL_PID" "$UE5_PID" "$XVFB_PID" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# --- Wait for SHM ---
echo "[entrypoint] Waiting for SHM at ${SHM_PATH}..."
RETRIES=0
while [ ! -f "$SHM_PATH" ] && [ "$RETRIES" -lt 60 ]; do
    sleep 1
    RETRIES=$((RETRIES + 1))
done
[ -f "$SHM_PATH" ] && echo "[entrypoint] SHM ready" || echo "[entrypoint] WARNING: SHM not found, continuing"

# --- Start Xvfb ---
echo "[entrypoint] Starting Xvfb on :${DISPLAY_NUM}"
Xvfb ":${DISPLAY_NUM}" -screen 0 "${SCREEN_RES}" -ac +extension GLX &
XVFB_PID=$!
export DISPLAY=":${DISPLAY_NUM}"
sleep 1

# Signaling server runs as a separate container (volta-ue5-signaling)
echo "[entrypoint] Using external signaling server at ${SIGNAL_HOST}:${WEBRTC_PORT}"
SIGNAL_PID=0

# --- Find and start UE5 packaged game ---
echo "[entrypoint] Looking for VoltaSim binary..."
UE5_BIN=$(find /opt/volta/game -maxdepth 3 \( -name "VoltaSim" -o -name "VoltaSim-Linux-Shipping" \) -type f -executable 2>/dev/null | head -1)
[ -z "$UE5_BIN" ] && UE5_BIN=$(find /opt/volta/game -maxdepth 3 -name "VoltaSim.sh" -type f 2>/dev/null | head -1)

if [ -z "$UE5_BIN" ]; then
    echo "[entrypoint] ERROR: No packaged game in /opt/volta/game"
    echo ""
    echo "  To package the game:"
    echo "    1. Open engine/VoltaSim.uproject in UE5 Editor"
    echo "    2. Platforms > Linux > Package Project"
    echo "    3. Copy output to volta-sim/packaged-game/"
    echo "    4. Rebuild: docker-compose -f docker-compose.yml -f docker-compose.ue5.yml build"
    echo ""
    echo "  Keeping container alive for debugging..."
    tail -f /dev/null
fi

chmod +x "$UE5_BIN"
echo "[entrypoint] Starting: $UE5_BIN"

"$UE5_BIN" \
    -RenderOffScreen \
    -Windowed \
    -ResX=1280 -ResY=720 \
    -PixelStreamingIP="${SIGNAL_HOST}" \
    -PixelStreamingPort="${WEBRTC_PORT}" \
    -AllowPixelStreamingCommands \
    -nosplash -nosound \
    -log \
    &
UE5_PID=$!

echo "[entrypoint] Pixel Streaming at http://0.0.0.0:${SIGNAL_PORT}"
wait -n
echo "[entrypoint] Process exited, shutting down..."
