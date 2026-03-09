#!/bin/bash
set -e

# Source cargo if available (use . for POSIX sh compatibility)
if [ -f "$HOME/.cargo/env" ]; then
    . "$HOME/.cargo/env"
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/logs"
mkdir -p "$LOG_DIR"

# ─── Parse flags ──────────────────────────────────────────────────────
DO_RUN=0
DO_CLEAN=0
DO_API=0
DO_ENGINE_ONLY=0
MUSIC_ON=false
LEVEL=""
EXECUTE=""
BOTS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --run)          DO_RUN=1; shift ;;
        --clean)        DO_CLEAN=1; shift ;;
        --api)          DO_API=1; shift ;;
        --engine-only)  DO_ENGINE_ONLY=1; shift ;;
        --music)
            case "${2:-}" in
                0|false) MUSIC_ON=false; shift 2 ;;
                1|true)  MUSIC_ON=true;  shift 2 ;;
                *)       MUSIC_ON=true;  shift ;;
            esac ;;
        --level)        LEVEL="$2"; shift 2 ;;
        --execute)      EXECUTE="$2"; shift 2 ;;
        --bots)         BOTS="$2"; shift 2 ;;
        *)              echo "Unknown flag: $1"; echo "Usage: build.sh [--clean] [--run] [--api] [--engine-only] [--music] [--level <map>] [--execute '<cmd>'] [--bots <n>]"; exit 1 ;;
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

# 1. Build qagame + ui dylibs from ioq3 source (skipped with --engine-only)
if [ "$DO_ENGINE_ONLY" -eq 0 ]; then
    IOQ3_DIR="$ROOT/ioq3"
    ARCH="$(uname -m)"; case "$ARCH" in arm64|aarch64) ARCH=arm64 ;; *) ARCH=x86_64 ;; esac

    # Clone ioq3 once
    if [ ! -d "$IOQ3_DIR/.git" ]; then
        echo "=== Cloning ioquake3 (first time) ==="
        git clone --depth=1 https://github.com/ioquake/ioq3.git "$IOQ3_DIR"
    fi

    # Ensure grapple patch
    G_CLIENT="$IOQ3_DIR/code/game/g_client.c"
    if ! grep -q "WP_GRAPPLING_HOOK.*STAT_WEAPONS\|STAT_WEAPONS.*WP_GRAPPLING_HOOK" "$G_CLIENT" 2>/dev/null; then
        echo "=== Applying grapple spawn patch ==="
        sed -i.bak '/ammo\[WP_GRAPPLING_HOOK\] = -1/i\\tclient->ps.stats[STAT_WEAPONS] |= ( 1 << WP_GRAPPLING_HOOK );' "$G_CLIENT"
    fi

    Q3IDE_BUILD="$IOQ3_DIR/build_q3ide"
    mkdir -p "$Q3IDE_BUILD"
    GCFLAGS="-O2 -arch ${ARCH}"

    # Build qagame
    echo "=== Building qagame ==="
    GAME_SRCS="g_main.c ai_chat.c ai_cmd.c ai_dmnet.c ai_dmq3.c ai_main.c ai_team.c ai_vcmd.c
        bg_misc.c bg_pmove.c bg_slidemove.c bg_lib.c
        g_active.c g_arenas.c g_bot.c g_client.c g_cmds.c g_combat.c
        g_items.c g_mem.c g_misc.c g_missile.c g_mover.c g_session.c
        g_spawn.c g_svcmds.c g_target.c g_team.c g_trigger.c g_utils.c g_weapon.c g_syscalls.c"
    GOBJS=""
    for f in $GAME_SRCS; do
        obj="$Q3IDE_BUILD/$(basename ${f%.c}.o)"
        clang $GCFLAGS -I"$IOQ3_DIR/code/game" -DQAGAME -DQ3_VM_LINKED -c "$IOQ3_DIR/code/game/$f" -o "$obj" 2>&1 || { echo "ERROR: qagame compile failed: $f"; exit 1; }
        GOBJS="$GOBJS $obj"
    done
    clang -dynamiclib -arch ${ARCH} -undefined dynamic_lookup $GOBJS -o "$Q3IDE_BUILD/qagame${ARCH}.dylib"
    cp "$Q3IDE_BUILD/qagame${ARCH}.dylib" "$ROOT/baseq3/qagame${ARCH}.dylib"
    cp "$Q3IDE_BUILD/qagame${ARCH}.dylib" "$ROOT/baseq3/qagame.dylib"

    # Build ui (ui dylib)
    echo "=== Building ui ==="
    UI_SRC="$IOQ3_DIR/code/q3_ui"
    UI_BUILD="$IOQ3_DIR/build_ui_q3ide"
    mkdir -p "$UI_BUILD"
    # GameSpy/ranking files (ui_login, ui_rankings, ui_rankstatus, ui_signup,
    # ui_specifyleague) omitted — require trap_CL_UI_Rank* stubs that don't
    # exist in STANDALONE builds. Those screens are unreachable anyway.
    UI_SRCS="ui_main.c ui_atoms.c ui_connect.c ui_controls2.c ui_demo2.c
        ui_cdkey.c ui_ingame.c ui_loadconfig.c ui_menu.c ui_mfield.c
        ui_mods.c ui_network.c ui_options.c ui_playermodel.c ui_players.c
        ui_qmenu.c ui_saveconfig.c ui_serverinfo.c ui_servers2.c
        ui_setup.c ui_sound.c ui_sparena.c ui_specifyserver.c
        ui_splevel.c ui_sppostgame.c ui_spreset.c ui_spskill.c ui_startserver.c
        ui_team.c ui_teamorders.c ui_video.c
        ui_addbots.c ui_removebots.c ui_cinematics.c
        ui_confirm.c ui_credits.c ui_display.c ui_gameinfo.c
        ui_playersettings.c ui_preferences.c"
    UIOBJS=""
    for f in $UI_SRCS; do
        obj="$UI_BUILD/$(basename ${f%.c}.o)"
        clang $GCFLAGS -I"$UI_SRC" -I"$IOQ3_DIR/code/game" -DUI -DQ3_VM_LINKED -c "$UI_SRC/$f" -o "$obj" 2>&1 || { echo "WARN: ui skip: $f"; continue; }
        UIOBJS="$UIOBJS $obj"
    done
    # bg_misc + bg_lib needed by ui
    for f in bg_misc.c bg_lib.c; do
        obj="$UI_BUILD/ui_$(basename ${f%.c}.o)"
        clang $GCFLAGS -I"$IOQ3_DIR/code/game" -DUI -DQ3_VM_LINKED -c "$IOQ3_DIR/code/game/$f" -o "$obj" 2>&1 || true
        UIOBJS="$UIOBJS $obj"
    done
    clang -dynamiclib -arch ${ARCH} -undefined dynamic_lookup $UIOBJS -o "$UI_BUILD/ui${ARCH}.dylib" 2>&1 && {
        cp "$UI_BUILD/ui${ARCH}.dylib" "$ROOT/baseq3/ui${ARCH}.dylib"
        cp "$UI_BUILD/ui${ARCH}.dylib" "$ROOT/baseq3/ui.dylib"
        echo "    ui.dylib built OK"
    } || echo "WARN: ui link failed — keeping existing ui.dylib"
fi

# 2. Build the capture dylib (skip with --engine-only)
if [ "$DO_ENGINE_ONLY" -eq 0 ]; then
    echo "=== Building capture dylib ==="
    cd "$ROOT/capture"
    cargo build --release
else
    echo "=== Skipping capture dylib (--engine-only) ==="
fi

# 3. Build the engine
echo "=== Building Quake3e engine ==="
cd "$ROOT/quake3e"
if [ "$DO_CLEAN" -eq 1 ]; then
    echo "=== Cleaning engine build ==="
    make clean ARCH="$Q3E_ARCH"
fi
make ARCH="$Q3E_ARCH" BUILD_SERVER=0

# 3. Find the build output directory
# Detect OS for build directory naming
OS_NAME="$(uname -s)"
case "$OS_NAME" in
    Darwin) BUILD_OS="darwin" ;;
    Linux)  BUILD_OS="linux"  ;;
    *)      echo "Unsupported OS: $OS_NAME"; exit 1 ;;
esac

BUILD_DIR="$ROOT/quake3e/build/release-${BUILD_OS}-${Q3E_ARCH}"
if [ ! -d "$BUILD_DIR" ]; then
    # Fallback for aarch64
    BUILD_DIR="$ROOT/quake3e/build/release-${BUILD_OS}-aarch64"
fi
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build output directory not found. Checked:"
    echo "  - $ROOT/quake3e/build/release-${BUILD_OS}-${Q3E_ARCH}"
    echo "  - $ROOT/quake3e/build/release-${BUILD_OS}-aarch64"
    echo "Check make output above."
    exit 1
fi

# 4. Copy dylib/so next to the engine binary (skip if --engine-only)
if [ "$DO_ENGINE_ONLY" -eq 0 ]; then
    echo "=== Copying capture library ==="
    if [ "$BUILD_OS" = "darwin" ]; then
        DYLIB_NAME="libq3ide_capture.dylib"
    else
        DYLIB_NAME="libq3ide_capture.so"
    fi
    DYLIB_SRC="$ROOT/capture/target/release/$DYLIB_NAME"
    if [ -f "$DYLIB_SRC" ]; then
        cp "$DYLIB_SRC" "$BUILD_DIR/"
        echo "Copied $DYLIB_NAME"
    else
        echo "WARNING: Capture library not found at $DYLIB_SRC"
    fi
fi

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
	echo "  Logs (tail via remote API: GET /logs?file=<alias>&n=400):"
	echo "    q3ide     → logs/q3ide.log           (q3ide structured, levelled)"
	echo "    engine    → logs/engine.log           (raw Quake3e + q3ide output)"
	echo "    multimon  → logs/q3ide_multimon.log   (multi-monitor renderer)"
	echo "    capture   → logs/q3ide_capture.log    (Rust capture dylib)"
	echo "    build     → logs/build.log            (last build output)"
	echo "    events    → logs/q3ide_events.jsonl   (structured JSON events)"
	echo ""
	echo "  Usage: build.sh [--run] [--api] [--level 0] [--bots 1] [--execute 'q3ide desktop']"
echo "========================================"

# 7. Ensure Screen Recording permission (macOS only)
if [ "$BUILD_OS" = "darwin" ] && [ -n "$ENGINE_BIN" ] && [ -f "$ENGINE_BIN" ]; then
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
    # Kill any running quake3e and wait for it to actually exit
    pkill -f "quake3e\." 2>/dev/null || true
    for _i in 1 2 3 4 5 6 7 8; do
        pgrep -f "quake3e\." >/dev/null 2>&1 || break
        sleep 0.5
    done

    cd "$BUILD_DIR"
    ENGINE_NAME="$(basename "$ENGINE_BIN")"

    # Build command args
    # vm_game 0 = native dylib; vm_cgame 2 = standard QVM (native cgame is incompatible with quake3e)
    ENGINE_ARGS="+set vm_game 0 +set vm_cgame 2 +set vm_ui 2"

    # --level: override the map
    if [ -n "$LEVEL" ]; then
        # Map shorthand: "1" -> "q3dm1", "q3dm7" stays as-is
        case "$LEVEL" in
            [0-9]|[0-9][0-9]) LEVEL="q3dm${LEVEL}" ;;
        esac
        ENGINE_ARGS="$ENGINE_ARGS +devmap $LEVEL"
    fi

    # --bots: set minimum player count so engine adds N bots
    if [ -n "$BOTS" ]; then
        BOT_MIN=$(( BOTS + 1 ))  # +1 for the human player
        ENGINE_ARGS="$ENGINE_ARGS +set bot_minplayers $BOT_MIN"
    fi

    # Music: random track on q3dm0 only, picked fresh each launch (requires --music)
    if [ "$MUSIC_ON" = "true" ] && [ "$LEVEL" = "q3dm0" ]; then
        q3_tracks=(
            "music/fla22k_01_intro.wav music/fla22k_01_loop.wav"
            "music/fla22k_02.wav"
            "music/fla22k_03.wav"
            "music/fla22k_04_intro.wav music/fla22k_04_loop.wav"
            "music/fla22k_05.wav"
            "music/fla22k_06.wav"
            "music/sonic1.wav"
            "music/sonic2.wav"
            "music/sonic3.wav"
            "music/sonic4.wav"
            "music/sonic5.wav"
            "music/sonic6.wav"
        )
        q3_track="${q3_tracks[$RANDOM % ${#q3_tracks[@]}]}"
        Q3_MUSIC_CMD="s_musicvolume 0.5; music $q3_track"
        if [ -n "$EXECUTE" ]; then
            EXECUTE="$EXECUTE; $Q3_MUSIC_CMD"
        else
            EXECUTE="$Q3_MUSIC_CMD"
        fi
        echo "[build] Music: $q3_track"
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

    # Auto-detect primary screen resolution (macOS only, Retina-safe logical coords)
    if command -v osascript > /dev/null 2>&1; then
        SCREEN_RES=$(osascript -e 'tell application "Finder" to get bounds of window of desktop' 2>/dev/null)
        if [ -n "$SCREEN_RES" ]; then
            SCREEN_W=$(echo "$SCREEN_RES" | awk -F'[, ]+' '{print $3}')
            SCREEN_H=$(echo "$SCREEN_RES" | awk -F'[, ]+' '{print $4}')
            # Sanity check — must be integers
            if echo "$SCREEN_W" | grep -qE '^[0-9]+$' && echo "$SCREEN_H" | grep -qE '^[0-9]+$'; then
                # Cap at 4K — some Macs report wild physical pixel counts
                [ "$SCREEN_W" -gt 3840 ] && SCREEN_W=3840
                [ "$SCREEN_H" -gt 2160 ] && SCREEN_H=2160
                ENGINE_ARGS="$ENGINE_ARGS +set r_mode -1 +set r_width $SCREEN_W +set r_height $SCREEN_H"
                echo "  Screen resolution: ${SCREEN_W}x${SCREEN_H}"
            else
                echo "  WARNING: Could not parse screen bounds ('$SCREEN_RES') — using engine default"
            fi
        fi
    fi

    ENGINE_LOG="$LOG_DIR/engine.log"
    echo ""
    echo "  Launching: ./$ENGINE_NAME $ENGINE_ARGS"
    echo "  Engine log: $ENGINE_LOG"
    echo ""
    "./$ENGINE_NAME" $ENGINE_ARGS 2>&1 | tee "$ENGINE_LOG"
fi
