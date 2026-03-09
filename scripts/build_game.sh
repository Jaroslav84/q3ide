#!/bin/sh
# build_game.sh — Build qagame dylib from ioquake3 source with grapple hook at spawn.
#
# ioquake3 is Quake3e's upstream. Their game module ABI is compatible.
# Unlike the pre-built quake3e qagame, ioquake3 has full weapon spawn control.
# g_client.c is patched to add WP_GRAPPLING_HOOK to STAT_WEAPONS at every spawn.
#
# Output: baseq3/qagamex86_64.dylib  (replaces existing pre-built one)
# Run on macOS: sh ./scripts/build_game.sh
# Or via API:   api('POST', '/build_game')

set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IOQ3_DIR="$ROOT/ioq3"
OUT_DIR="$ROOT/baseq3"

ARCH="$(uname -m)"
case "$ARCH" in
    arm64|aarch64) ARCH=arm64 ;;
    *) ARCH=x86_64 ;;
esac

echo "=== Building qagame with grapple (ioquake3 source, arch=$ARCH) ==="

# ── 1. Clone ioquake3 (shallow) ───────────────────────────────────────────────
if [ ! -d "$IOQ3_DIR/.git" ]; then
    echo "--- Cloning ioquake3..."
    git clone --depth=1 https://github.com/ioquake/ioq3.git "$IOQ3_DIR"
else
    echo "--- ioquake3 already cloned at $IOQ3_DIR"
fi

# ── 2. Verify grapple patch ───────────────────────────────────────────────────
# g_client.c must have WP_GRAPPLING_HOOK added to STAT_WEAPONS at spawn.
# This is patched directly in the ioq3/ source tree (not via sed at build time).
G_CLIENT="$IOQ3_DIR/code/game/g_client.c"
if grep -q "WP_GRAPPLING_HOOK.*STAT_WEAPONS\|STAT_WEAPONS.*WP_GRAPPLING_HOOK" "$G_CLIENT"; then
    echo "--- Grapple spawn patch confirmed in g_client.c"
else
    echo "--- Applying grapple spawn patch to g_client.c..."
    sed -i.bak '/ammo\[WP_GRAPPLING_HOOK\] = -1/i\\tclient->ps.stats[STAT_WEAPONS] |= ( 1 << WP_GRAPPLING_HOOK );' "$G_CLIENT"
fi

# ── 3. Build game module only ─────────────────────────────────────────────────
# ioquake3 Makefile supports building just the game:
#   BUILD_CLIENT=0 BUILD_SERVER=0 BUILD_GAME_SO=1 BUILD_GAME_QVM=0
# Output: build/release-darwin-<arch>/baseq3/qagame<arch>.dylib
echo "--- Compiling qagame with clang (takes ~30s)..."
SRC="$IOQ3_DIR/code/game"
BUILD_DIR="$IOQ3_DIR/build_q3ide"
mkdir -p "$BUILD_DIR"

# Compile all game .c files to .o
GAME_SRCS="
    g_main.c ai_chat.c ai_cmd.c ai_dmnet.c ai_dmq3.c ai_main.c ai_team.c ai_vcmd.c
    bg_misc.c bg_pmove.c bg_slidemove.c bg_lib.c
    g_active.c g_arenas.c g_bot.c g_client.c g_cmds.c g_combat.c
    g_items.c g_mem.c g_misc.c g_missile.c g_mover.c g_session.c
    g_spawn.c g_svcmds.c g_target.c g_team.c g_trigger.c g_utils.c g_weapon.c
    g_syscalls.c
"

CFLAGS="-O2 -arch ${ARCH} -I${SRC} -DQAGAME -DQ3_VM_LINKED"
OBJS=""
for f in $GAME_SRCS; do
    obj="$BUILD_DIR/$(basename ${f%.c}.o)"
    clang $CFLAGS -c "$SRC/$f" -o "$obj" 2>&1 || { echo "ERROR: compile failed: $f"; exit 1; }
    OBJS="$OBJS $obj"
done

BUILD_OUT="$BUILD_DIR/qagame${ARCH}.dylib"
clang -dynamiclib -arch ${ARCH} -undefined suppress -flat_namespace \
    $OBJS -o "$BUILD_OUT" 2>&1

if [ ! -f "$BUILD_OUT" ]; then
    echo "ERROR: Link failed — output not found"
    exit 1
fi

# ── 4. Copy qagame to baseq3 ──────────────────────────────────────────────────
cp "$BUILD_OUT" "$OUT_DIR/qagame${ARCH}.dylib"
cp "$BUILD_OUT" "$OUT_DIR/qagame.dylib"

# ── 5. Build ui dylib (accent color #FF34DD) ──────────────────────────────────
echo "--- Compiling ui with clang..."
UI_SRC="$IOQ3_DIR/code/q3_ui"
UI_BUILD_DIR="$IOQ3_DIR/build_ui_q3ide"
mkdir -p "$UI_BUILD_DIR"

UI_SRCS="
    ui_main.c ui_atoms.c ui_connect.c ui_controls2.c ui_demo2.c
    ui_cdkey.c ui_ingame.c ui_loadconfig.c ui_menu.c ui_mfield.c
    ui_mods.c ui_network.c ui_options.c ui_playermodel.c ui_players.c
    ui_qmenu.c ui_saveconfig.c ui_serverinfo.c ui_servers2.c
    ui_setup.c ui_sound.c ui_sparena.c ui_specifyleader.c ui_specifyteam.c
    ui_splevel.c ui_sppostgame.c ui_spreset.c ui_spskill.c ui_startserver.c
    ui_team.c ui_teamorders.c ui_video.c ui_syscalls.c
    ui_addbots.c ui_removebots.c ui_cinematics.c
    ../game/bg_misc.c ../game/bg_lib.c
"

UI_CFLAGS="-O2 -arch ${ARCH} -I${UI_SRC} -I${IOQ3_DIR}/code/game -DUI -DQ3_VM_LINKED"
UI_OBJS=""
for f in $UI_SRCS; do
    src_path="$UI_SRC/$f"
    obj="$UI_BUILD_DIR/$(basename ${f%.c}.o)"
    clang $UI_CFLAGS -c "$src_path" -o "$obj" 2>&1 || { echo "WARN: ui compile failed: $f (skipping)"; continue; }
    UI_OBJS="$UI_OBJS $obj"
done

UI_OUT="$UI_BUILD_DIR/ui${ARCH}.dylib"
clang -dynamiclib -arch ${ARCH} -undefined suppress -flat_namespace \
    $UI_OBJS -o "$UI_OUT" 2>&1 && {
    cp "$UI_OUT" "$OUT_DIR/ui${ARCH}.dylib"
    cp "$UI_OUT" "$OUT_DIR/ui.dylib"
    echo "    $OUT_DIR/ui${ARCH}.dylib (accent #FF34DD)"
} || echo "WARN: ui link failed — keeping existing ui.dylib"

echo ""
echo "=== Done ==="
echo "    $OUT_DIR/qagame${ARCH}.dylib"
echo ""
echo "Grapple hook + accent color #FF34DD applied."
echo "Restart the game to apply."
