/*
 * q3ide_params.h — Single source of truth for all q3ide tunable constants.
 *
 * Change here; rebuild. No magic numbers scattered in .c files.
 * Runtime params struct (q3ide_params_t) is at the bottom.
 */
#pragma once
#include "../qcommon/q_shared.h"

/* ══════════════════════════════════════════════════════════════════
 * CAPS & THROTTLES — all hard limits live here so they're easy to find and remove
 * ══════════════════════════════════════════════════════════════════ */

/* FPS cap for OS screen-capture tunnel windows. SCK has an undocumented
 * concurrent-stream hardware limit (~9 streams on this machine); keeping tunnel
 * fps lower leaves render budget for the rest of the game.
 * Set equal to Q3IDE_FPS_FULL to remove the cap. */
#define Q3IDE_MAX_TUNNEL_FPS     25 /* fps — tunnel window hard cap */

/* Max consecutive cap_start failures to skip before giving up on a wall-shot
 * placement attempt. Prevents an infinite hang when SCK won't start a stream. */
#define Q3IDE_ATTACH_MAX_SKIP     8 /* retries */

/* Minimum pixel dimensions to bother capturing a window.
 * Tiny windows waste a scratch slot and produce blurry output. */
#define Q3IDE_MIN_WIN_W         400 /* px */
#define Q3IDE_MIN_WIN_H         300 /* px */

/* ── Window pool ─────────────────────────────────────────────────── */

#define Q3IDE_MAX_WIN            96 /* total window slots: 64 capture + headroom for display slices */

/* ── Capture / streaming ─────────────────────────────────────────── */

#define Q3IDE_CAPTURE_FPS        60 /* SCK stream target fps */
#define Q3IDE_CAPTURE_RING_BUF_SIZE 3

/* ── Distance-based FPS throttle ────────────────────────────────── */

#define Q3IDE_DIST_FULL       200.0f /* closer than this → full fps */
#define Q3IDE_DIST_HALF       500.0f /* → half fps */
#define Q3IDE_DIST_LOW       1000.0f /* → low fps */
#define Q3IDE_FPS_FULL           60
#define Q3IDE_FPS_HALF           30
#define Q3IDE_FPS_LOW            15
#define Q3IDE_FPS_MIN             5

/* ── Window sizing (world units) ────────────────────────────────── */

#define Q3IDE_WINDOW_DEFAULT_WIDTH   256.0f
#define Q3IDE_WINDOW_DEFAULT_HEIGHT  192.0f
#define Q3IDE_WINDOW_MIN_WIDTH        64.0f
#define Q3IDE_WIN_INCHES            150.0f /* real-world target diagonal, inches */

/* ── Wall placement ─────────────────────────────────────────────── */

#define Q3IDE_WALL_DIST          512.0f /* wall-scan trace length */
#define Q3IDE_WALL_OFFSET          3.0f /* push off wall surface to avoid z-fighting */

/* ── Spawn / attach ─────────────────────────────────────────────── */

#define Q3IDE_SPAWN_WIN_W        100.0f /* world width of initial placed window */
#define Q3IDE_SPAWN_WIN_DIST     200.0f /* distance ahead of player eye for initial placement */
#define Q3IDE_DISPLAY_ASPECT  (16.0f / 9.0f)

/* ── Interaction / timing ───────────────────────────────────────── */

#define Q3IDE_DWELL_MS           150.0f /* ms crosshair must dwell on window to activate hover */
#define Q3IDE_POINTER_MAX_DIST  1000.0f /* max distance for Pointer Mode */
#define Q3IDE_EDGE_ZONE_UV        0.05f /* UV fraction treated as window edge zone */
#define Q3IDE_REPOSITION_MS        3000 /* ms window stays selected waiting for shoot-to-move destination */
#define Q3IDE_IDLE_TIMEOUT_MS      5000 /* ms without a frame before window is marked IDLE */

/* ── Effects ────────────────────────────────────────────────────── */

#define Q3IDE_SPLAT_LIFE_MS     700ULL /* ms blood-splat decal stays visible */
#define Q3IDE_HOVER_GLOW         1.15f /* color multiplier at full hover */
#define Q3IDE_HOVER_LIFT_Z        4.0f /* units toward player at full hover */
#define Q3IDE_HOVER_FADE_IN_SEC  0.15f /* hover animation duration */
#define Q3IDE_HOVER_BORDER_THICK  1.0f /* border quad thickness in world units */

/* ── Entity scan ────────────────────────────────────────────────── */

#define Q3IDE_ENT_MAX_DIST       512.0f /* max distance for entity name hover */
#define Q3IDE_ENT_COS_CONE      0.9976f /* cos(4°) — crosshair cone half-angle */

/* ── Overlay HUD (left monitor keybinding panel) ────────────────── */

#define Q3IDE_OVL_CHAR_W          0.14f /* bigchars character width in world units */
#define Q3IDE_OVL_CHAR_H          0.20f /* bigchars character height */
#define Q3IDE_OVL_LINE_H          0.30f /* line pitch */
#define Q3IDE_OVL_GAP             0.22f /* gap between key column and label column */
#define Q3IDE_OVL_KEY_W           0.56f /* fixed key column width (4 chars) */
#define Q3IDE_OVL_DIST            10.0f /* distance from camera — small so always in front of walls */
#define Q3IDE_OVL_SMALL_SCALE      0.6f /* small text variant scale factor */

/* ── Teleport history ───────────────────────────────────────────── */

#define Q3IDE_TELE_HIST             20 /* ring buffer slots for teleport position history */

/* ── Post-MVP: animations ───────────────────────────────────────── */

#define Q3IDE_ANIM_SPAWN_SEC      0.35f
#define Q3IDE_ANIM_CLOSE_SEC      0.25f
#define Q3IDE_ANIM_GRAB_SCALE     1.02f
#define Q3IDE_ANIM_LERP_SPEED     8.0f

/* ── Post-MVP: window chrome ────────────────────────────────────── */

#define Q3IDE_CORNER_RADIUS       12.0f
#define Q3IDE_WINDOW_BAR_HEIGHT   32.0f
#define Q3IDE_WINDOW_BAR_OVERLAP  20.0f
#define Q3IDE_ORNAMENT_Z_OFFSET    8.0f
#define Q3IDE_ORNAMENT_OVERLAP    20.0f
#define Q3IDE_MIN_TAP_TARGET      60.0f

/* ── Post-MVP: status overlay tints ────────────────────────────── */

#define Q3IDE_STATUS_PASS_R  0.2f
#define Q3IDE_STATUS_PASS_G  0.9f
#define Q3IDE_STATUS_PASS_B  0.4f
#define Q3IDE_STATUS_PASS_A  0.15f
#define Q3IDE_STATUS_FAIL_R  1.0f
#define Q3IDE_STATUS_FAIL_G  0.3f
#define Q3IDE_STATUS_FAIL_B  0.3f
#define Q3IDE_STATUS_FAIL_A  0.15f
#define Q3IDE_STATUS_BUILD_R 1.0f
#define Q3IDE_STATUS_BUILD_G 0.8f
#define Q3IDE_STATUS_BUILD_B 0.2f
#define Q3IDE_STATUS_BUILD_A 0.15f

/* ── Runtime params (accent colour, visual sizes) ───────────────────
 * Defined once in q3ide_laser.c; read everywhere else as const.   */
typedef struct {
	/* Laser pointer ribbon width in world units (~2 px at typical distance). */
	float laserPointerWidth;
	/* Border frame width in world units — applied to all hover/select borders. */
	float borderThickness;
	/* TV chassis depth in world units — edge quads span this around the screen perimeter. */
	float windowDepth;
	/* Accent colour (RGB) used for borders, blood splat and scratch slot 63 texture.
	 * "Quake red": deep crimson matching the Q3 HUD palette. */
	byte accentColor[3]; /* {R, G, B} */
} q3ide_params_t;

/* Singleton — defined in q3ide_laser.c, read-only everywhere else. */
extern const q3ide_params_t q3ide_params;
