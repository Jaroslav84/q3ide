/*
 * q3ide_wm.c — Q3IDE Window Manager core.
 *
 * Dylib loading, frame upload to GPU scratch slots, poly rendering,
 * wall ray tracing, and the window attach/detach primitives.
 * Complex commands (attach all, desktop) live in q3ide_cmd.c.
 */

#include "q3ide_wm.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../../../q3ide_design.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <dlfcn.h>
#include <math.h>
#include <string.h>

#ifdef __APPLE__
#define Q3IDE_DYLIB "libq3ide_capture.dylib"
#else
#define Q3IDE_DYLIB "libq3ide_capture.so"
#endif

#define Q3IDE_WALL_DIST 512.0f
#define Q3IDE_WALL_OFFSET 2.0f

/* Global state — also accessed by q3ide_cmd.c via q3ide_wm_internal.h */
q3ide_wm_t q3ide_wm;

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
	SYM(cap_inject_click, "q3ide_inject_click");        /* optional */
	SYM(cap_inject_key, "q3ide_inject_key");            /* optional */
	SYM(cap_poll_changes, "q3ide_poll_window_changes"); /* optional */
	SYM(cap_free_changes, "q3ide_free_change_list");    /* optional */
#undef SYM

	if (!q3ide_wm.cap_init || !q3ide_wm.cap_shutdown || !q3ide_wm.cap_get_frame) {
		Q3IDE_LOGE("missing dylib symbols");
		Q3IDE_Event("dylib_failed", "\"reason\":\"missing_symbols\"");
		dlclose(dl);
		q3ide_wm.dylib = NULL;
		return qfalse;
	}
	// clang-format off
	Q3IDE_LOGI("dylib loaded ok (inject_click=%s inject_key=%s poll_changes=%s)",
	           q3ide_wm.cap_inject_click ? "yes" : "no",
	           q3ide_wm.cap_inject_key   ? "yes" : "no",
	           q3ide_wm.cap_poll_changes ? "yes" : "no");
	// clang-format on
	Q3IDE_Event("dylib_loaded", "");
	return qtrue;
}

qboolean Q3IDE_WM_TraceWall(vec3_t start, vec3_t dir, vec3_t out_pos, vec3_t out_normal)
{
	trace_t tr;
	vec3_t end, mins = {0, 0, 0}, maxs = {0, 0, 0};
	end[0] = start[0] + dir[0] * Q3IDE_WALL_DIST;
	end[1] = start[1] + dir[1] * Q3IDE_WALL_DIST;
	end[2] = start[2] + dir[2] * Q3IDE_WALL_DIST;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction >= 1.0f || tr.startsolid)
		return qfalse;
	out_pos[0] = tr.endpos[0] + tr.plane.normal[0] * Q3IDE_WALL_OFFSET;
	out_pos[1] = tr.endpos[1] + tr.plane.normal[1] * Q3IDE_WALL_OFFSET;
	out_pos[2] = tr.endpos[2] + tr.plane.normal[2] * Q3IDE_WALL_OFFSET;
	VectorCopy(tr.plane.normal, out_normal);
	return qtrue;
}

static void q3ide_upload_frame(q3ide_win_t *win, const Q3ideFrame *frame)
{
	int needed = (int) (frame->width * frame->height * 4);
	int row = (int) frame->width * 4;
	unsigned int r, c;

	if (!re.UploadCinematic)
		return;

	if (needed > q3ide_wm.fbuf_size) {
		if (q3ide_wm.fbuf)
			Z_Free(q3ide_wm.fbuf);
		q3ide_wm.fbuf = Z_Malloc(needed);
		q3ide_wm.fbuf_size = needed;
	}

	for (r = 0; r < frame->height; r++) {
		const unsigned char *s = frame->pixels + r * frame->stride;
		byte *d = q3ide_wm.fbuf + r * row;
		for (c = 0; c < frame->width; c++, s += 4, d += 4) {
			d[0] = s[2];
			d[1] = s[1];
			d[2] = s[0];
			d[3] = s[3]; /* BGRA→RGBA */
		}
	}

	re.UploadCinematic((int) frame->width, (int) frame->height, (int) frame->width, (int) frame->height, q3ide_wm.fbuf,
	                   win->scratch_slot, qtrue);
	win->tex_w = (int) frame->width;
	win->tex_h = (int) frame->height;

	if (win->frames == 0) {
		char name[32];
		Com_sprintf(name, sizeof(name), "q3ide/win%d", win->scratch_slot);
		win->shader = re.RegisterShader(name);
		Q3IDE_LOGI("win[%d] id=%u slot=%d shader=%d %dx%d", (int) (win - q3ide_wm.wins), win->capture_id,
		           win->scratch_slot, win->shader, frame->width, frame->height);
	}
}

/* Change detection — implemented in q3ide_cmd.c */
extern void Q3IDE_WM_PollChanges(void);

/* Geometry helpers — implemented in q3ide_geom.c */
extern void q3ide_add_portal_frame(q3ide_win_t *win, qhandle_t shader);
extern void q3ide_add_depth_quad(q3ide_win_t *win);
extern void q3ide_add_poly(q3ide_win_t *win);
extern void q3ide_add_blood_splat(q3ide_win_t *win);

extern int Q3IDE_WM_TraceWindowHit(vec3_t start, vec3_t dir);

qboolean Q3IDE_WM_Attach(unsigned int id, vec3_t origin, vec3_t normal, float ww, float wh, qboolean do_start)
{
	int i, slot;
	q3ide_win_t *win;

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

	for (slot = 0; slot < 16; slot++) {
		if (!(q3ide_wm.slot_mask & (1u << slot)))
			break;
	}
	if (slot >= 16) {
		Com_Printf("q3ide: no scratch slots\n");
		return qfalse;
	}

	if (do_start && q3ide_wm.cap_start && q3ide_wm.cap_start(q3ide_wm.cap, id, Q3IDE_CAPTURE_FPS) != 0) {
		Com_Printf("q3ide: capture start failed id=%u\n", id);
		return qfalse;
	}

	q3ide_wm.slot_mask |= (1u << slot);
	win = &q3ide_wm.wins[i];
	memset(win, 0, sizeof(*win));
	win->active = qtrue;
	win->capture_id = id;
	win->scratch_slot = slot;
	VectorCopy(origin, win->origin);
	VectorCopy(normal, win->normal);
	win->world_w = ww;
	win->world_h = wh;
	// clang-format off
	Com_Printf("q3ide: attach win[%d] id=%u slot=%d pos=(%.0f,%.0f,%.0f) norm=(%.2f,%.2f,%.2f) size=%.0fx%.0f\n",
	           i, id, slot, origin[0], origin[1], origin[2], normal[0], normal[1], normal[2],
	           win->world_w, win->world_h);
	// clang-format on
	q3ide_wm.num_active++;
	return qtrue;
}

void Q3IDE_WM_MoveWindow(int idx, vec3_t origin, vec3_t normal)
{
	if (idx < 0 || idx >= Q3IDE_MAX_WIN)
		return;
	if (!q3ide_wm.wins[idx].active)
		return;
	VectorCopy(origin, q3ide_wm.wins[idx].origin);
	VectorCopy(normal, q3ide_wm.wins[idx].normal);
}

/* ── Public API ─────────────────────────────────────────────────── */

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
	return qtrue;
}

void Q3IDE_WM_Shutdown(void)
{
	Q3IDE_WM_CmdDetachAll();
	if (q3ide_wm.cap && q3ide_wm.cap_shutdown)
		q3ide_wm.cap_shutdown(q3ide_wm.cap);
	if (q3ide_wm.dylib)
		dlclose(q3ide_wm.dylib);
	if (q3ide_wm.fbuf)
		Z_Free(q3ide_wm.fbuf);
	memset(&q3ide_wm, 0, sizeof(q3ide_wm));
}

void Q3IDE_WM_PollFrames(void)
{
	int i;
	unsigned long long now_ms;
	if (!q3ide_wm.cap || !q3ide_wm.cap_get_frame)
		return;
	now_ms = Sys_Milliseconds();

	/* Poll for new/closed windows every 2 seconds */
	if (now_ms - q3ide_wm.last_scan_ms >= 2000) {
		Q3IDE_WM_PollChanges();
		q3ide_wm.last_scan_ms = now_ms;
	}
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		Q3ideFrame frame;
		if (!win->active)
			continue;
		frame = q3ide_wm.cap_get_frame(q3ide_wm.cap, win->capture_id);
		if (!frame.pixels)
			continue;
		if (frame.source_wid != win->capture_id) {
			if (win->frames < 5)
				Com_Printf("q3ide: MISMATCH [%d] want=%u got=%u\n", i, win->capture_id, frame.source_wid);
			continue;
		}
		/* Batch 1: Update status and timestamps */
		win->last_frame_ms = now_ms;
		if (win->first_frame_ms == 0)
			win->first_frame_ms = now_ms;
		win->status = Q3IDE_WIN_STATUS_ACTIVE;

		/* Batch 1: Skip frame based on target FPS */
		if (win->fps_target > 0 && win->fps_target < Q3IDE_FPS_FULL) {
			int skip_ratio = (Q3IDE_FPS_FULL / win->fps_target) - 1;
			if (win->skip_counter++ < skip_ratio)
				continue;
			win->skip_counter = 0;
		}

		if (win->tex_w > 0 && ((int) frame.width != win->tex_w || (int) frame.height != win->tex_h)) {
			win->world_h = win->world_w * ((float) frame.height / (float) frame.width);
			Com_Printf("q3ide: win %u resized to %dx%d (world %.0fx%.0f)\n", win->capture_id, (int) frame.width,
			           (int) frame.height, win->world_w, win->world_h);
		}
		q3ide_upload_frame(win, &frame);
		win->frames++;
	}
}

void Q3IDE_WM_InvalidateShaders(void)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_wm.wins[i].shader = 0;
		q3ide_wm.wins[i].frames = 0;
	}
	q3ide_wm.border_shader = 0;
	q3ide_wm.portal_shader = 0;
}

void Q3IDE_WM_AddPolys(void)
{
	int i, j, n = 0;
	int order[Q3IDE_MAX_WIN];
	static unsigned long long last_log_ms = 0;
	unsigned long long now_ms;

	/* Lazy-register shaders */
	if (!q3ide_wm.border_shader && re.RegisterShader)
		q3ide_wm.border_shader = re.RegisterShader("q3ide/border");
	if (!q3ide_wm.portal_shader && re.RegisterShader)
		q3ide_wm.portal_shader = re.RegisterShader("models/mapobjects/teleporter/energy");

	/* Collect active textured windows */
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].shader)
			order[n++] = i;

	/* Log every 3s: active windows, how many have shaders, border_shader status */
	now_ms = (unsigned long long) Sys_Milliseconds();
	if (now_ms - last_log_ms >= 3000) {
		int active = 0, with_shader = 0;
		for (i = 0; i < Q3IDE_MAX_WIN; i++) {
			if (q3ide_wm.wins[i].active)
				active++;
			if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].shader)
				with_shader++;
		}
		Com_Printf("q3ide: AddPolys active=%d shader=%d submitting=%d border_sh=%d\n", active, with_shader, n,
		           q3ide_wm.border_shader);
		last_log_ms = now_ms;
	}

	/* Insertion sort back-to-front by player_dist (farthest first = painter's algorithm) */
	for (i = 1; i < n; i++) {
		int tmp = order[i];
		j = i - 1;
		while (j >= 0 && q3ide_wm.wins[order[j]].player_dist < q3ide_wm.wins[tmp].player_dist) {
			order[j + 1] = order[j];
			j--;
		}
		order[j + 1] = tmp;
	}

	/* Render windows: opaque shaders write their own depth — no prepass needed. */
	for (i = 0; i < n; i++)
		q3ide_add_poly(&q3ide_wm.wins[order[i]]);

	/* Pass 3: teleporter energy overlay on wins[0] — additive so window shows through.
	 * Best after "q3ide snap" which positions wins[0] inside the real arch. */
	if (q3ide_wm.wins[0].active && q3ide_wm.wins[0].shader && q3ide_wm.portal_shader)
		q3ide_add_portal_frame(&q3ide_wm.wins[0], q3ide_wm.portal_shader);

	/* Pass 3: blood splats — DISABLED temporarily */
#if 0
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].hit_time_ms)
			q3ide_add_blood_splat(&q3ide_wm.wins[i]);
#endif
	(void) q3ide_add_blood_splat;
}

void Q3IDE_WM_CmdList(void)
{
	char *s;
	if (!q3ide_wm.cap || !q3ide_wm.cap_list_fmt) {
		Com_Printf("q3ide: not ready\n");
		return;
	}
	s = q3ide_wm.cap_list_fmt(q3ide_wm.cap);
	if (s) {
		Com_Printf("%s", s);
		q3ide_wm.cap_free_str(s);
	}
}

void Q3IDE_WM_CmdDetachAll(void)
{
	int i, n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		if (!q3ide_wm.wins[i].active)
			continue;
		if (q3ide_wm.cap_stop)
			q3ide_wm.cap_stop(q3ide_wm.cap, q3ide_wm.wins[i].capture_id);
		memset(&q3ide_wm.wins[i], 0, sizeof(q3ide_win_t));
		n++;
	}
	q3ide_wm.num_active = 0;
	q3ide_wm.slot_mask = 0;
	q3ide_wm.auto_attach = qfalse;
	if (n)
		Com_Printf("q3ide: detached %d window(s)\n", n);
}

void Q3IDE_WM_CmdStatus(void)
{
	int i;
	const char *status_str[] = {"INACTIVE", "ACTIVE", "IDLE", "ERROR"};
	Com_Printf("q3ide: %d active  dylib=%s\n", q3ide_wm.num_active, q3ide_wm.cap ? "ok" : "not loaded");
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active)
			continue;
		Com_Printf("  [%d] wid=%u slot=%d status=%s dist=%.0f fps=%d frames=%llu\n", i, w->capture_id, w->scratch_slot,
		           status_str[w->status], w->player_dist, w->fps_target, w->frames);
	}
}
