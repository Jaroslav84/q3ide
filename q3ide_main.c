/*
 * q3ide_main.c — Main integration point for q3ide.
 *
 * Orchestrates:
 *   - Loading the capture dylib at engine startup
 *   - Registering console commands (/q3ide_*)
 *   - Per-frame texture updates from captured windows
 *   - Window placement on spawn
 *   - Shutdown and cleanup
 *
 * This file calls the engine adapter (engine/adapter.h) and
 * the capture dylib (q3ide_capture.h). It never calls engine
 * internals directly.
 */

#include "engine/adapter.h"
#include "q3ide_design.h"
#include "spatial/panel.h"
#include "spatial/placement.h"

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

/* ─── Capture dylib function pointers ─────────────────────────────────── */
/* Loaded at runtime via dlopen. These match q3ide_capture.h. */

/* Opaque capture handle */
typedef struct Q3ideCapture Q3ideCapture;

/* Frame data from capture dylib */
typedef struct {
	const unsigned char *pixels;
	unsigned int         width;
	unsigned int         height;
	unsigned int         stride;
	unsigned long long   timestamp_ns;
} Q3ideFrame;

/* Function pointer types */
typedef Q3ideCapture *(*q3ide_init_fn)(void);
typedef void          (*q3ide_shutdown_fn)(Q3ideCapture *);
typedef char *        (*q3ide_list_windows_formatted_fn)(Q3ideCapture *);
typedef void          (*q3ide_free_string_fn)(char *);
typedef unsigned int  (*q3ide_attach_by_title_fn)(Q3ideCapture *, const char *, unsigned int);
typedef void          (*q3ide_stop_capture_fn)(Q3ideCapture *, unsigned int);
typedef Q3ideFrame    (*q3ide_get_frame_fn)(Q3ideCapture *, unsigned int);

/* ─── Global state ────────────────────────────────────────────────────── */

static struct {
	/* Dylib handle */
	void                           *dylib;

	/* Function pointers */
	q3ide_init_fn                   cap_init;
	q3ide_shutdown_fn               cap_shutdown;
	q3ide_list_windows_formatted_fn cap_list;
	q3ide_free_string_fn            cap_free_string;
	q3ide_attach_by_title_fn        cap_attach;
	q3ide_stop_capture_fn           cap_stop;
	q3ide_get_frame_fn              cap_get_frame;

	/* Capture context */
	Q3ideCapture                   *capture;

	/* Windows */
	q3ide_window_t                  windows[Q3IDE_MAX_WINDOWS];
	int                             active_window_count;

	/* Initialization flag */
	int                             initialized;
} q3ide;

/* ─── Dylib loading ───────────────────────────────────────────────────── */

#define Q3IDE_DYLIB_NAME "libq3ide_capture.dylib"

static int q3ide_load_dylib(void)
{
	q3ide.dylib = dlopen(Q3IDE_DYLIB_NAME, RTLD_LAZY);
	if (!q3ide.dylib) {
		/* Try relative path from engine binary */
		q3ide.dylib = dlopen("./" Q3IDE_DYLIB_NAME, RTLD_LAZY);
	}
	if (!q3ide.dylib) {
		Q3IDE_CONSOLE_PRINT("q3ide: failed to load " Q3IDE_DYLIB_NAME "\n");
		return 0;
	}

	/* Resolve symbols */
	#define LOAD_SYM(name, type) \
		q3ide.name = (type)dlsym(q3ide.dylib, #name); \
		if (!q3ide.name) { \
			Q3IDE_CONSOLE_PRINT("q3ide: missing symbol: " #name "\n"); \
			dlclose(q3ide.dylib); \
			q3ide.dylib = NULL; \
			return 0; \
		}

	LOAD_SYM(cap_init,        q3ide_init_fn);

	/* The dylib exports q3ide_* names, not cap_* */
	q3ide.cap_init      = (q3ide_init_fn)dlsym(q3ide.dylib, "q3ide_init");
	q3ide.cap_shutdown   = (q3ide_shutdown_fn)dlsym(q3ide.dylib, "q3ide_shutdown");
	q3ide.cap_list       = (q3ide_list_windows_formatted_fn)dlsym(q3ide.dylib, "q3ide_list_windows_formatted");
	q3ide.cap_free_string = (q3ide_free_string_fn)dlsym(q3ide.dylib, "q3ide_free_string");
	q3ide.cap_attach     = (q3ide_attach_by_title_fn)dlsym(q3ide.dylib, "q3ide_attach_by_title");
	q3ide.cap_stop       = (q3ide_stop_capture_fn)dlsym(q3ide.dylib, "q3ide_stop_capture");
	q3ide.cap_get_frame  = (q3ide_get_frame_fn)dlsym(q3ide.dylib, "q3ide_get_frame");

	if (!q3ide.cap_init || !q3ide.cap_shutdown || !q3ide.cap_get_frame) {
		Q3IDE_CONSOLE_PRINT("q3ide: missing required dylib symbols\n");
		dlclose(q3ide.dylib);
		q3ide.dylib = NULL;
		return 0;
	}

	#undef LOAD_SYM
	return 1;
}

/* ─── Console commands ────────────────────────────────────────────────── */

static void cmd_q3ide_list(void)
{
	char *list;

	if (!q3ide.capture || !q3ide.cap_list) {
		Q3IDE_CONSOLE_PRINT("q3ide: not initialized\n");
		return;
	}

	list = q3ide.cap_list(q3ide.capture);
	if (list) {
		Q3IDE_CONSOLE_PRINT(list);
		q3ide.cap_free_string(list);
	} else {
		Q3IDE_CONSOLE_PRINT("q3ide: failed to list windows\n");
	}
}

static void cmd_q3ide_attach(void)
{
	/* TODO: Parse command arguments to get title_query.
	 * In Quake3e this would be via Cmd_Argv(1).
	 * For now, demonstrate the flow with a placeholder. */
	const char *title_query = "iTerm";  /* TODO: Cmd_Argv(1) */
	unsigned int wid;
	int i;
	q3ide_window_t *win;

	if (!q3ide.capture || !q3ide.cap_attach) {
		Q3IDE_CONSOLE_PRINT("q3ide: not initialized\n");
		return;
	}

	wid = q3ide.cap_attach(q3ide.capture, title_query, Q3IDE_CAPTURE_TARGET_FPS);
	if (wid == 0) {
		Q3IDE_CONSOLE_PRINT("q3ide: no window found matching query\n");
		return;
	}

	/* Find free window slot */
	for (i = 0; i < Q3IDE_MAX_WINDOWS; i++) {
		if (!q3ide.windows[i].active) {
			break;
		}
	}
	if (i >= Q3IDE_MAX_WINDOWS) {
		Q3IDE_CONSOLE_PRINT("q3ide: max windows reached\n");
		q3ide.cap_stop(q3ide.capture, wid);
		return;
	}

	win = &q3ide.windows[i];
	memset(win, 0, sizeof(*win));
	win->active = 1;
	win->capture_window_id = wid;
	win->state = Q3IDE_WINDOW_STATE_ACTIVE;
	win->placement = Q3IDE_PLACEMENT_WALL;

	/* Place on nearest wall */
	if (q3ide_calc_wall_placement(win)) {
		/* Create engine texture */
		win->tex_id = Q3IDE_TEXTURE_CREATE(
			(int)win->width, (int)win->height, Q3IDE_FORMAT_BGRA8);

		/* Create engine surface */
		win->surface_id = q3ide_adapter->surface_create(
			win->origin[0], win->origin[1], win->origin[2],
			win->width, win->height, win->normal);

		if (win->tex_id && win->surface_id) {
			q3ide_adapter->surface_set_texture(win->surface_id, win->tex_id);
			q3ide.active_window_count++;
			Q3IDE_CONSOLE_PRINT("q3ide: window attached\n");
		} else {
			Q3IDE_CONSOLE_PRINT("q3ide: failed to create texture/surface\n");
			win->active = 0;
		}
	} else {
		Q3IDE_CONSOLE_PRINT("q3ide: no wall found for placement\n");
		win->active = 0;
		q3ide.cap_stop(q3ide.capture, wid);
	}
}

static void cmd_q3ide_detach(void)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WINDOWS; i++) {
		q3ide_window_t *win = &q3ide.windows[i];
		if (!win->active) {
			continue;
		}

		/* Stop capture */
		if (q3ide.cap_stop) {
			q3ide.cap_stop(q3ide.capture, win->capture_window_id);
		}

		/* Destroy engine resources */
		if (win->surface_id) {
			q3ide_adapter->surface_destroy(win->surface_id);
		}
		if (win->tex_id) {
			Q3IDE_TEXTURE_DESTROY(win->tex_id);
		}

		memset(win, 0, sizeof(*win));
		q3ide.active_window_count--;
	}

	Q3IDE_CONSOLE_PRINT("q3ide: all windows detached\n");
}

static void cmd_q3ide_status(void)
{
	char buf[512];
	int i;

	snprintf(buf, sizeof(buf),
	         "q3ide status: %d active window(s)\n",
	         q3ide.active_window_count);
	Q3IDE_CONSOLE_PRINT(buf);

	for (i = 0; i < Q3IDE_MAX_WINDOWS; i++) {
		q3ide_window_t *win = &q3ide.windows[i];
		if (!win->active) {
			continue;
		}

		snprintf(buf, sizeof(buf),
		         "  [%d] wid=%u tex=%dx%d frames=%llu\n",
		         i, win->capture_window_id,
		         win->tex_width, win->tex_height,
		         win->frames_received);
		Q3IDE_CONSOLE_PRINT(buf);
	}
}

/* ─── Per-frame update ────────────────────────────────────────────────── */

void q3ide_frame_update(float delta_time)
{
	int i;

	if (!q3ide.initialized || !q3ide.capture || !q3ide.cap_get_frame) {
		return;
	}

	for (i = 0; i < Q3IDE_MAX_WINDOWS; i++) {
		q3ide_window_t *win = &q3ide.windows[i];
		Q3ideFrame frame;

		if (!win->active || win->state != Q3IDE_WINDOW_STATE_ACTIVE) {
			continue;
		}

		/* Poll for new frame */
		frame = q3ide.cap_get_frame(q3ide.capture, win->capture_window_id);
		if (!frame.pixels) {
			continue;
		}

		/* Handle resolution change */
		if ((int)frame.width != win->tex_width ||
		    (int)frame.height != win->tex_height) {
			/* Recreate texture at new size */
			if (win->tex_id) {
				Q3IDE_TEXTURE_DESTROY(win->tex_id);
			}
			win->tex_id = Q3IDE_TEXTURE_CREATE(
				(int)frame.width, (int)frame.height, Q3IDE_FORMAT_BGRA8);
			win->tex_width = (int)frame.width;
			win->tex_height = (int)frame.height;

			if (win->surface_id) {
				q3ide_adapter->surface_set_texture(win->surface_id, win->tex_id);
			}

			/* Recalculate world-space dimensions */
			q3ide_calc_window_size(win->tex_width, win->tex_height,
			                      &win->width, &win->height);
		}

		/* Upload frame to GPU */
		Q3IDE_TEXTURE_UPLOAD(win->tex_id, frame.pixels,
		                     (int)frame.width, (int)frame.height,
		                     (int)frame.stride);

		win->frames_received++;
		win->last_frame_timestamp_ns = frame.timestamp_ns;
	}

	(void)delta_time;
}

/* ─── Init / Shutdown ─────────────────────────────────────────────────── */

int q3ide_main_init(void)
{
	memset(&q3ide, 0, sizeof(q3ide));

	/* Load capture dylib */
	if (!q3ide_load_dylib()) {
		return 0;
	}

	/* Initialize capture */
	q3ide.capture = q3ide.cap_init();
	if (!q3ide.capture) {
		Q3IDE_CONSOLE_PRINT("q3ide: capture init failed\n");
		dlclose(q3ide.dylib);
		q3ide.dylib = NULL;
		return 0;
	}

	/* Register console commands */
	q3ide_adapter->cmd_register("q3ide_list", cmd_q3ide_list);
	q3ide_adapter->cmd_register("q3ide_attach", cmd_q3ide_attach);
	q3ide_adapter->cmd_register("q3ide_detach", cmd_q3ide_detach);
	q3ide_adapter->cmd_register("q3ide_status", cmd_q3ide_status);

	q3ide.initialized = 1;
	Q3IDE_CONSOLE_PRINT("q3ide: initialized\n");
	return 1;
}

void q3ide_main_shutdown(void)
{
	/* Detach all windows */
	cmd_q3ide_detach();

	/* Shutdown capture */
	if (q3ide.capture && q3ide.cap_shutdown) {
		q3ide.cap_shutdown(q3ide.capture);
		q3ide.capture = NULL;
	}

	/* Unload dylib */
	if (q3ide.dylib) {
		dlclose(q3ide.dylib);
		q3ide.dylib = NULL;
	}

	q3ide.initialized = 0;
	Q3IDE_CONSOLE_PRINT("q3ide: shut down\n");
}
