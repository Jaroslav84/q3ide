/*
 * q3ide_params.h — Single source of truth for all q3ide tunable constants.
 *
 * Change here; rebuild. No magic numbers scattered in .c files.
 * Runtime params struct (q3ide_params_t) is at the bottom.
 */

/*
RULES:
- DO NOT FUCK UP MY TABULATIONS AFTER PARAM NAME!!! Q3IDE_SOME PARAM			<-- until this point is reached
- IF YOU ADD NEW PARAMS THEN MARK IT WITH "NEW" IN COMMENTS and do NOT use tabulations. This will I will spot the
changes easily.
*/

#pragma once
#include "../qcommon/q_shared.h"

#define Q3IDE_UNUSED __attribute__((unused))

/* ══════════════════════════════════════════════════════════════════
 * DEBUG TOGGLES — flip to 1 to disable feature, measure FPS, flip back
 * ══════════════════════════════════════════════════════════════════ */

#define Q3IDE_DISABLE_LOS_CHECK  0 /* 1 = skip CM_BoxTrace LOS per window — all render, measures occlusion cost */
#define Q3IDE_DISABLE_EDGE_QUADS 0 /* 1 = skip 4 border quads per window — measures quad-submission cost */

/* ── Window pool ─────────────────────────────────────────────────── */

#define Q3IDE_MAX_WIN   100 /* total window slots — must not exceed MAX_VIDEO_HANDLES */
#define Q3IDE_MIN_WIN_W 42  /* px — minimum window width to bother capturing */
#define Q3IDE_MIN_WIN_H 42  /* px — minimum window height to bother capturing */

/* ── SCK streaming ───────────────────────────────────────────────── */

/* SCK stream FPS cap — controls minimumFrameInterval in SCStreamConfiguration.
 *  -1  OFF        Apple decides autonomously (content-driven). Idle = zero cost. RECOMMENDED.
 *   0  STATIC     Passes fps=0 — freezes after first frame. Debug/screenshots only.
 *   1  1 FPS      Slowest live cap. Good for barely-changing background windows.
 *  25  25 FPS cap Apple won't deliver more than 25fps. Still skips idle frames for free.
 * Passed verbatim as signed int through C→Rust FFI. Requires FULL rebuild to change. */
#define Q3IDE_CAPTURE_FPS               -1         /* -1=Apple decides, 0=static, N=max fps cap */
#define Q3IDE_SCK_POLL_INTERVAL_NS      1000000000 /* ns  — background poll thread wake interval (1s) */
#define Q3IDE_SCK_THROTTLE_NOFRAME_MS   1000ULL    /* ms  — stream considered stalled after this long without a frame */
#define Q3IDE_SCK_THROTTLE_RETRIGGER_MS 2000ULL    /* ms  — minimum gap between throttle warning re-fires */
#define Q3IDE_SCK_ATTACH_MAX_SKIP       8   /* retries — max cap_start failures before giving up on a placement */
#define Q3IDE_SCK_FOCUS3_RETRY_COUNT    10  /* retries — max cap_start_disp failures during focus3 show */
#define Q3IDE_SCK_FOCUS3_RETRY_MS       300 /* ms  — delay between each focus3 retry attempt */
#define Q3IDE_CAPTURE_RING_BUF_SIZE     3   /* NOT_USED_YET — per-window ring buffer depth (current + 2 ahead) */

/* ── Multi-monitor rendering ─────────────────────────────────────── */

#define Q3IDE_MAX_MONITORS                                                                                             \
	1024 /* max monitor slots in sorted[] render array. Bruhuh, who knows maybe a millionare wants to play on many     \
	        monitors */
#define Q3IDE_MONITOR_FOV 90.0f /* per-monitor horizontal FOV in degrees */

/* Logical→physical display index mapping (swap if LEFT↔RIGHT appear miswired) */
#define Q3IDE_TRIMON_IDX_LEFT   0 /* macOS display index for LEFT   panel */
#define Q3IDE_TRIMON_IDX_CENTER 1 /* macOS display index for CENTER panel */
#define Q3IDE_TRIMON_IDX_RIGHT  2 /* macOS display index for RIGHT  panel */

/* ── View modes — Focus3 (I) + Overview (O) ─────────────────────── */

#define Q3IDE_VIEWMODE_ARC_DIST 190.0f /* world units — arc spawn radius shared by Focus3 and Overview (~10ft) */
#define Q3IDE_FOCUS3_MIN_DIST   75.0f  /* world units — minimum arc radius after wall-clip shrink */
#define Q3IDE_OVERVIEW_GAP      5.0f   /* world units — vertical gap between Overview grid rows */
#define Q3IDE_OVERVIEW_SCROLL_Z_STEP                                                                                   \
	120.0f /* world units shifted per scroll tick — continuous vertical slide of the O grid */
#define Q3IDE_OVERVIEW_SCROLL_COOLDOWN_MS                                                                              \
	150 /* ms — min interval between O scroll ticks; trackpad fires many events per gesture */
#define Q3IDE_OVERVIEW_SCROLL_IDLE_MS 400 /* ms of no scroll events before layout re-snaps to player view */

/* ── LOS & tracing ───────────────────────────────────────────────── */

#define Q3IDE_LOS_HIT_THRESHOLD       0.95f  /* trace fraction counted as "clear" — allows slight wall graze */
#define Q3IDE_LOS_CORNER_INSET        0.9f   /* corner sample inset (0..1) — samples at ±90% of half-size */
#define Q3IDE_LOS_PROXIMITY_MULT      3.0f   /* skip LOS when player is within this × window diagonal */
#define Q3IDE_LOS_PROXIMITY_HYST_MULT 4.0f   /* hysteresis: once proximity-exempt, stay exempt until this × diagonal */
#define Q3IDE_TRACE_BOX_HALF          4.0f   /* half-extent of box trace for arc/focus3 wall distance queries */
#define Q3IDE_CLAMP_MIN_SIZE          32.0f  /* world units — minimum size a clamped window may shrink to */
#define Q3IDE_CLAMP_WALL_GAP          1.5f   /* world units — margin kept between window edge and wall */
#define Q3IDE_TRACE_PLANE_EPS         0.001f /* ray-plane dot threshold below which ray is parallel to plane */

/* ── Wall placement ──────────────────────────────────────────────── */

#define Q3IDE_WALL_DIST       6666.0f /* world units — wall-scan trace length */
#define Q3IDE_WALL_OFFSET     5.0f    /* world units — push off wall surface to avoid z-fighting */
#define Q3IDE_WALL_MAX_TILT_Z 0.5f    /* |normal[2]| threshold — rejects floors/ceilings/tilted surfaces (sin 30°) */
#define Q3IDE_WALL_WINDOWS_OFFSET                                                                                      \
	0.1f /* world units — extra depth per overlapping window on same wall (z-fight stack) */
#define Q3IDE_WALL_OVERLAP_PLANE_TOL 2.0f    /* world units — max along-normal gap to consider two windows coplanar */
#define Q3IDE_WALL_SLOT_ASPECT       0.8718f /* 16:9 width coefficient: slot_w = ideal_diag × this (≈87u per 100u) */
#define Q3IDE_WALL_MARGIN            8       /* world units — reserved on each side of wall edge */
#define Q3IDE_WINDOW_GAP             8       /* world units — gap between adjacent windows on same wall */

/* ── Placement — window sizing ───────────────────────────────────── */

#define Q3IDE_IDEAL_WINDOW_SIZE 600   /* world units diagonal — ideal window size */
#define Q3IDE_MIN_WINDOW_SIZE   400   /* world units diagonal — absolute floor */
#define Q3IDE_MAX_WINDOW_SIZE   800   /* world units diagonal — ceiling (matches spawn size) */
#define Q3IDE_WINDOW_WALL_RATIO 0.85f /* window height = this fraction of wall height */
#define Q3IDE_MAX_CACHED_WALLS  128   /* max walls stored in area wall cache */
#define Q3IDE_MAX_WALL_SLOTS    8     /* max window slots pre-computed per wall */
#define Q3IDE_MIN_WALL_HEIGHT   59    /* world units — walls shorter than this are skipped (~1.5m) */
#define Q3IDE_MAX_WALL_HEIGHT   394   /* world units — walls taller than this get capped sizing (~10m) */
#define Q3IDE_MIN_WALL_WIDTH    114   /* world units — min_window_width(66) + 2×margin(24) */

/* ── Spawn & sizing defaults ─────────────────────────────────────── */

/* World-unit width for Q3IDE_WIN_INCHES diagonal 16:9 at 24u/ft (2u/inch):
 *   width = diag × (16/√337) × 2  ≈  diag × 1.7436                          */
#define Q3IDE_WORLD_UNITS_PER_INCH  2.0f   /* 2u per inch — 24u/ft scale; used for size conversions */
#define Q3IDE_WIN_INCHES            100.0f /* real-world diagonal in inches — drives spawn size */
#define Q3IDE_SPAWN_WIN_W           (Q3IDE_WIN_INCHES * 1.7436f) /* ≈174u world width for 100" diagonal */
#define Q3IDE_DISPLAY_ASPECT        (16.0f / 9.0f)               /* assumed aspect ratio for display captures */
#define Q3IDE_WINDOW_DEFAULT_WIDTH  256.0f /* NOT_USED_YET — fallback world width when no wall geometry */
#define Q3IDE_WINDOW_DEFAULT_HEIGHT 192.0f /* NOT_USED_YET — fallback world height (4:3 matches default width) */
#define Q3IDE_WINDOW_MIN_WIDTH      64.0f  /* NOT_USED_YET — smallest world width before window is unreadable */
#define Q3IDE_SPAWN_WIN_DIST        200.0f /* NOT_USED_YET — distance ahead of player eye for initial placement */

/* ── Drag & Resize (CMD + aim-to-move, scroll-resize) ───────────── */
#define Q3IDE_RESIZE_SCROLL_STEP 80.0f   /* world units added/removed per scroll tick */
#define Q3IDE_RESIZE_MIN_INCHES  10.0f   /* minimum resize diagonal in real-world inches */
#define Q3IDE_RESIZE_MAX_INCHES  9999.0f /* maximum resize diagonal — effectively no limit */
#define Q3IDE_RESIZE_MIN_DIAG    (Q3IDE_RESIZE_MIN_INCHES * Q3IDE_WORLD_UNITS_PER_INCH)
#define Q3IDE_RESIZE_MAX_DIAG    (Q3IDE_RESIZE_MAX_INCHES * Q3IDE_WORLD_UNITS_PER_INCH)

/* ── Interaction & timing ────────────────────────────────────────── */

#define Q3IDE_SHORTPRESS_MS          300  /* ms — hold threshold: below = tap, at or above = hold */
#define Q3IDE_REPOSITION_MS          3000 /* ms — window stays selected waiting for shoot-to-move target */
#define Q3IDE_PLACE_COOLDOWN_MS      2000 /* ms — after placing, this long before newly-placed can be selected */
#define Q3IDE_BURST_PLACE_MS         150  /* ms — min interval between placements during rapid-fire sweep */
#define Q3IDE_IDLE_TIMEOUT_MS        5000 /* ms — no frame for this long → window marked IDLE */
#define Q3IDE_AUTOEXEC_DELAY_MS      1000 /* ms — after map load before firing the autoexec nextdemo command */
#define Q3IDE_HEARTBEAT_MS           5000 /* ms — interval between heartbeat FPS log entries */
#define Q3IDE_HUD_CONFIRM_MS         1000 /* ms — brief confirmation toast (mode off, not in game) */
#define Q3IDE_HUD_STATUS_MS          1500 /* ms — status toast (mode activated, no windows found) */
#define Q3IDE_HUD_ERROR_MS           2000 /* ms — error toast (no dylib, no displays found) */
#define Q3IDE_MAP_BANNER_MS          3000 /* ms — map name banner shown on map load */
#define Q3IDE_VIEWMODE_TOGGLE_OFF_MS 3000 /* NOT_USED_YET — ms after show before second tap can hide layout */

/* ── Overlay HUD ─────────────────────────────────────────────────── */

#define Q3IDE_OVL_DIST 10.0f /* world units — overlay distance from camera (always in front of walls) */
/* Calibrated 2026-03-13: 22 normal chars measured 284px on 1918px/20wu screen.
 * → 12.9px/char → 0.134wu. Small (×0.6): 7.7px/char. LINE_H=0.30 confirmed. */
#define Q3IDE_OVL_CHAR_W      0.134f                                /* world units — bigchars character advance width */
#define Q3IDE_OVL_CHAR_H      0.20f                                 /* world units — bigchars character height */
#define Q3IDE_OVL_CHAR_ASPECT (Q3IDE_OVL_CHAR_W / Q3IDE_OVL_CHAR_H) /* ~0.67 */
#define Q3IDE_OVL_LINE_H      0.30f                                 /* world units — line pitch (text rows) */
#define Q3IDE_OVL_SMALL_SCALE 0.6f                                  /* scale multiplier for small text variant */
#define Q3IDE_OVL_KEY_ROW_H   0.26f /* world units — row pitch for keyboard grid (tighter than LINE_H) */
#define Q3IDE_OVL_LABEL_ABOVE 0.12f /* world units — ux offset above star row for -90° label anchor */
#define OVL_REBUILD_MS        500   /* ms — keyboard cache rebuild interval (2 Hz) */
/* Overlay layout — pixel anchors converted via q3ide_ovl_pixel_pos() */
#define Q3IDE_OVL_KB_LEFT_MARGIN_PX  10 /* px — gap from left screen edge to keyboard */
#define Q3IDE_OVL_KB_TOP_MARGIN_PX   10 /* px — gap from top to keyboard anchor */
#define Q3IDE_OVL_WL_LEFT_MARGIN_PX  10 /* px — gap from left screen edge to window list */
#define Q3IDE_OVL_WL_RIGHT_MARGIN_PX 20 /* NEW px — gap from right screen edge to window list anchor */
#define Q3IDE_OVL_WL_CONTENT_PX 260 /* NEW px — total panel width (lamps + max label); anchor = right-margin - this */
#define Q3IDE_OVL_AREA_LABEL_BOTTOM_PX  10 /* px — distance from screen bottom for area/room label */
#define Q3IDE_OVL_WINLIST_BOTTOM_PX     10 /* px — distance from bottom of screen to winlist bottom edge */
#define Q3IDE_OVL_WINLIST_HDR_OFFSET_PX 50 /* px — extra downward offset for Windows:/highlight header */
#define Q3IDE_AIM_LABEL_TOP_PX          30 /* NEW px — aimed window label distance from top of each monitor */
#define Q3IDE_OVL_WINHDR_UP_PX          30 /* NEW px — upward offset for WINDOWS: header vs default wl_top */

/* Text layout — derived from calibrated char size */
#define Q3IDE_OVL_PX_TO_WU     (Q3IDE_OVL_CHAR_W / 12.9f) /* ~0.01039 wu/px — pixel to world-unit conversion */
#define Q3IDE_TEXT_PADDING_TOP 10                         /* px — gap from one text line to the next */
#define Q3IDE_OVL_SM_LINE_H                                                                                            \
	(Q3IDE_OVL_CHAR_H * Q3IDE_OVL_SMALL_SCALE +                                                                        \
	 Q3IDE_TEXT_PADDING_TOP * Q3IDE_OVL_PX_TO_WU)             /* ~0.224 wu — small text line stride */
#define Q3IDE_OVL_SECTION_PAD_WU (20.0f * Q3IDE_OVL_PX_TO_WU) /* ~0.208 wu — 20px section gap between UI groups */

/* NOT_USED_YET overlay layout */
#define Q3IDE_OVL_GAP          0.22f /* NOT_USED_YET — gap between key column and label column */
#define Q3IDE_OVL_KEY_W        0.56f /* NOT_USED_YET — fixed key column width (4 chars) */
#define Q3IDE_OVL_KEY_CELL     0.28f /* NOT_USED_YET — per-key column stride (2 chars + margin) */
#define MAX_LEFT_UI_RENDER_FPS 2     /* NOT_USED_YET — keyboard section rebuilt at most 2× per second */

#include "q3ide_params_windows.h"

/* ── Window background (black + logo behind tunnel face) ────────── */

#define Q3IDE_BG_DEPTH_OFFSET 0.0001f /* normal units — bg pushed behind tunnel face to prevent z-fight */

/* ── Async logging ───────────────────────────────────────────────── */

#define Q3IDE_LOG_QUEUE_CAP 512 /* NEW — ring buffer slot count (must be power of 2); drops silently on overflow */
#define Q3IDE_LOG_LINE_LEN  512 /* NEW — max formatted line length (level+ts+msg+newline) */

/* ── Teleport history ────────────────────────────────────────────── */

#define Q3IDE_TELE_HIST 20 /* ring buffer slots for teleport position history */

/* ══════════════════════════════════════════════════════════════════
 * POST-MVP — not yet implemented. Do not remove; values are calibrated.
 * ══════════════════════════════════════════════════════════════════ */

/* Hover effects */
#define Q3IDE_HOVER_GLOW         1.15f /* NOT_USED_YET — color multiplier at full hover */
#define Q3IDE_HOVER_LIFT_Z       4.0f  /* NOT_USED_YET — units toward player at full hover */
#define Q3IDE_HOVER_FADE_IN_SEC  0.15f /* NOT_USED_YET — hover animation duration in seconds */
#define Q3IDE_HOVER_BORDER_THICK 1.0f  /* NOT_USED_YET — border quad thickness in world units */

/* Animations */
#define Q3IDE_ANIM_SPAWN_SEC  0.35f /* NOT_USED_YET — window spawn animation duration in seconds */
#define Q3IDE_ANIM_CLOSE_SEC  0.25f /* NOT_USED_YET — window close animation duration in seconds */
#define Q3IDE_ANIM_GRAB_SCALE 1.02f /* NOT_USED_YET — scale factor while window is grabbed/dragged */
#define Q3IDE_ANIM_LERP_SPEED 8.0f  /* NOT_USED_YET — lerp speed for smooth position transitions */

/* Window chrome */
#define Q3IDE_CORNER_RADIUS      12.0f /* NOT_USED_YET — rounded corner radius in world units */
#define Q3IDE_WINDOW_BAR_HEIGHT  32.0f /* NOT_USED_YET — title bar height in world units */
#define Q3IDE_WINDOW_BAR_OVERLAP 20.0f /* NOT_USED_YET — title bar overlap into window content area */
#define Q3IDE_ORNAMENT_Z_OFFSET  8.0f  /* NOT_USED_YET — Z push toward player for ornament geometry */
#define Q3IDE_ORNAMENT_OVERLAP   20.0f /* NOT_USED_YET — ornament extension beyond window edge */
#define Q3IDE_MIN_TAP_TARGET     60.0f /* NOT_USED_YET — minimum tap/click target size (accessibility) */

/* CI / build status overlay tints */
#define Q3IDE_STATUS_PASS_R  0.2f  /* NOT_USED_YET — pass tint R */
#define Q3IDE_STATUS_PASS_G  0.9f  /* NOT_USED_YET — pass tint G */
#define Q3IDE_STATUS_PASS_B  0.4f  /* NOT_USED_YET — pass tint B */
#define Q3IDE_STATUS_PASS_A  0.15f /* NOT_USED_YET — pass tint alpha */
#define Q3IDE_STATUS_FAIL_R  1.0f  /* NOT_USED_YET — fail tint R */
#define Q3IDE_STATUS_FAIL_G  0.3f  /* NOT_USED_YET — fail tint G */
#define Q3IDE_STATUS_FAIL_B  0.3f  /* NOT_USED_YET — fail tint B */
#define Q3IDE_STATUS_FAIL_A  0.15f /* NOT_USED_YET — fail tint alpha */
#define Q3IDE_STATUS_BUILD_R 1.0f  /* NOT_USED_YET — building tint R */
#define Q3IDE_STATUS_BUILD_G 0.8f  /* NOT_USED_YET — building tint G */
#define Q3IDE_STATUS_BUILD_B 0.2f  /* NOT_USED_YET — building tint B */
#define Q3IDE_STATUS_BUILD_A 0.15f /* NOT_USED_YET — building tint alpha */

/* ── Runtime params (set once at init, read everywhere) ─────────────
 * Defined in q3ide_scene.c; extern everywhere else.                 */
typedef struct {
	float borderThickness; /* border frame width in world units — applied to all hover/select borders */
	float windowDepth;     /* TV chassis depth in world units — edge quads span this around perimeter */
	byte accentColor[3];   /* RGB accent colour for borders and scratch slot 63 texture */
} q3ide_params_t;

extern const q3ide_params_t q3ide_params;
