#!/bin/bash
set -e

# Source cargo if available
if [ -f "$HOME/.cargo/env" ]; then
    source "$HOME/.cargo/env"
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/logs"
mkdir -p "$LOG_DIR"

# ─── Parse flags ──────────────────────────────────────────────────────
DO_RUN=0
DO_CLEAN=0
DO_API=0
DO_ENGINE_ONLY=0
LEVEL=""
EXECUTE=""
BOTS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --run)          DO_RUN=1; shift ;;
        --clean)        DO_CLEAN=1; shift ;;
        --api)          DO_API=1; shift ;;
        --engine-only)  DO_ENGINE_ONLY=1; shift ;;
        --level)        LEVEL="$2"; shift 2 ;;
        --execute)      EXECUTE="$2"; shift 2 ;;
        --bots)         BOTS="$2"; shift 2 ;;
        *)              echo "Unknown flag: $1"; echo "Usage: build.sh [--clean] [--run] [--api] [--engine-only] [--level <map>] [--execute '<cmd>'] [--bots <n>]"; exit 1 ;;
    esac
done

# Detect native architecture
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64)  Q3E_ARCH="x86_64" ;;
    arm64)   Q3E_ARCH="arm64"  ;;
    aarch64) Q3E_ARCH="arm64"  ;;
    *)       echo "Unsupported arch: $ARCH"; exit 1 ;;
esac

echo "=== Detected arch: $Q3E_ARCH ==="

# 1. Build the capture dylib (skip with --engine-only)
if [ "$DO_ENGINE_ONLY" -eq 0 ]; then
    echo "=== Building capture dylib ==="
    cd "$ROOT/capture"
    cargo build --release
else
    echo "=== Skipping capture dylib (--engine-only) ==="
fi

# 2. Build the engine
echo "=== Building Quake3e engine ==="
cd "$ROOT/quake3e"
if [ "$DO_CLEAN" -eq 1 ]; then
    echo "=== Cleaning engine build ==="
    make clean ARCH="$Q3E_ARCH"
fi
make ARCH="$Q3E_ARCH"

# 3. Find the build output directory
BUILD_DIR="$ROOT/quake3e/build/release-darwin-${Q3E_ARCH}"
if [ ! -d "$BUILD_DIR" ]; then
    BUILD_DIR="$ROOT/quake3e/build/release-darwin-aarch64"
fi
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build output directory not found. Check make output above."
    exit 1
fi

# 4. Copy dylib next to the engine binary
echo "=== Copying capture dylib ==="
cp "$ROOT/capture/target/release/libq3ide_capture.dylib" "$BUILD_DIR/"

# 5. Symlink baseq3 if not already there
if [ ! -e "$BUILD_DIR/baseq3" ]; then
    echo "=== Linking baseq3 ==="
    ln -s "$ROOT/baseq3" "$BUILD_DIR/baseq3"
fi

# 6. Find the engine binary (quake3e.<arch>, not .ded. or .dylib)
ENGINE_BIN=""
for f in "$BUILD_DIR"/quake3e.*; do
    case "$f" in
        *.ded.*|*.dylib) continue ;;
        *) ENGINE_BIN="$f"; break ;;
    esac
done

echo ""
echo "========================================"
echo "  BUILD COMPLETE"
echo "========================================"
echo ""
echo "  Build dir: $BUILD_DIR"
echo ""
if [ -n "$ENGINE_BIN" ] && [ -f "$ENGINE_BIN" ]; then
    echo "  Binary: $(basename "$ENGINE_BIN")"
else
    echo "  WARNING: Engine binary not found in $BUILD_DIR"
    ls -la "$BUILD_DIR"/ 2>/dev/null
fi
echo ""
echo "  In-game console (~):"
	echo "    q3ide desktop        - capture all monitors on nearest wall (default)"
	echo "    q3ide attach all     - attach iTerm/Terminal windows"
	echo "    q3ide list           - list capturable windows"
	echo "    q3ide detach         - detach all windows"
	echo "    q3ide status         - show active windows"
	echo ""
	echo "  Logs:"
	echo "    $BUILD_DIR/logs/q3ide.log"
	echo "    $BUILD_DIR/logs/q3ide_capture.log"
	echo ""
	echo "  Usage: build.sh [--run] [--api] [--level 0] [--bots 1] [--execute 'q3ide desktop']"
echo "========================================"

# 7. Ensure Screen Recording permission
if [ -n "$ENGINE_BIN" ] && [ -f "$ENGINE_BIN" ]; then
    PERMISSION_MARKER="$ROOT/.q3ide_screen_permission_granted"
    if [ ! -f "$PERMISSION_MARKER" ]; then
        echo ""
        echo "  Screen Recording permission is required for window capture."
        echo "  Binary: $ENGINE_BIN"
        echo ""
        echo "  Opening Settings and revealing the binary in Finder..."
        echo "  Drag the highlighted file into the Screen Recording list."
        echo ""
        open "x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture"
        sleep 1
        open -R "$ENGINE_BIN"
        echo ""
        read -p "  Press Enter once permission is granted..." _
        touch "$PERMISSION_MARKER"
    fi
fi

# 8. Launch API server in background (--api)
if [ "$DO_API" = "1" ]; then
    API_SCRIPT="$ROOT/scripts/remote_api.py"
    API_LOG="$LOG_DIR/remote_api.log"
    API_PID_FILE="/tmp/q3ide_api.pid"

    # Kill any existing instance
    if [ -f "$API_PID_FILE" ]; then
        OLD_PID="$(cat "$API_PID_FILE" 2>/dev/null)"
        if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
            echo "  Stopping existing API server (pid $OLD_PID)..."
            kill "$OLD_PID" 2>/dev/null
            sleep 0.5
        fi
        rm -f "$API_PID_FILE"
    fi

    echo "  Starting API server in background..."
    python3 "$API_SCRIPT" > "$API_LOG" 2>&1 &
    echo $! > "$API_PID_FILE"
    sleep 0.5
    if kill -0 "$(cat "$API_PID_FILE")" 2>/dev/null; then
        echo "  API server started (pid $(cat "$API_PID_FILE"))"
        echo "  Log: $API_LOG"
        echo "  ws://host.docker.internal:${Q3IDE_API_PORT:-6666}/ws"
    else
        echo "  WARNING: API server failed to start — check $API_LOG"
    fi
    echo ""
fi

# 9. Run
if [ -z "$ENGINE_BIN" ] || [ ! -f "$ENGINE_BIN" ]; then
    exit 0
fi

# If --run not passed, just exit (non-interactive when called from API)
if [ "$DO_RUN" = "0" ]; then
    exit 0
fi

if [ "$DO_RUN" = "1" ]; then
    cd "$BUILD_DIR"
    ENGINE_NAME="$(basename "$ENGINE_BIN")"

    # Build command args
    ENGINE_ARGS=""

    # --level: override the map
    if [ -n "$LEVEL" ]; then
        # Map shorthand: "1" -> "q3dm1", "q3dm7" stays as-is
        case "$LEVEL" in
            [0-9]|[0-9][0-9]) LEVEL="q3dm${LEVEL}" ;;
        esac
        ENGINE_ARGS="$ENGINE_ARGS +map $LEVEL"
    fi

    # --bots: set minimum player count so engine adds N bots
    if [ -n "$BOTS" ]; then
        BOT_MIN=$(( BOTS + 1 ))  # +1 for the human player
        ENGINE_ARGS="$ENGINE_ARGS +set bot_minplayers $BOT_MIN"
    fi

    # --execute: run a command after a delay (via a temp cfg)
    if [ -n "$EXECUTE" ]; then
        # Write a temp config that waits ~1 sec then executes
        # Quake3 doesn't have a delay command, so we use "wait" trick:
        # schedule the command via +set nextdemo which fires after map load
        ENGINE_ARGS="$ENGINE_ARGS +set nextdemo \"$EXECUTE\""

        # Alternative: write to a cfg and exec it
        AUTOCMD_CFG="$BUILD_DIR/baseq3/q3ide_autocmd.cfg"
        if [ -L "$BUILD_DIR/baseq3" ]; then
            AUTOCMD_CFG="$(readlink "$BUILD_DIR/baseq3")/q3ide_autocmd.cfg"
        fi
        echo "$EXECUTE" > "$AUTOCMD_CFG"
        # We'll trigger it from the engine side instead
    fi

    ENGINE_LOG="$LOG_DIR/engine.log"
    echo ""
    echo "  Launching: ./$ENGINE_NAME $ENGINE_ARGS"
    echo "  Engine log: $ENGINE_LOG"
    echo ""
    "./$ENGINE_NAME" $ENGINE_ARGS 2>&1 | tee "$ENGINE_LOG"
fi
