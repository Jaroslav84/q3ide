/*
 * q3ide_win_mngr_internal.h — private types shared between q3ide_win_mngr.c and q3ide_commands.c.
 * Not included by engine source files.
 */

#ifndef Q3IDE_WM_INTERNAL_H
#define Q3IDE_WM_INTERNAL_H

#include "../qcommon/q_shared.h"
#include "q3ide_params.h"
#include <pthread.h>

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
typedef int (*q3ide_fn_start_cap)(Q3ideCapture *, unsigned int, int); /* fps: -1=off, 0=static, N=cap */
typedef void (*q3ide_fn_stop_cap)(Q3ideCapture *, unsigned int);
typedef Q3ideFrame (*q3ide_fn_get_frame)(Q3ideCapture *, unsigned int);
typedef Q3ideDisplayList (*q3ide_fn_list_disp)(Q3ideCapture *);
typedef void (*q3ide_fn_free_dlist)(Q3ideDisplayList);
typedef int (*q3ide_fn_start_disp)(Q3ideCapture *, unsigned int, int); /* fps: -1=off, 0=static, N=cap */
/* Batch 2: Input injection (optional — gracefully absent if dylib lacks them) */
typedef void (*q3ide_fn_inject_click)(Q3ideCapture *, unsigned int, float, float);
typedef void (*q3ide_fn_inject_key)(Q3ideCapture *, unsigned int, int, int);

/* ── Window change detection ──────────────────────────────────── */
typedef struct {
	unsigned int window_id;
	char *app_name; /* owned by lib (added events only; NULL for removed) */
	unsigned int width, height;
	int is_added; /* 1=added, 0=removed, 2=resized */
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
	unsigned long long last_upload_ms; /* Sys_Milliseconds() of last texture upload */
	/* Batch 2: Interaction */
	int hover_active; /* 1 = crosshair dwelling on this window */
	float hover_t;    /* 0..1 hover animation progress */
	char label[128];  /* display name: window title or "Display N" */
	/* Hit effect: blood splat on window surface */
	unsigned long long hit_time_ms; /* Sys_Milliseconds() at last bullet impact; 0=none */
	vec3_t hit_pos;                 /* world position of hit point */
	qboolean wall_mounted; /* qtrue = placed by auto-layout engine; qfalse = user-manual or floating */
	qboolean los_visible;  /* cached LOS result — updated once per frame, reused across monitor passes */
	/* Tunnel: OS screen-capture window. detach-all only removes these.
	 * Non-tunnel windows (HUD, FPS, overlays) survive detach-all. */
	qboolean is_tunnel;
	/* Display slice: UV crop */
	float uv_x0, uv_x1;  /* horizontal UV crop [0..1], default 0.0/1.0 */
	qboolean owns_stream;                /* qtrue = calls cap_stop on detach */
	qboolean stream_active;              /* qtrue = SCK stream currently delivering frames */
	qboolean ever_failed;                /* qtrue = stream has ever been throttled or died; never resets */
	unsigned long long last_throttle_ms; /* Sys_Milliseconds() last time Apple gave no frame for >1s */
} q3ide_win_t;

/* ── Global window manager state (defined in q3ide_win_mngr.c) ──────── */
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
	int frame_uploads; /* texture uploads since last heartbeat */
	byte *fbuf;
	int fbuf_size;
	qhandle_t border_shader; /* scratch slot 63: solid red — hover/select/splat */
	qhandle_t edge_shader;   /* scratch slot 62: solid black — TV chassis edge quads */
	/* Background poll thread — fetches SCK change list off the main thread */
	pthread_t poll_thread;
	pthread_mutex_t poll_mutex;
	volatile int poll_running;
	Q3ideWindowChangeList poll_pending; /* protected by poll_mutex */
	qboolean poll_has_pending;          /* protected by poll_mutex */
} q3ide_wm_t;

extern q3ide_wm_t q3ide_wm;
#define q3ide_win_mngr q3ide_wm /* alias for renamed references */

#endif /* Q3IDE_WM_INTERNAL_H */
