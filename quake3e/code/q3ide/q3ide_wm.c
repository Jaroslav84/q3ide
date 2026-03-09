/*
 * q3ide_wm.c — Window manager: dylib load, attach, move, find, init/shutdown.
 * Frame polling: q3ide_poll.c.  Scene rendering: q3ide_scene.c.  Mirror: q3ide_mirror.c.
 */

#include "q3ide_wm.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "q3ide_design.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <dlfcn.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#define Q3IDE_DYLIB "libq3ide_capture.dylib"
#else
#define Q3IDE_DYLIB "libq3ide_capture.so"
#endif

#define Q3IDE_WALL_DIST   512.0f
#define Q3IDE_WALL_OFFSET 3.0f

q3ide_wm_t q3ide_wm;

/* Geometry clamp — q3ide_geom_clamp.c */
extern void q3ide_clamp_window_size(q3ide_win_t *win);

static qboolean q3ide_load_dylib(void)
{
	void *dl = dlopen(Q3IDE_DYLIB, RTLD_LAZY);
	if (!dl)
		dl = dlopen("./" Q3IDE_DYLIB, RTLD_LAZY);
	if (!dl) {
		Com_Printf("q3ide: cannot load dylib\n");
		return qfalse;
	}
	q3ide_wm.dylib = dl;

#define SYM(f, n) q3ide_wm.f = dlsym(dl, n)
	SYM(cap_init, "q3ide_init");
	SYM(cap_shutdown, "q3ide_shutdown");
	SYM(cap_list_fmt, "q3ide_list_windows_formatted");
	SYM(cap_free_str, "q3ide_free_string");
	SYM(cap_list_wins, "q3ide_list_windows");
	SYM(cap_free_wlist, "q3ide_free_window_list");
	SYM(cap_start, "q3ide_start_capture");
	SYM(cap_stop, "q3ide_stop_capture");
	SYM(cap_get_frame, "q3ide_get_frame");
	SYM(cap_list_disp, "q3ide_list_displays");
	SYM(cap_free_dlist, "q3ide_free_display_list");
	SYM(cap_start_disp, "q3ide_start_display_capture");
	SYM(cap_inject_click, "q3ide_inject_click");
	SYM(cap_inject_key, "q3ide_inject_key");
	SYM(cap_poll_changes, "q3ide_poll_window_changes");
	SYM(cap_free_changes, "q3ide_free_change_list");
#undef SYM

	if (!q3ide_wm.cap_init || !q3ide_wm.cap_shutdown || !q3ide_wm.cap_get_frame) {
		Q3IDE_LOGE("missing dylib symbols");
		Q3IDE_Event("dylib_failed", "\"reason\":\"missing_symbols\"");
		dlclose(dl);
		q3ide_wm.dylib = NULL;
		return qfalse;
	}
	// clang-format off
	Q3IDE_LOGI("dylib loaded (inject_click=%s inject_key=%s poll_changes=%s)",
	           q3ide_wm.cap_inject_click ? "yes" : "no",
	           q3ide_wm.cap_inject_key   ? "yes" : "no",
	           q3ide_wm.cap_poll_changes ? "yes" : "no");
	// clang-format on
	Q3IDE_Event("dylib_loaded", "");
	return qtrue;
}

/* Q3IDE_WM_TraceWall — q3ide_geom_clamp.c */

qboolean Q3IDE_WM_Attach(unsigned int id, vec3_t origin, vec3_t normal, float ww, float wh, qboolean do_start,
                         qboolean skip_clamp)
{
	int i, slot;
	q3ide_win_t *win;
	float len;

	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == id)
			return qfalse;

	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (!q3ide_wm.wins[i].active)
			break;
	if (i >= Q3IDE_MAX_WIN) {
		Com_Printf("q3ide: max windows\n");
		return qfalse;
	}

	for (slot = 0; slot < Q3IDE_MAX_WIN; slot++)
		if (!(q3ide_wm.slot_mask & (1ULL << slot)))
			break;
	if (slot >= Q3IDE_MAX_WIN) {
		Com_Printf("q3ide: no scratch slots\n");
		return qfalse;
	}

	if (do_start && q3ide_wm.cap_start && q3ide_wm.cap_start(q3ide_wm.cap, id, Q3IDE_CAPTURE_FPS) != 0) {
		Com_Printf("q3ide: capture start failed id=%u\n", id);
		return qfalse;
	}

	q3ide_wm.slot_mask |= (1ULL << slot);
	win = &q3ide_wm.wins[i];
	memset(win, 0, sizeof(*win));
	win->active = qtrue;
	win->capture_id = id;
	win->scratch_slot = slot;
	VectorCopy(origin, win->origin);
	VectorCopy(normal, win->normal);
	win->normal[2] = 0.0f;
	len = sqrtf(win->normal[0] * win->normal[0] + win->normal[1] * win->normal[1]);
	if (len > 0.001f) {
		win->normal[0] /= len;
		win->normal[1] /= len;
	}
	win->world_w = ww;
	win->world_h = wh;
	win->wall_mounted = skip_clamp;
	win->is_tunnel = do_start; /* OS screen-capture windows are tunnels */
	q3ide_wm.num_active++;
	if (!skip_clamp)
		q3ide_clamp_window_size(win);
	return qtrue;
}

void Q3IDE_WM_MoveWindow(int idx, vec3_t origin, vec3_t normal, qboolean skip_clamp)
{
	float len;
	if (idx < 0 || idx >= Q3IDE_MAX_WIN || !q3ide_wm.wins[idx].active)
		return;
	VectorCopy(origin, q3ide_wm.wins[idx].origin);
	VectorCopy(normal, q3ide_wm.wins[idx].normal);
	q3ide_wm.wins[idx].normal[2] = 0.0f;
	len = sqrtf(q3ide_wm.wins[idx].normal[0] * q3ide_wm.wins[idx].normal[0] +
	            q3ide_wm.wins[idx].normal[1] * q3ide_wm.wins[idx].normal[1]);
	if (len > 0.001f) {
		q3ide_wm.wins[idx].normal[0] /= len;
		q3ide_wm.wins[idx].normal[1] /= len;
	}
	if (!skip_clamp)
		q3ide_clamp_window_size(&q3ide_wm.wins[idx]);
}

int Q3IDE_WM_FindById(unsigned int cid)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == cid)
			return i;
	return -1;
}

/* Background thread: fetch SCK change list every 1s, store for main thread drain. */
static void *q3ide_poll_thread_fn(void *arg)
{
	(void) arg;
	while (q3ide_wm.poll_running) {
		struct timespec ts = {1, 0};
		nanosleep(&ts, NULL);
		if (!q3ide_wm.poll_running || !q3ide_wm.auto_attach)
			continue;
		if (!q3ide_wm.cap_poll_changes || !q3ide_wm.cap)
			continue;
		{
			Q3ideWindowChangeList clist = q3ide_wm.cap_poll_changes(q3ide_wm.cap);
			if (clist.changes && clist.count) {
				pthread_mutex_lock(&q3ide_wm.poll_mutex);
				if (q3ide_wm.poll_has_pending && q3ide_wm.cap_free_changes)
					q3ide_wm.cap_free_changes(q3ide_wm.poll_pending);
				q3ide_wm.poll_pending = clist;
				q3ide_wm.poll_has_pending = qtrue;
				pthread_mutex_unlock(&q3ide_wm.poll_mutex);
			}
		}
	}
	return NULL;
}

qboolean Q3IDE_WM_Init(void)
{
	memset(&q3ide_wm, 0, sizeof(q3ide_wm));
	if (!q3ide_load_dylib())
		return qfalse;
	q3ide_wm.cap = q3ide_wm.cap_init();
	if (!q3ide_wm.cap) {
		Com_Printf("q3ide: capture init failed\n");
		dlclose(q3ide_wm.dylib);
		q3ide_wm.dylib = NULL;
		return qfalse;
	}
	pthread_mutex_init(&q3ide_wm.poll_mutex, NULL);
	q3ide_wm.poll_running = 1;
	pthread_create(&q3ide_wm.poll_thread, NULL, q3ide_poll_thread_fn, NULL);
	return qtrue;
}

void Q3IDE_WM_Shutdown(void)
{
	/* Stop poll thread before cap_shutdown — thread uses cap */
	q3ide_wm.poll_running = 0;
	pthread_join(q3ide_wm.poll_thread, NULL);
	if (q3ide_wm.poll_has_pending && q3ide_wm.cap_free_changes)
		q3ide_wm.cap_free_changes(q3ide_wm.poll_pending);
	pthread_mutex_destroy(&q3ide_wm.poll_mutex);

	Q3IDE_WM_CmdDetachAll();
	if (q3ide_wm.cap && q3ide_wm.cap_shutdown)
		q3ide_wm.cap_shutdown(q3ide_wm.cap);
	if (q3ide_wm.dylib)
		dlclose(q3ide_wm.dylib);
	if (q3ide_wm.fbuf)
		Z_Free(q3ide_wm.fbuf);
	memset(&q3ide_wm, 0, sizeof(q3ide_wm));
}
