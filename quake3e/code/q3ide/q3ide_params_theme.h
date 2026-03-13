/*
 * q3ide_params_theme.h — All UI colors used in the overlay system.
 * Single source of truth for every RGB triplet.
 *
 * Format: R, G, B  (each 0-255, used directly as function arguments)
 * Usage:  q3ide_ovl_str(x, y, z, rx, ux, s, Q3IDE_CLR_HUD_AMBER);
 */

#pragma once

/* ── HUD / notifications ─────────────────────────────────────────── */

#define Q3IDE_CLR_HUD_AMBER    255, 220, 80  /* bright amber    — main HUD message banner (centered upper screen) */
#define Q3IDE_CLR_NOTIF_PAUSED 255, 170, 30  /* orange          — PAUSED banner in left overlay */
#define Q3IDE_CLR_NOTIF_HIDDEN 120, 120, 255 /* periwinkle blue — HIDDEN banner in left overlay */

/* ── Window list header ──────────────────────────────────────────── */

#define Q3IDE_CLR_HEADER_DEAD      255, 50, 50 /* red    — DEAD count when > 0 */
#define Q3IDE_CLR_HEADER_THROTTLED 255, 200, 0 /* yellow — THROTTLED count when > 0 */

/* ── Window labels ───────────────────────────────────────────────── */

#define Q3IDE_CLR_WINLABEL_HOVER 255, 220, 0   /* yellow      — hovered/aimed window name */
#define Q3IDE_CLR_WINNAME_DEAD   255, 80, 40   /* orange red  — window name when stream is dead */
#define Q3IDE_CLR_WINNAME_NORMAL 210, 210, 210 /* light gray  — window name in normal state */

/* ── Lamp states ─────────────────────────────────────────────────── */

#define Q3IDE_CLR_LAMP_OK      40, 200, 70 /* green       — lamp: no failure ever / not failing now */
#define Q3IDE_CLR_LAMP_FAILED  255, 50, 50 /* red         — lamp: ever_failed = true */
#define Q3IDE_CLR_LAMP_FAILING 255, 30, 30 /* deep red    — lamp: failing right now */
#define Q3IDE_CLR_LAMP_ACTIVE  40, 220, 80 /* bright green — lamp: stream active and delivering frames */
#define Q3IDE_CLR_LAMP_IDLE    255, 200, 0 /* yellow      — lamp: stream alive but no recent frames */
#define Q3IDE_CLR_LAMP_DEAD    255, 30, 30 /* deep red    — lamp: stream completely dead */

/* ── Area / legend labels ────────────────────────────────────────── */

#define Q3IDE_CLR_AREA_LABEL  100, 200, 255 /* sky blue    — AAS area/cluster info text */
#define Q3IDE_CLR_ALERT_RED   255, 50, 50   /* red         — stream failure alert line */
#define Q3IDE_CLR_LEGEND_GRAY 120, 120, 120 /* gray        — lamp legend annotation text */

/* ── Map switcher menu ────────────────────────────────────────────── */

#define Q3IDE_CLR_MENU_SEL  80, 255, 80   /* bright green — selected entry */
#define Q3IDE_CLR_MENU_ITEM 255, 255, 255 /* white        — unselected map/skin entry */
#define Q3IDE_CLR_MENU_CAT  220, 60, 60   /* red          — unselected category entry */

/* ── Keyboard grid ───────────────────────────────────────────────── */

#define Q3IDE_CLR_KEY_Q3IDE       220, 220, 220 /* white       — key bound to a q3ide handler */
#define Q3IDE_CLR_KEY_QUAKE       190, 155, 25  /* dark amber  — key bound to a Quake handler */
#define Q3IDE_CLR_KEY_COLLISION   255, 30, 30   /* deep red    — key bound to both (conflict) */
#define Q3IDE_CLR_KEY_PASSTHROUGH 60, 200, 120  /* mint green  — key passed through to game */
#define Q3IDE_CLR_KEY_DEFAULT     155, 155, 155 /* gray        — key with no handler */
#define Q3IDE_CLR_KEY_LABEL       230, 230, 230 /* off-white   — key label text */
#define Q3IDE_CLR_KEY_LINE        130, 130, 130 /* medium gray — connector lines between keys */
