#!/usr/bin/env bash
set -euo pipefail
#
# Package VoltaSim UE5 project for Linux Shipping.
# Runs inside the Epic Games UE5 Docker image.
#
# Usage (inside container):
#   /opt/volta/package.sh
#
# Environment:
#   UE_ROOT  - path to UE5 Installed Build (auto-detected)
#   PROJECT  - path to .uproject file (default: /opt/volta/engine/VoltaSim.uproject)
#   OUTPUT   - archive output directory (default: /opt/volta/packaged)

PROJECT="${PROJECT:-/opt/volta/engine/VoltaSim.uproject}"
OUTPUT="${OUTPUT:-/opt/volta/packaged}"

# Auto-detect UE root
if [ -z "${UE_ROOT:-}" ]; then
    for candidate in \
        /home/ue4/UnrealEngine \
        /opt/unreal-engine \
        /home/*/UnrealEngine; do
        if [ -f "$candidate/Engine/Build/BatchFiles/RunUAT.sh" ]; then
            UE_ROOT="$candidate"
            break
        fi
    done
fi

if [ -z "${UE_ROOT:-}" ]; then
    echo "ERROR: Cannot find UE5 installation. Set UE_ROOT."
    exit 1
fi

UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd"

echo "=== VoltaSim Linux Packaging ==="
echo "UE Root:  $UE_ROOT"
echo "Project:  $PROJECT"
echo "Output:   $OUTPUT"

# --- Step 1: Create minimal map if missing ---
MAP_DIR="$(dirname "$PROJECT")/Content/Maps"
MAP_FILE="$MAP_DIR/BoardMap.umap"

if [ ! -f "$MAP_FILE" ]; then
    echo ""
    echo "--- Creating blank BoardMap ---"
    mkdir -p "$MAP_DIR"

    # Use the Editor commandlet to create and save a blank level
    "$EDITOR" "$PROJECT" \
        -run=WorldPartitionBuilder \
        -ExecCmds="new map,saveCurrentLevel /Game/Maps/BoardMap" \
        -unattended -nopause -nosplash -nullrhi -log 2>&1 | tail -20 || true

    # Fallback: if the commandlet didn't create the map, use a Python commandlet
    if [ ! -f "$MAP_FILE" ]; then
        echo "Trying Python commandlet fallback..."
        cat > /tmp/create_map.py << 'PYEOF'
import unreal
world = unreal.EditorLevelLibrary.new_level("/Game/Maps/BoardMap")
unreal.EditorAssetLibrary.save_asset("/Game/Maps/BoardMap")
PYEOF
        "$EDITOR" "$PROJECT" \
            -run=pythonscript -script=/tmp/create_map.py \
            -unattended -nopause -nosplash -nullrhi -log 2>&1 | tail -20 || true
    fi

    # Last resort: copy a template map from the engine
    if [ ! -f "$MAP_FILE" ]; then
        echo "Trying engine template map fallback..."
        TEMPLATE=$(find "$UE_ROOT/Engine/Content" -name "*.umap" -type f 2>/dev/null | head -1)
        if [ -n "$TEMPLATE" ]; then
            cp "$TEMPLATE" "$MAP_FILE"
            echo "Copied template: $TEMPLATE → $MAP_FILE"
        else
            echo "WARNING: Could not create map. Packaging may fail."
        fi
    fi
fi

# --- Step 2: Package for Linux Shipping ---
echo ""
echo "--- Running BuildCookRun ---"
mkdir -p "$OUTPUT"

"$UAT" BuildCookRun \
    -project="$PROJECT" \
    -noP4 \
    -platform=Linux \
    -clientconfig=Shipping \
    -cook \
    -build \
    -stage \
    -pak \
    -archive \
    -archivedirectory="$OUTPUT" \
    -nocompileeditor \
    -skipbuildeditor \
    -UTF8Output \
    -NoDebugInfo \
    -unattended \
    -nosplash \
    2>&1

echo ""
echo "=== Packaging complete ==="
echo "Output: $OUTPUT"
ls -la "$OUTPUT/" 2>/dev/null || true
