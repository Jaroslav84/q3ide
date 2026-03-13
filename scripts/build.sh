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
DO_FAST=0
DO_API=0
DO_ENGINE_ONLY=0
MUSIC_ON=false
LEVEL=""
EXECUTE=""
BOTS=""
RELEASE_BUILD="nightbuild"
QUEUE_ID=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --run)          DO_RUN=1; shift ;;
        --clean)        DO_CLEAN=1; shift ;;
        --fast)         DO_FAST=1; DO_ENGINE_ONLY=1; shift ;;
        --api)          DO_API=1; shift ;;
        --engine-only)  DO_ENGINE_ONLY=1; shift ;;
        --queue-id)     QUEUE_ID="$2"; shift 2 ;;
        --music)
            case "${2:-}" in
                0|false) MUSIC_ON=false; shift 2 ;;
                1|true)  MUSIC_ON=true;  shift 2 ;;
                *)       MUSIC_ON=true;  shift ;;
            esac ;;
        --level)
            case "${2:-}" in
                r|random)
                    _maps=(
                        # Lunaran
                        lun3dm1 lun3dm2 lun3dm3-cpm lun3dm4 lun3dm5 lun3_20b1
                        # Acidwire
                        acid3dm2 acid3dm3 acid3dm3r acid3tourney3
                        acid3dm4 acid3dm5 acid3dm6 acid3dm7
                        acid3dm8 acid3dm9 acid3dm10 acid3dm11 acid3dm12
                        # CPMA
                        cpm1a cpm2 cpm3 cpm3a cpm4 cpm4a cpm5 cpm6 cpm7 cpm8 cpm9 cpm10
                        cpm11 cpm11a cpm12 cpm13 cpm14 cpm15 cpm16 cpm17
                        cpm18 cpm18r cpm19 cpm20 cpm21 cpm22 cpm23 cpm24
                        cpm25 cpm26 cpm27 cpm28 cpm29
                        cpmctf1 cpmctf2 cpmctf3 cpmctf4 cpmctf5 cpma3
                        # CTF
                        q3ctfchnu01 QuadCTF jlctf1 jlctf2 jlctf3 q3tourney6_ctf
                        # DC Mappack
                        dc_map02 dc_map03 dc_map04 dc_map05 dc_map06 dc_map07
                        dc_map08 dc_map09 dc_map10 dc_map11 dc_map12 dc_map13
                        dc_map14 dc_map15 dc_map16 dc_map17 dc_map18 dc_map19
                        dc_map20 dc_map21 dc_map22 dc_map23 dc_map24
                        # ZTN
                        ztn3dm1 ztn3dm1-ho ztn3tourney1
                        # Classic Pro DM
                        hub3aeroq3 aggressor bloodcovenant focal_p132 overkill
                        pro-nodm9 pro-q3dm6 pro-q3dm13 pro-q3tourney2 pro-q3tourney4
                        pro-q3tourney7 pukka3tourney2 tig_den
                        # Egyptian
                        egyptsm1 gpl-gypt
                        # OSP
                        ospca1 ospctf1 ospctf2
                        ospdm1 ospdm2 ospdm3 ospdm4 ospdm5 ospdm6
                        ospdm7 ospdm8 ospdm9 ospdm10 ospdm11 ospdm12
                        # Threewave CTF
                        q3wcp1 q3wcp2 q3wcp3 q3wcp4 q3wcp5 q3wcp6 q3wcp7 q3wcp8
                        q3wxs1 q3wcp9 q3wcp10 q3wcp11 q3wcp12 q3wcp13 q3wcp14 q3wcp15
                        q3wcp16 q3wxs2
                        # WTF Pack
                        wtf01 wtf02 wtf03 wtf04 wtf05 wtf06 wtf07 wtf08 wtf09 wtf10
                        wtf11 wtf12 wtf13 wtf14 wtf15 wtf16 wtf17 wtf18 wtf19 wtf20
                        wtf21 wtf22 wtf23 wtf24 wtf25 wtf26 wtf27 wtf28 wtf29 wtf30
                        wtf31 wtf32 wtf33 wtf34 wtf35 wtf36 wtf37 wtf38 wtf39 wtf40
                        wtf41 wtf42 wtf43 wtf44 wtf45 wtf46 wtf47 wtf48
                        wtf01-pro wtf02-pro wtf04-pro wtf07-pro wtf08-pro
                        wtf25-pro wtf32-day wtf33-pro wtf37-pro wtf48-pro
                        wtf35-bfg wtf35-gauntlet wtf35-grayscale wtf35-grenades
                        wtf35-lightning wtf35-machine wtf35-plasmagun
                        wtf35-rocket wtf35-shotgun wtf35-ultra
                        # sst13 fun
                        13matrix 13star 13cube 13island 13stone 13dyna
                        # Other
                        ori_apt quatrix r7-blockworld1 m3amap1
                    )
                    LEVEL="${_maps[$RANDOM % ${#_maps[@]}]}"
                    echo "[build] Random map: $LEVEL"
                    shift 2 ;;
                *)
                    LEVEL="$2"; shift 2 ;;
            esac ;;
        --execute)      EXECUTE="$2"; shift 2 ;;
        --bots)         BOTS="$2"; shift 2 ;;
        --release)
            if [ -z "${2:-}" ]; then
                echo "--release requires a value: orig | stable | nightbuild | /path/to/quake3e-dir"
                exit 1
            fi
            RELEASE_BUILD="$2"; shift 2 ;;
        *)              echo "Unknown flag: $1"; echo "Usage: build.sh [--clean] [--fast] [--run] [--api] [--engine-only] [--music] [--level <map|r>] [--execute '<cmd>'] [--bots <n>] [--release orig|stable|nightbuild|/path]"; exit 1 ;;
    esac
done

# Header helper: every === banner includes timestamp + queue id so agents know what's building
build_hdr() {
    local ts; ts="$(date '+%H:%M:%S')"
    if [ -n "$QUEUE_ID" ]; then
        echo "=== [$ts] [q:$QUEUE_ID] $1 ==="
    else
        echo "=== [$ts] $1 ==="
    fi
}

# Resolve engine source directory from --release value (alias or path)
Q3IDE_VERSION="$(grep -o 'Version-v[^-]*' "$ROOT/README.md" 2>/dev/null | head -1 | sed 's/Version-//')"
[ -z "$Q3IDE_VERSION" ] && Q3IDE_VERSION="unknown"

case "$RELEASE_BUILD" in
    orig)       Q3E_DIR="$ROOT/quake3e-orig";   RELEASE_LABEL="orig (quake3e-orig)" ;;
    stable)     Q3E_DIR="$ROOT/quake3e-stable"; RELEASE_LABEL="stable (quake3e-stable)" ;;
    nightbuild) Q3E_DIR="$ROOT/quake3e";        RELEASE_LABEL="nightbuild (quake3e)" ;;
    *)          Q3E_DIR="$RELEASE_BUILD";        RELEASE_LABEL="custom ($Q3E_DIR)" ;;
esac
if [ ! -d "$Q3E_DIR" ]; then
    echo "ERROR: Engine source directory not found: $Q3E_DIR"
    exit 1
fi

# Detect native architecture
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64)  Q3E_ARCH="x86_64" ;;
    arm64)   Q3E_ARCH="arm64"  ;;
    aarch64) Q3E_ARCH="arm64"  ;;
    *)       echo "Unsupported arch: $ARCH"; exit 1 ;;
esac

# System info
OS_TYPE="$(uname -s)"
case "$OS_TYPE" in
    Darwin)  SYS_TYPE="macOS" ;;
    Linux)   SYS_TYPE="linux" ;;
    MINGW*|MSYS*|CYGWIN*) SYS_TYPE="windows" ;;
    *)       SYS_TYPE="$OS_TYPE" ;;
esac
if [ "$OS_TYPE" = "Darwin" ]; then
    SP_DISP="$(system_profiler SPDisplaysDataType 2>/dev/null)"

    CPU=$(sysctl -n machdep.cpu.brand_string 2>/dev/null \
        | sed 's/Intel(R) Core(TM) //;s/Apple //;s/ CPU//;s/(R)//g;s/(TM)//g;s/  */ /g;s/^ //;s/ $//')
    [ -z "$CPU" ] && CPU="unknown"
    RAM=$(sysctl -n hw.memsize 2>/dev/null | awk '{printf "%.0fG", $1/1073741824}')
    [ -z "$RAM" ] && RAM="?"

    GPU=$(echo "$SP_DISP" | awk -F': ' '/Chipset Model:/{print $2; exit}')
    [ -z "$GPU" ] && GPU="unknown"
    GPU_VRAM=$(echo "$SP_DISP" | awk -F': ' '/VRAM/{print $2; exit}' | tr -d ' ')
    [ -z "$GPU_VRAM" ] && GPU_VRAM="?"

    METAL_N=$(echo "$SP_DISP" | grep -ic "Metal: Supported" || true)
    [ "${METAL_N:-0}" -gt 0 ] && METAL_STR="YES" || METAL_STR="NO"
    OPENCL_N=$(echo "$SP_DISP" | grep -ic "OpenCL" || true)
    [ "${OPENCL_N:-0}" -gt 0 ] && OPENCL_STR="YES" || OPENCL_STR="NO"
    HIDPI_N=$(echo "$SP_DISP" | grep -ic "HiDPI: Yes\|Retina: Yes" || true)
    [ "${HIDPI_N:-0}" -gt 0 ] && HIDPI_STR="YES" || HIDPI_STR="NO"
    OPENGL_STR="YES"  # always present on macOS (deprecated but present)

    # Monitor info: one line per display "[n] WxH @ HzHz  Retina|Non-Retina"
    MONITOR_LAYOUT=$(python3 - <<'PYEOF' 2>/dev/null
import subprocess, re
out = subprocess.check_output(['system_profiler', 'SPDisplaysDataType'], text=True)
lines = out.splitlines()
displays = []
i = 0
while i < len(lines):
    m = re.match(r'\s+Resolution:\s+(\d+)\s*[xX\u00d7]\s*(\d+)(?:\s*@\s*([\d.]+)\s*Hz)?', lines[i])
    if m:
        w, h, hz = m.group(1), m.group(2), m.group(3)
        retina = None
        conn = None
        for j in range(i + 1, min(i + 30, len(lines))):
            if re.match(r'\s+Resolution:', lines[j]):
                break
            mf = re.match(r'\s+(?:Framerate|Frame Rate|Refresh Rate):\s+([\d.]+)\s*Hz', lines[j])
            if mf and not hz:
                hz = mf.group(1)
            mf2 = re.match(r'\s+UI Looks Like:\s+\d+\s*[xX\u00d7]\s*\d+\s*@\s*([\d.]+)', lines[j])
            if mf2 and not hz:
                hz = mf2.group(1)
            # broad fallback: any "NNN Hz" near this display entry
            mf3 = re.search(r'([\d.]+)\s*Hz', lines[j])
            if mf3 and not hz:
                hz = mf3.group(1)
            if re.search(r'(?:HiDPI|Retina)\s*:\s*Yes', lines[j], re.I):
                retina = True
            elif re.search(r'(?:HiDPI|Retina)\s*:\s*No', lines[j], re.I):
                retina = False
            mc = re.match(r'\s+Connection Type:\s+(.+)', lines[j])
            if mc and not conn:
                conn = mc.group(1).strip()
        displays.append((w, h, hz, retina, conn))
    i += 1
if not displays:
    print('unknown')
else:
    for idx, (w, h, hz, retina, conn) in enumerate(displays):
        s = f'[{idx}]  {w}x{h}'
        if hz:
            s += f'  @{int(float(hz))}Hz'
        s += '  ' + ('Retina' if retina is True else 'Non-Retina')
        if conn:
            conn = conn.replace('Thunderbolt/DisplayPort', 'DP').replace('DisplayPort', 'DP').replace('Thunderbolt', 'TB')
            s += f'  {conn}'
        print(s)
PYEOF
    )
    [ -z "$MONITOR_LAYOUT" ] && MONITOR_LAYOUT="unknown"
else
    CPU=$(awk -F': ' '/model name/{print $2; exit}' /proc/cpuinfo 2>/dev/null | sed 's/  */ /g' || true)
    [ -z "$CPU" ] && CPU="unknown"
    RAM=$(awk '/MemTotal/{printf "%.0fG", $2/1048576}' /proc/meminfo 2>/dev/null || true)
    [ -z "$RAM" ] && RAM="?"
    GPU=$(lspci 2>/dev/null | grep -i "vga\|3d\|display" | head -1 | sed 's/.*: //' || true)
    [ -z "$GPU" ] && GPU="unknown"
    GPU_VRAM="?"
    METAL_STR="NO"; OPENCL_STR="NO"; HIDPI_STR="NO"; OPENGL_STR="YES"
    MONITOR_LAYOUT="unknown"
fi

# ── System banner: ASCII skull left (32) │ system info right (49) ───────────
# Per-line: "  ║  " (5) + art(32) + "  │  " (5) + info(49) + "  ║" (3) = 94
_TS="$(date '+%H:%M:%S')"
_RDIV="─────────────────────────────────────────────────"
_G='\033[32m'; _D='\033[2m'; _R0='\033[0m'
_yn() { [ "$1" = "YES" ] && printf "${_G}YES${_R0}" || printf "${_D}NO${_R0}"; }
_api() { printf "%-6s %s" "$1" "$(_yn "$2")"; }

# Parse monitor lines into array (one per display)
IFS=$'\n' read -r -d '' -a _MON <<< "$MONITOR_LAYOUT" 2>/dev/null || true
[ ${#_MON[@]} -eq 0 ] && _MON=("unknown")

# ASCII skull art — 17 lines (shifted down 1, left 2), each ≤32 chars
_A=(
    ""
    "       .,o'           \`o,."
    "     o8'                \`8o"
    "   o8:                    ;8o"
    "  .88                      88."
    "  :88.                    ,88:"
    "  \`888                    888'"
    "   888o   \`888 88 888'   o888"
    "  \`888o,. \`88 88 88' .,o888'"
    "    \`888888888 88 8888888888'"
    "     \`8888888 88 88888888'"
    "         \`::88 88 ;88;:'"
    "           88 88 88"
    "           88 88 88"
    "            8 88 8"
    ""
    "       Quake III IDE"
)

# Info column — 18 rows. API rows use ANSI color; no right-wall so width is free.
_R=(
    "Q3IDE BUILD ENVIRONMENT  [$_TS]"
    "$_RDIV"
    "Version   $Q3IDE_VERSION"
    "Release   $RELEASE_LABEL"
    "$(printf 'Arch  %-12s  OS    %s' "$Q3E_ARCH" "$SYS_TYPE")"
    "$_RDIV"
    "CPU   $CPU"
    "$(printf 'RAM   %-8s  GPU   %s' "$RAM" "$GPU")"
    "VRAM  $GPU_VRAM"
    "$_RDIV"
    "$(_api Metal "$METAL_STR")   $(_api OpenGL "$OPENGL_STR")   $(_api OpenCL "$OPENCL_STR")   $(_api HiDPI "$HIDPI_STR")"
    "$_RDIV"
    "${_MON[0]:-}"
    "${_MON[1]:-}"
    "${_MON[2]:-}"
    ""
    "$_RDIV"
    "${LEVEL:+Map   $LEVEL}"
)

# ASCII art needs 19 lines too — pad with blanks
_A+=( "" )

printf '\n'
printf '  ╔══════════════════════════════════════════════════════════════════════════════════════════\n'
for _i in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18; do
    printf '  ║  %-32s  │  %s\n' "${_A[$_i]}" "${_R[$_i]}"
done
printf '  ╚══════════════════════════════════════════════════════════════════════════════════════════\n'
printf '\n'

# 1. Build qagame + ui dylibs from ioq3 source (skipped with --engine-only)
if [ "$DO_ENGINE_ONLY" -eq 0 ]; then
    IOQ3_DIR="$ROOT/ioq3"
    ARCH="$(uname -m)"; case "$ARCH" in arm64|aarch64) ARCH=arm64 ;; *) ARCH=x86_64 ;; esac

    # Clone ioq3 once
    if [ ! -d "$IOQ3_DIR/.git" ]; then
        build_hdr "Cloning ioquake3 (first time)"
        git clone --depth=1 https://github.com/ioquake/ioq3.git "$IOQ3_DIR"
    fi

    # Ensure grapple patch
    G_CLIENT="$IOQ3_DIR/code/game/g_client.c"
    if ! grep -q "WP_GRAPPLING_HOOK.*STAT_WEAPONS\|STAT_WEAPONS.*WP_GRAPPLING_HOOK" "$G_CLIENT" 2>/dev/null; then
        build_hdr "Applying grapple spawn patch"
        sed -i.bak '/ammo\[WP_GRAPPLING_HOOK\] = -1/i\\tclient->ps.stats[STAT_WEAPONS] |= ( 1 << WP_GRAPPLING_HOOK );' "$G_CLIENT"
    fi

    Q3IDE_BUILD="$IOQ3_DIR/build_q3ide"
    mkdir -p "$Q3IDE_BUILD"
    GCFLAGS="-O2 -arch ${ARCH}"

    # Build qagame
    build_hdr "Building qagame"
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
    build_hdr "Building ui"
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
    build_hdr "Building capture dylib"
    cd "$ROOT/capture"
    cargo build --release
else
    build_hdr "Skipping capture dylib (--engine-only)"
fi

# 3. Build the engine
build_hdr "Building Quake3e engine ($RELEASE_BUILD)"
cd "$Q3E_DIR"
if [ "$DO_CLEAN" -eq 1 ]; then
    build_hdr "Cleaning engine build"
    make clean ARCH="$Q3E_ARCH"
elif [ "$DO_FAST" -eq 1 ]; then
    # --fast: hash-based delta detection.
    # Hashes code/q3ide/*.{c,h} + known engine files.
    # Only deletes .o for files that actually changed since last --fast run.
    # Any .h change → conservative: wipe all q3ide_*.o (header deps are shared).
    BUILD_DIR_FAST="$Q3E_DIR/build/release-$(uname -s | tr '[:upper:]' '[:lower:]')-${Q3E_ARCH}"
    if [ -d "$BUILD_DIR_FAST" ]; then
        build_hdr "Fast build: hash-based delta detection"
        python3 - "$Q3E_DIR" "$BUILD_DIR_FAST" <<'PYEOF'
import sys, os, hashlib, json

q3e_dir, build_dir = sys.argv[1], sys.argv[2]
hash_file = os.path.join(q3e_dir, '.q3ide_fast_hashes')

# Load previous hashes
old = {}
try:
    with open(hash_file) as f:
        old = json.load(f)
except Exception:
    pass

new = {}
deleted = set()

def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        h.update(f.read())
    return h.hexdigest()

def stem(fn):
    return os.path.splitext(os.path.basename(fn))[0]

def wipe_obj(s):
    """Delete <stem>.o from all known build subdirs."""
    for bd in ('client', 'rendergl1', 'rendergl2', 'rendervk'):
        obj = os.path.join(build_dir, bd, s + '.o')
        if os.path.exists(obj):
            os.remove(obj)
            deleted.add(s + '.o')

# ── q3ide sources ──────────────────────────────────────────────────────
q3ide_dir = os.path.join(q3e_dir, 'code', 'q3ide')
h_changed = False
c_changed = []
if os.path.isdir(q3ide_dir):
    for fn in sorted(os.listdir(q3ide_dir)):
        if not (fn.endswith('.c') or fn.endswith('.h')):
            continue
        path = os.path.join(q3ide_dir, fn)
        h = md5(path)
        new[path] = h
        if old.get(path) != h:
            if fn.endswith('.h'):
                h_changed = True
            else:
                c_changed.append(stem(fn))

if h_changed:
    # Conservative: any header change → wipe all q3ide_*.o
    client_dir = os.path.join(build_dir, 'client')
    if os.path.isdir(client_dir):
        for fn in os.listdir(client_dir):
            if fn.startswith('q3ide_') and fn.endswith('.o'):
                os.remove(os.path.join(client_dir, fn))
                deleted.add(fn)
    print('  [q3ide .h changed] wiped all q3ide_*.o')
else:
    for s in c_changed:
        wipe_obj(s)

# ── Known-modified engine files ────────────────────────────────────────
ENGINE_STEMS = {
    'cl_main', 'cl_cgame', 'cl_console', 'cl_cin', 'cl_keys', 'cl_input',
    'sdl_glimp', 'sdl_gamma', 'sdl_input',
    'unix_main', 'unix_shared',
    'common', 'sv_game',
    'tr_backend', 'tr_scene', 'tr_init', 'tr_local',
}
src_root = os.path.join(q3e_dir, 'code')
for dirpath, _, files in os.walk(src_root):
    for fn in files:
        if not fn.endswith('.c'):
            continue
        s = fn[:-2]
        if s not in ENGINE_STEMS:
            continue
        path = os.path.join(dirpath, fn)
        h = md5(path)
        new[path] = h
        if old.get(path) != h:
            wipe_obj(s)

# ── Save updated hashes ────────────────────────────────────────────────
with open(hash_file, 'w') as f:
    json.dump(new, f, indent=2)

# ── Report ─────────────────────────────────────────────────────────────
if deleted:
    for d in sorted(deleted):
        print(f'  wiped: {d}')
else:
    print('  no source changes detected — make will do nothing extra')
PYEOF
    fi
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

BUILD_DIR="$Q3E_DIR/build/release-${BUILD_OS}-${Q3E_ARCH}"
if [ ! -d "$BUILD_DIR" ]; then
    # Fallback for aarch64
    BUILD_DIR="$Q3E_DIR/build/release-${BUILD_OS}-aarch64"
fi
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build output directory not found. Checked:"
    echo "  - $Q3E_DIR/build/release-${BUILD_OS}-${Q3E_ARCH}"
    echo "  - $Q3E_DIR/build/release-${BUILD_OS}-aarch64"
    echo "Check make output above."
    exit 1
fi

# 4. Copy dylib/so next to the engine binary (skip if --engine-only)
if [ "$DO_ENGINE_ONLY" -eq 0 ]; then
    build_hdr "Copying capture library"
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
    build_hdr "Linking baseq3"
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
    PERMISSION_MARKER="$HOME/.config/q3ide/screen_permission_granted"
    mkdir -p "$HOME/.config/q3ide"
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
    ENGINE_ARGS="+set vm_game 0 +set vm_cgame 2 +set vm_ui 2 +set cl_renderer opengl1"

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
