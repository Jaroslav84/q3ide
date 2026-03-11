/*
 * q3ide_params.h — Single source of truth for all q3ide tunable constants.
 *
 * Change here; rebuild. No magic numbers scattered in .c files.
 * Runtime params struct (q3ide_params_t) is at the bottom.
 */
#pragma once
#include "../qcommon/q_shared.h"

#define Q3IDE_UNUSED __attribute__((unused))

/* ══════════════════════════════════════════════════════════════════
 * TEMPORARY FPS EXPERIMENTS — flip to 1 to disable, measure, flip back
 * ══════════════════════════════════════════════════════════════════ */

/* Set 1 to skip the 5× CM_BoxTrace LOS check per window per frame.
 * All windows render regardless of occlusion. Pure FPS cost measurement. */
#define Q3IDE_DISABLE_LOS_CHECK 0

/* Set 1 to skip q3ide_add_frame() (the 4 edge/border quads per window).
 * Windows lose the TV-chassis surround. Measures quad-submission cost. */
#define Q3IDE_DISABLE_EDGE_QUADS 0

/* ══════════════════════════════════════════════════════════════════
 * CAPS & THROTTLES — all hard limits live here so they're easy to find and remove
 * ══════════════════════════════════════════════════════════════════ */

/* Max consecutive cap_start failures to skip before giving up on a wall-shot
 * placement attempt. Prevents an infinite hang when SCK won't start a stream. */
#define Q3IDE_ATTACH_MAX_SKIP 8 /* retries */

/* Minimum pixel dimensions to bother capturing a window.
 * Tiny windows waste a scratch slot and produce blurry output. */
#define Q3IDE_MIN_WIN_W 400 /* px */
#define Q3IDE_MIN_WIN_H 300 /* px */

/* ── Window pool ─────────────────────────────────────────────────── */

#define Q3IDE_MAX_WIN 100 /* total window slots — must not exceed MAX_VIDEO_HANDLES */

/* ── Capture / streaming ─────────────────────────────────────────── */

/* ── SCK stream FPS cap ──────────────────────────────────────────────────────
 * Controls the minimumFrameInterval passed to SCStreamConfiguration.
 *
 *  -1  OFF — no cap passed to Apple at all. Apple's ScreenCaptureKit decides
 *            completely autonomously when to deliver frames (content-driven).
 *            Idle windows cost zero. Active windows get whatever the content
 *            refresh rate demands. THIS IS THE RECOMMENDED PRODUCTION VALUE.
 *            Never throttle Apple when it knows better than us.
 *
 *   0  STATIC IMAGE — passes fps=0 to SCK. Effectively freezes the stream
 *            after the first frame. Use only for debugging/screenshots.
 *
 *   1  ONE FRAME PER SECOND — slowest useful live cap. Good for background
 *            reference windows that barely change.
 *
 *  25  SOFT 25fps cap — Apple will not deliver more than 25 fps even if the
 *            window is changing faster. Does NOT guarantee 25fps (Apple's
 *            content-driven model still skips idle frames for free).
 *
 * IMPORTANT: Passed verbatim as a signed int through the C→Rust FFI.
 *            The Rust layer skips with_fps() entirely when value is < 0.
 *            Changing this value requires a FULL rebuild (Rust + engine).
 * ──────────────────────────────────────────────────────────────────────────*/
#define Q3IDE_CAPTURE_FPS           -1 /* -1=Apple decides, 0=static, N=max fps cap */
#define Q3IDE_CAPTURE_RING_BUF_SIZE 3  /* frames kept in the per-window ring buffer; 3 = current + 2 ahead */

/* ── Window sizing (world units) ────────────────────────────────── */

#define Q3IDE_WINDOW_DEFAULT_WIDTH  256.0f /* fallback world width when no wall geometry is available */
#define Q3IDE_WINDOW_DEFAULT_HEIGHT 192.0f /* fallback world height (4:3 aspect matches default width) */
#define Q3IDE_WINDOW_MIN_WIDTH      64.0f  /* smallest a window is ever clamped to — below this it's unreadable */
#define Q3IDE_WIN_INCHES            100.0f /* real-world target diagonal, inches */

/* ── Wall placement (legacy — shoot-to-place) ──────────────────── */

#define Q3IDE_WALL_DIST   512.0f /* wall-scan trace length */
#define Q3IDE_WALL_OFFSET 3.0f   /* push off wall surface to avoid z-fighting */


/* ── Spawn / attach ─────────────────────────────────────────────── */

/* World-unit width for Q3IDE_WIN_INCHES diagonal 16:9 at 24u/ft (2u/inch):
 *   width = diag * (16/sqrt(337)) * 2  ≈  diag * 1.7436                   */
#define Q3IDE_SPAWN_WIN_W    (Q3IDE_WIN_INCHES * 1.7436f) /* ≈174u for 100" diag */
#define Q3IDE_SPAWN_WIN_DIST 200.0f         /* distance ahead of player eye for initial placement */
#define Q3IDE_DISPLAY_ASPECT (16.0f / 9.0f) /* assumed aspect ratio for display captures (monitors are 16:9) */

/* ── Overview mode (hold O) ─────────────────────────────────────── */
/* Uses identical arc placement as Focus3; rows stack upward with a vertical gap. */
#define Q3IDE_OVERVIEW_DIST Q3IDE_FOCUS3_DIST /* spawn pos for newly-attached windows */
#define Q3IDE_OVERVIEW_GAP  5.0f              /* vertical gap between rows (world units) */

/* ── Focus3 mode (press I) ──────────────────────────────────────── */
#define Q3IDE_FOCUS3_HEIGHT    130.0f /* world height of each panel (fallback only) */
#define Q3IDE_FOCUS3_DIST      190.0f /* target arc radius from player eye ≈ 10 ft (24u/ft) */
#define Q3IDE_FOCUS3_MIN_DIST  50.0f  /* minimum safe arc radius after wall-clip shrink */
#define Q3IDE_TRIMON_ANGLE_DEG 30.0f  /* angle (degrees) between center and each side panel in the arc */

/* ── Triple-monitor arc — display slot assignment ───────────────── *
 * Maps logical position (LEFT/CENTER/RIGHT) to SCK scratch slots.  *
 * Slot numbers come from the physical monitor order reported by     *
 * ScreenCaptureKit. Swap these if your displays are wired           *
 * differently (LEFT↔RIGHT confusion is common with USB-C hubs).    */
#define Q3IDE_TRIMON_IDX_LEFT   0 /* macOS display index for LEFT  panel */
#define Q3IDE_TRIMON_IDX_CENTER 1 /* macOS display index for CENTER panel */
#define Q3IDE_TRIMON_IDX_RIGHT  2 /* macOS display index for RIGHT  panel */

/* ── Bright-map gamma correction ────────────────────────────────── */
/* When the loaded map's leaf name matches any pattern in Q3IDE_BRIGHT_MAPS,
 * r_gamma is multiplied by Q3IDE_BRIGHT_MAP_GAMMA and r_overbrightbits is set
 * to Q3IDE_BRIGHT_MAP_OVERBRIGHT_BITS. Both are restored on map change.
 *
 * Pattern syntax (matched against leaf name, case-insensitive):
 *   *   wildcard — matches any sequence of characters
 *   ?   wildcard — matches any single character
 *   No regex engine; uses simple glob matching (fnmatch-style).
 *
 * Add maps that are notoriously over-bright here. */
#define Q3IDE_BRIGHT_MAPS \
    "lun*",               /* Lunaran maps (lun3dm5, lunctf1, …) */ \
    /* add more patterns here, e.g. "ztn3dm*", "pukka3tourney*" */ \
    NULL

#define Q3IDE_BRIGHT_MAP_GAMMA           0.7f /* multiply r_gamma by this on bright maps */
#define Q3IDE_BRIGHT_MAP_OVERBRIGHT_BITS 0    /* flattest, most even lighting */

/* ── Interaction / timing ───────────────────────────────────────── */

#define Q3IDE_SHORTPRESS_MS          300  /* ms — hold threshold: >= this = hold (hide on release) */
#define Q3IDE_VIEWMODE_TOGGLE_OFF_MS 3000 /* ms after showing before a second tap can hide the layout */
#define Q3IDE_REPOSITION_MS          3000 /* ms window stays selected waiting for shoot-to-move destination */
#define Q3IDE_PLACE_COOLDOWN_MS      2000 /* ms after placing a window before newly-placed windows can be selected */
#define Q3IDE_IDLE_TIMEOUT_MS        5000 /* ms without a frame before window is marked IDLE */

/* ── Effects ────────────────────────────────────────────────────── */

#define Q3IDE_HOVER_GLOW         1.15f /* color multiplier at full hover */
#define Q3IDE_HOVER_LIFT_Z       4.0f  /* units toward player at full hover */
#define Q3IDE_HOVER_FADE_IN_SEC  0.15f /* hover animation duration */
#define Q3IDE_HOVER_BORDER_THICK 1.0f  /* border quad thickness in world units */

/* ── Overlay HUD (left monitor keybinding panel) ────────────────── */

#define Q3IDE_OVL_CHAR_W       0.14f /* bigchars character width in world units */
#define Q3IDE_OVL_CHAR_H       0.20f /* bigchars character height */
#define Q3IDE_OVL_LINE_H       0.30f /* line pitch */
#define Q3IDE_OVL_GAP          0.22f /* gap between key column and label column */
#define Q3IDE_OVL_KEY_W        0.56f /* fixed key column width (4 chars) */
#define Q3IDE_OVL_DIST         10.0f /* distance from camera — small so always in front of walls */
#define Q3IDE_OVL_SMALL_SCALE  0.6f  /* small text variant scale factor */
#define MAX_LEFT_UI_RENDER_FPS 2     /* keyboard section rebuilt at most 2x per second */
#define OVL_REBUILD_MS         500   /* 1000 / MAX_LEFT_UI_RENDER_FPS */
#define Q3IDE_OVL_KEY_CELL     0.28f /* per-key column stride (2 chars + margin) */
#define Q3IDE_OVL_KEY_ROW_H    0.26f /* row pitch for keyboard grid — tighter than OVL_LINE_H */

/* ── Teleport history ───────────────────────────────────────────── */

#define Q3IDE_TELE_HIST 20 /* ring buffer slots for teleport position history */

/* ── Post-MVP: animations ───────────────────────────────────────── */

#define Q3IDE_ANIM_SPAWN_SEC  0.35f /* window spawn animation duration in seconds */
#define Q3IDE_ANIM_CLOSE_SEC  0.25f /* window close animation duration in seconds */
#define Q3IDE_ANIM_GRAB_SCALE 1.02f /* scale factor applied while window is grabbed/dragged */
#define Q3IDE_ANIM_LERP_SPEED 8.0f  /* lerp speed for smooth position transitions (units/s) */

/* ── Post-MVP: window chrome ────────────────────────────────────── */

#define Q3IDE_CORNER_RADIUS      12.0f /* rounded corner radius in world units */
#define Q3IDE_WINDOW_BAR_HEIGHT  32.0f /* title bar height in world units */
#define Q3IDE_WINDOW_BAR_OVERLAP 20.0f /* how far the title bar overlaps the window content area */
#define Q3IDE_ORNAMENT_Z_OFFSET  8.0f  /* Z push toward player for ornament geometry (avoids z-fighting) */
#define Q3IDE_ORNAMENT_OVERLAP   20.0f /* ornament extends this many units beyond window edge */
#define Q3IDE_MIN_TAP_TARGET     60.0f /* minimum tap/click target size in world units (accessibility) */

/* ── Post-MVP: status overlay tints ────────────────────────────── */
/* RGBA tints drawn over windows to indicate CI/build status.       */

#define Q3IDE_STATUS_PASS_R  0.2f  /* pass: green tint R */
#define Q3IDE_STATUS_PASS_G  0.9f  /* pass: green tint G */
#define Q3IDE_STATUS_PASS_B  0.4f  /* pass: green tint B */
#define Q3IDE_STATUS_PASS_A  0.15f /* pass: overlay opacity */
#define Q3IDE_STATUS_FAIL_R  1.0f  /* fail: red tint R */
#define Q3IDE_STATUS_FAIL_G  0.3f  /* fail: red tint G */
#define Q3IDE_STATUS_FAIL_B  0.3f  /* fail: red tint B */
#define Q3IDE_STATUS_FAIL_A  0.15f /* fail: overlay opacity */
#define Q3IDE_STATUS_BUILD_R 1.0f  /* building: amber tint R */
#define Q3IDE_STATUS_BUILD_G 0.8f  /* building: amber tint G */
#define Q3IDE_STATUS_BUILD_B 0.2f  /* building: amber tint B */
#define Q3IDE_STATUS_BUILD_A 0.15f /* building: overlay opacity */

/* ── Runtime params (accent colour, visual sizes) ───────────────────
 * Defined once in q3ide_scene.c; read everywhere else as const.   */
typedef struct {
	/* Border frame width in world units — applied to all hover/select borders. */
	float borderThickness;
	/* TV chassis depth in world units — edge quads span this around the screen perimeter. */
	float windowDepth;
	/* Accent colour (RGB) used for borders and scratch slot 63 texture.
	 * "Quake red": deep crimson matching the Q3 HUD palette. */
	byte accentColor[3]; /* {R, G, B} */
} q3ide_params_t;

/* Singleton — defined in q3ide_scene.c, read-only everywhere else. */
extern const q3ide_params_t q3ide_params;
