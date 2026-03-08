/*
 * q3ide_wm_internal.h — private types shared between q3ide_wm.c and q3ide_cmd.c.
 * Not included by engine source files.
 */

#ifndef Q3IDE_WM_INTERNAL_H
#define Q3IDE_WM_INTERNAL_H

#include "../qcommon/q_shared.h"

/* ── Constants ─────────────────────────────────────────────────── */
#define Q3IDE_MAX_WIN      16
#define Q3IDE_CAPTURE_FPS  60
#define Q3IDE_WIN_INCHES   150.0f
#define Q3IDE_MIN_WIN_W    400
#define Q3IDE_MIN_WIN_H    300

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
	const char  *title, *app_name;
	unsigned int width, height;
	int          is_on_screen;
} Q3ideWindowInfo;

typedef struct { const Q3ideWindowInfo *windows; unsigned int count; } Q3ideWindowList;

typedef struct {
	unsigned int display_id, width, height;
	int x, y;
} Q3ideDisplayInfo;

typedef struct { const Q3ideDisplayInfo *displays; unsigned int count; } Q3ideDisplayList;

typedef Q3ideCapture    *(*q3ide_fn_init)(void);
typedef void             (*q3ide_fn_shutdown)(Q3ideCapture *);
typedef char            *(*q3ide_fn_list_fmt)(Q3ideCapture *);
typedef void             (*q3ide_fn_free_str)(char *);
typedef Q3ideWindowList  (*q3ide_fn_list_wins)(Q3ideCapture *);
typedef void             (*q3ide_fn_free_wlist)(Q3ideWindowList);
typedef int              (*q3ide_fn_start_cap)(Q3ideCapture *, unsigned int, unsigned int);
typedef void             (*q3ide_fn_stop_cap)(Q3ideCapture *, unsigned int);
typedef Q3ideFrame       (*q3ide_fn_get_frame)(Q3ideCapture *, unsigned int);
typedef Q3ideDisplayList (*q3ide_fn_list_disp)(Q3ideCapture *);
typedef void             (*q3ide_fn_free_dlist)(Q3ideDisplayList);
typedef int              (*q3ide_fn_start_disp)(Q3ideCapture *, unsigned int, unsigned int);

/* ── Per-window state ─────────────────────────────────────────── */
typedef struct {
	qboolean     active;
	unsigned int capture_id;
	int          scratch_slot;
	int          tex_w, tex_h;
	vec3_t       origin, normal;
	float        world_w, world_h;
	qhandle_t    shader;
	unsigned long long frames;
} q3ide_win_t;

/* ── Global window manager state (defined in q3ide_wm.c) ──────── */
typedef struct {
	void               *dylib;
	q3ide_fn_init       cap_init;
	q3ide_fn_shutdown   cap_shutdown;
	q3ide_fn_list_fmt   cap_list_fmt;
	q3ide_fn_free_str   cap_free_str;
	q3ide_fn_list_wins  cap_list_wins;
	q3ide_fn_free_wlist cap_free_wlist;
	q3ide_fn_start_cap  cap_start;
	q3ide_fn_stop_cap   cap_stop;
	q3ide_fn_get_frame  cap_get_frame;
	q3ide_fn_list_disp  cap_list_disp;
	q3ide_fn_free_dlist cap_free_dlist;
	q3ide_fn_start_disp cap_start_disp;
	Q3ideCapture       *cap;
	q3ide_win_t         wins[Q3IDE_MAX_WIN];
	int                 num_active;
	int                 next_slot;
	byte               *fbuf;
	int                 fbuf_size;
} q3ide_wm_t;

extern q3ide_wm_t q3ide_wm;

#endif /* Q3IDE_WM_INTERNAL_H */
