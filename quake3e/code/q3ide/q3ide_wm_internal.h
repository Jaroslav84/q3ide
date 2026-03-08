/*
 * q3ide_wm_internal.h — private types shared between q3ide_wm.c and q3ide_cmd.c.
 * Not included by engine source files.
 */

#ifndef Q3IDE_WM_INTERNAL_H
#define Q3IDE_WM_INTERNAL_H

#include "../qcommon/q_shared.h"

/* ── Constants ─────────────────────────────────────────────────── */
#define Q3IDE_MAX_WIN 16
#define Q3IDE_CAPTURE_FPS 60
#define Q3IDE_WIN_INCHES 150.0f
#define Q3IDE_MIN_WIN_W 400
#define Q3IDE_MIN_WIN_H 300

/* ── Dylib C-ABI types ─────────────────────────────────────────── */

typedef struct Q3ideCapture Q3ideCapture;

typedef struct {
	const unsigned char *pixels;
	unsigned int width, height, stride;
	unsigned long long timestamp_ns;
	unsigned int source_wid;
} Q3ideFrame;

typedef struct {
	unsigned int window_id;
	const char *title, *app_name;
	unsigned int width, height;
	int is_on_screen;
} Q3ideWindowInfo;

typedef struct {
	const Q3ideWindowInfo *windows;
	unsigned int count;
} Q3ideWindowList;

typedef struct {
	unsigned int display_id, width, height;
	int x, y;
} Q3ideDisplayInfo;

typedef struct {
	const Q3ideDisplayInfo *displays;
	unsigned int count;
} Q3ideDisplayList;

typedef Q3ideCapture *(*q3ide_fn_init)(void);
typedef void (*q3ide_fn_shutdown)(Q3ideCapture *);
typedef char *(*q3ide_fn_list_fmt)(Q3ideCapture *);
typedef void (*q3ide_fn_free_str)(char *);
typedef Q3ideWindowList (*q3ide_fn_list_wins)(Q3ideCapture *);
typedef void (*q3ide_fn_free_wlist)(Q3ideWindowList);
typedef int (*q3ide_fn_start_cap)(Q3ideCapture *, unsigned int, unsigned int);
typedef void (*q3ide_fn_stop_cap)(Q3ideCapture *, unsigned int);
typedef Q3ideFrame (*q3ide_fn_get_frame)(Q3ideCapture *, unsigned int);
typedef Q3ideDisplayList (*q3ide_fn_list_disp)(Q3ideCapture *);
typedef void (*q3ide_fn_free_dlist)(Q3ideDisplayList);
typedef int (*q3ide_fn_start_disp)(Q3ideCapture *, unsigned int, unsigned int);
/* Batch 2: Input injection (optional — gracefully absent if dylib lacks them) */
typedef void (*q3ide_fn_inject_click)(Q3ideCapture *, unsigned int, float, float);
typedef void (*q3ide_fn_inject_key)(Q3ideCapture *, unsigned int, int, int);

/* ── Window change detection ──────────────────────────────────── */
typedef struct {
	unsigned int window_id;
	char *app_name; /* owned by lib (added events only; NULL for removed) */
	unsigned int width, height;
	int is_added; /* 1=added, 0=removed */
} Q3ideWindowChange;

typedef struct {
	Q3ideWindowChange *changes;
	unsigned int count;
} Q3ideWindowChangeList;

typedef Q3ideWindowChangeList (*q3ide_fn_poll_changes)(Q3ideCapture *);
typedef void (*q3ide_fn_free_changes)(Q3ideWindowChangeList);

/* ── Status tracking (Batch 1) ───────────────────────────────── */
typedef enum {
	Q3IDE_WIN_STATUS_INACTIVE = 0,
	Q3IDE_WIN_STATUS_ACTIVE,
	Q3IDE_WIN_STATUS_IDLE, /* no new frames for > 5s */
	Q3IDE_WIN_STATUS_ERROR,
} q3ide_win_status_t;

#define Q3IDE_DIST_FULL 200.0f /* full 60fps */
#define Q3IDE_DIST_HALF 500.0f /* 30fps */
#define Q3IDE_DIST_LOW 1000.0f /* 15fps */
#define Q3IDE_FPS_FULL 60
#define Q3IDE_FPS_HALF 30
#define Q3IDE_FPS_LOW 15
#define Q3IDE_FPS_MIN 5

/* ── Per-window state ─────────────────────────────────────────── */
typedef struct {
	qboolean active;
	unsigned int capture_id;
	int scratch_slot;
	int tex_w, tex_h;
	vec3_t origin, normal;
	float world_w, world_h;
	qhandle_t shader;
	unsigned long long frames;
	/* Batch 1: Status tracking */
	q3ide_win_status_t status;
	unsigned long long first_frame_ms; /* Sys_Milliseconds() on first frame */
	unsigned long long last_frame_ms;  /* Sys_Milliseconds() on last frame */
	float player_dist;                 /* distance from player, updated each frame */
	int fps_target;                    /* target FPS for this window (distance-based) */
	int skip_counter;                  /* frames skipped since last upload */
	/* Batch 2: Interaction */
	int hover_active; /* 1 = crosshair dwelling on this window */
	float hover_t;    /* 0..1 hover animation progress */
	/* Hit effect: blood splat on window surface */
	unsigned long long hit_time_ms; /* Sys_Milliseconds() at last bullet impact; 0=none */
	vec3_t hit_pos;                 /* world position of hit point */
} q3ide_win_t;

/* ── Global window manager state (defined in q3ide_wm.c) ──────── */
typedef struct {
	void *dylib;
	q3ide_fn_init cap_init;
	q3ide_fn_shutdown cap_shutdown;
	q3ide_fn_list_fmt cap_list_fmt;
	q3ide_fn_free_str cap_free_str;
	q3ide_fn_list_wins cap_list_wins;
	q3ide_fn_free_wlist cap_free_wlist;
	q3ide_fn_start_cap cap_start;
	q3ide_fn_stop_cap cap_stop;
	q3ide_fn_get_frame cap_get_frame;
	q3ide_fn_list_disp cap_list_disp;
	q3ide_fn_free_dlist cap_free_dlist;
	q3ide_fn_start_disp cap_start_disp;
	q3ide_fn_inject_click cap_inject_click; /* optional: NULL if dylib lacks symbol */
	q3ide_fn_inject_key cap_inject_key;     /* optional: NULL if dylib lacks symbol */
	q3ide_fn_poll_changes cap_poll_changes; /* optional: window open/close events */
	q3ide_fn_free_changes cap_free_changes; /* optional */
	Q3ideCapture *cap;
	vec3_t player_eye;               /* eye position, set each frame by UpdatePlayerPos */
	qboolean auto_attach;            /* true after "q3ide attach all" — auto-place new windows */
	unsigned long long last_scan_ms; /* last time we polled for changes */
	q3ide_win_t wins[Q3IDE_MAX_WIN];
	int num_active;
	unsigned int slot_mask; /* bit N = scratch slot N is in use */
	byte *fbuf;
	int fbuf_size;
	qhandle_t border_shader;  /* *white for hover border strips */
	qhandle_t portal_shader;  /* teleporter energy glow for first mirror */
	/* Standalone Q3 mirror portal (real recursive rendering, no capture) */
	qboolean  mirror_active;
	vec3_t    mirror_origin;
	vec3_t    mirror_normal;
	float     mirror_w, mirror_h;
	qhandle_t mirror_shader;        /* q3ide/mirror — sort portal */
	qhandle_t mirror_energy_shader; /* energy glow overlay */
} q3ide_wm_t;

extern q3ide_wm_t q3ide_wm;

#endif /* Q3IDE_WM_INTERNAL_H */
