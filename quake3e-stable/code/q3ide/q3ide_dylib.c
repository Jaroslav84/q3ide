/*
 * q3ide_dylib.c — Dylib load/unload, poll thread, WM init/shutdown.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"

#include "../qcommon/qcommon.h"
#include <dlfcn.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#define Q3IDE_DYLIB "libq3ide_capture.dylib"
#else
#define Q3IDE_DYLIB "libq3ide_capture.so"
#endif

extern q3ide_wm_t q3ide_wm;

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
	SYM(cap_raise_win, "q3ide_raise_window");
	SYM(cap_unminimize_all, "q3ide_unminimize_all_and_focus");
	SYM(cap_pause_streams, "q3ide_pause_all_streams");
	SYM(cap_resume_streams, "q3ide_resume_all_streams");
#undef SYM

	if (!q3ide_wm.cap_init || !q3ide_wm.cap_shutdown || !q3ide_wm.cap_get_frame) {
		Q3IDE_LOGE("missing dylib symbols");
		Q3IDE_Event("dylib_failed", "\"reason\":\"missing_symbols\"");
		dlclose(dl);
		q3ide_wm.dylib = NULL;
		return qfalse;
	}
	// clang-format off
	Q3IDE_LOGI("dylib loaded (inject_click=%s inject_key=%s poll_changes=%s raise_win=%s unminimize_all=%s pause_streams=%s)",
	           q3ide_wm.cap_inject_click   ? "yes" : "no",
	           q3ide_wm.cap_inject_key     ? "yes" : "no",
	           q3ide_wm.cap_poll_changes   ? "yes" : "no",
	           q3ide_wm.cap_raise_win      ? "yes" : "no",
	           q3ide_wm.cap_unminimize_all ? "yes" : "no",
	           q3ide_wm.cap_pause_streams  ? "yes" : "no");
	// clang-format on
	Q3IDE_Event("dylib_loaded", "");
	return qtrue;
}

/* Background thread: fetch SCK change list every 500ms, store for main thread drain.
 * Runs whenever there are active windows — position changes on composite windows
 * are always detected (Calendar drag, etc.). */
static void *q3ide_poll_thread_fn(void *arg)
{
	(void) arg;
	while (q3ide_wm.poll_running) {
		struct timespec ts = {0, 500000000}; /* 500ms */
		nanosleep(&ts, NULL);
		if (!q3ide_wm.poll_running)
			continue;
		if (!q3ide_wm.cap_poll_changes || !q3ide_wm.cap)
			continue;
		if (q3ide_wm.num_active == 0)
			continue; /* nothing to watch */
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
	/* Cache total macOS window count for the "A" slot in the Windows header.
	 * Apply same junk + size filters used when enqueueing windows. */
	if (q3ide_wm.cap_list_wins && q3ide_wm.cap_free_wlist) {
		Q3ideWindowList wl = q3ide_wm.cap_list_wins(q3ide_wm.cap);
		unsigned int k;
		q3ide_wm.macos_win_count = 0;
		for (k = 0; k < wl.count; k++) {
			const Q3ideWindowInfo *wi = &wl.windows[k];
			if ((int) wi->width < Q3IDE_MIN_WIN_W || (int) wi->height < Q3IDE_MIN_WIN_H)
				continue;
			if (Q3IDE_IsJunkAppName(wi->app_name))
				continue;
			q3ide_wm.macos_win_count++;
		}
		q3ide_wm.cap_free_wlist(wl);
	}
	/* Unminimize all apps + focus Quake before first attach */
	if (q3ide_wm.cap_unminimize_all)
		q3ide_wm.cap_unminimize_all(q3ide_wm.cap);
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
