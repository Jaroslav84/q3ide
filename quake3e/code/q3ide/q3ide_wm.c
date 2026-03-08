/*
 * q3ide_wm.c — Q3IDE Window Manager core.
 *
 * Dylib loading, frame upload to GPU scratch slots, poly rendering,
 * wall ray tracing, and the window attach/detach primitives.
 * Complex commands (attach all, desktop) live in q3ide_cmd.c.
 */

#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
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

#define Q3IDE_WALL_DIST   512.0f
#define Q3IDE_WALL_OFFSET 2.0f

/* Global state — also accessed by q3ide_cmd.c via q3ide_wm_internal.h */
q3ide_wm_t q3ide_wm;

static qboolean q3ide_load_dylib(void)
{
	void *dl = dlopen(Q3IDE_DYLIB, RTLD_LAZY);
	if (!dl) dl = dlopen("./" Q3IDE_DYLIB, RTLD_LAZY);
	if (!dl) { Com_Printf("q3ide: cannot load dylib\n"); return qfalse; }
	q3ide_wm.dylib = dl;

#define SYM(f, n) q3ide_wm.f = dlsym(dl, n)
	SYM(cap_init,       "q3ide_init");
	SYM(cap_shutdown,   "q3ide_shutdown");
	SYM(cap_list_fmt,   "q3ide_list_windows_formatted");
	SYM(cap_free_str,   "q3ide_free_string");
	SYM(cap_list_wins,  "q3ide_list_windows");
	SYM(cap_free_wlist, "q3ide_free_window_list");
	SYM(cap_start,      "q3ide_start_capture");
	SYM(cap_stop,       "q3ide_stop_capture");
	SYM(cap_get_frame,  "q3ide_get_frame");
	SYM(cap_list_disp,  "q3ide_list_displays");
	SYM(cap_free_dlist, "q3ide_free_display_list");
	SYM(cap_start_disp, "q3ide_start_display_capture");
#undef SYM

	if (!q3ide_wm.cap_init || !q3ide_wm.cap_shutdown || !q3ide_wm.cap_get_frame) {
		Com_Printf("q3ide: missing dylib symbols\n");
		dlclose(dl);
		q3ide_wm.dylib = NULL;
		return qfalse;
	}
	return qtrue;
}

qboolean Q3IDE_WM_TraceWall(vec3_t start, vec3_t dir,
                              vec3_t out_pos, vec3_t out_normal)
{
	trace_t tr;
	vec3_t end, mins = {0, 0, 0}, maxs = {0, 0, 0};
	end[0] = start[0] + dir[0] * Q3IDE_WALL_DIST;
	end[1] = start[1] + dir[1] * Q3IDE_WALL_DIST;
	end[2] = start[2] + dir[2] * Q3IDE_WALL_DIST;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction >= 1.0f || tr.startsolid) return qfalse;
	out_pos[0] = tr.endpos[0] + tr.plane.normal[0] * Q3IDE_WALL_OFFSET;
	out_pos[1] = tr.endpos[1] + tr.plane.normal[1] * Q3IDE_WALL_OFFSET;
	out_pos[2] = tr.endpos[2] + tr.plane.normal[2] * Q3IDE_WALL_OFFSET;
	VectorCopy(tr.plane.normal, out_normal);
	return qtrue;
}

static void q3ide_upload_frame(q3ide_win_t *win, const Q3ideFrame *frame)
{
	int needed = (int)(frame->width * frame->height * 4);
	int row    = (int)frame->width * 4;
	unsigned int r, c;

	if (!re.UploadCinematic) return;

	if (needed > q3ide_wm.fbuf_size) {
		if (q3ide_wm.fbuf) Z_Free(q3ide_wm.fbuf);
		q3ide_wm.fbuf = Z_Malloc(needed);
		q3ide_wm.fbuf_size = needed;
	}

	for (r = 0; r < frame->height; r++) {
		const unsigned char *s = frame->pixels + r * frame->stride;
		byte *d = q3ide_wm.fbuf + r * row;
		for (c = 0; c < frame->width; c++, s += 4, d += 4) {
			d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3]; /* BGRA→RGBA */
		}
	}

	re.UploadCinematic((int)frame->width, (int)frame->height,
		(int)frame->width, (int)frame->height,
		q3ide_wm.fbuf, win->scratch_slot, qtrue);
	win->tex_w = (int)frame->width;
	win->tex_h = (int)frame->height;

	if (win->frames == 0) {
		char name[32];
		Com_sprintf(name, sizeof(name), "*scratch%d", win->scratch_slot);
		win->shader = re.RegisterShader(name);
		Com_Printf("q3ide: win[%d] id=%u slot=%d shader=%d %dx%d\n",
			(int)(win - q3ide_wm.wins), win->capture_id,
			win->scratch_slot, win->shader, frame->width, frame->height);
	}
}

static void q3ide_add_poly(q3ide_win_t *win)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	int i, face;

	if (!win->shader || !re.AddPolyToScene) return;

	if (fabsf(win->normal[2]) > 0.99f) {
		vec3_t fwd = {1, 0, 0};
		CrossProduct(win->normal, fwd, right);
	} else {
		vec3_t wup = {0, 0, 1};
		CrossProduct(win->normal, wup, right);
	}
	VectorNormalize(right);
	CrossProduct(right, win->normal, up);
	VectorNormalize(up);

	for (face = 0; face < 2; face++) {
		float sign = face ? -1.0f : 1.0f;
		for (i = 0; i < 4; i++) {
			float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
			float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
			verts[i].xyz[0] = win->origin[0] + sign*right[0]*sx*hw + up[0]*sy*hh;
			verts[i].xyz[1] = win->origin[1] + sign*right[1]*sx*hw + up[1]*sy*hh;
			verts[i].xyz[2] = win->origin[2] + sign*right[2]*sx*hw + up[2]*sy*hh;
			verts[i].st[0] = 1.0f - (sx + 1.0f) * 0.5f;
			verts[i].st[1] = 1.0f - (sy + 1.0f) * 0.5f;
			verts[i].modulate.rgba[0] = 255;
			verts[i].modulate.rgba[1] = 255;
			verts[i].modulate.rgba[2] = 255;
			verts[i].modulate.rgba[3] = 255;
		}
		re.AddPolyToScene(win->shader, 4, verts, 1);
	}
}

qboolean Q3IDE_WM_Attach(unsigned int id, vec3_t origin, vec3_t normal,
                           float ww, float wh, qboolean do_start)
{
	int i, slot;
	q3ide_win_t *win;

	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == id)
			return qfalse;

	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (!q3ide_wm.wins[i].active) break;
	if (i >= Q3IDE_MAX_WIN) { Com_Printf("q3ide: max windows\n"); return qfalse; }

	slot = q3ide_wm.next_slot;
	if (slot >= 16) { Com_Printf("q3ide: no scratch slots\n"); return qfalse; }

	if (do_start && q3ide_wm.cap_start &&
	    q3ide_wm.cap_start(q3ide_wm.cap, id, Q3IDE_CAPTURE_FPS) != 0) {
		Com_Printf("q3ide: capture start failed id=%u\n", id);
		return qfalse;
	}

	q3ide_wm.next_slot++;
	win = &q3ide_wm.wins[i];
	memset(win, 0, sizeof(*win));
	win->active = qtrue;
	win->capture_id = id;
	win->scratch_slot = slot;
	VectorCopy(origin, win->origin);
	VectorCopy(normal, win->normal);
	win->world_w = ww;
	win->world_h = wh;
	q3ide_wm.num_active++;
	return qtrue;
}

int Q3IDE_WM_TraceWindowHit(vec3_t start, vec3_t dir)
{
	int i, best = -1;
	float best_t = Q3IDE_WALL_DIST;

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		vec3_t right, up, diff, hit;
		float denom, t, hw, hh, lx, ly;

		if (!win->active || !win->shader) continue;

		denom = DotProduct(dir, win->normal);
		if (fabsf(denom) < 0.001f) continue;

		VectorSubtract(win->origin, start, diff);
		t = DotProduct(diff, win->normal) / denom;
		if (t < 0 || t >= best_t) continue;

		hit[0] = start[0] + dir[0] * t;
		hit[1] = start[1] + dir[1] * t;
		hit[2] = start[2] + dir[2] * t;

		if (fabsf(win->normal[2]) > 0.99f) {
			vec3_t fwd = {1, 0, 0};
			CrossProduct(win->normal, fwd, right);
		} else {
			vec3_t wup = {0, 0, 1};
			CrossProduct(win->normal, wup, right);
		}
		VectorNormalize(right);
		CrossProduct(right, win->normal, up);
		VectorNormalize(up);

		VectorSubtract(hit, win->origin, diff);
		lx = DotProduct(diff, right);
		ly = DotProduct(diff, up);
		hw = win->world_w * 0.5f;
		hh = win->world_h * 0.5f;

		if (fabsf(lx) <= hw && fabsf(ly) <= hh) {
			best_t = t;
			best = i;
		}
	}
	return best;
}

void Q3IDE_WM_MoveWindow(int idx, vec3_t origin, vec3_t normal)
{
	if (idx < 0 || idx >= Q3IDE_MAX_WIN) return;
	if (!q3ide_wm.wins[idx].active) return;
	VectorCopy(origin, q3ide_wm.wins[idx].origin);
	VectorCopy(normal, q3ide_wm.wins[idx].normal);
}

/* ── Public API ─────────────────────────────────────────────────── */

qboolean Q3IDE_WM_Init(void)
{
	memset(&q3ide_wm, 0, sizeof(q3ide_wm));
	if (!q3ide_load_dylib()) return qfalse;
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
	if (q3ide_wm.cap && q3ide_wm.cap_shutdown) q3ide_wm.cap_shutdown(q3ide_wm.cap);
	if (q3ide_wm.dylib) dlclose(q3ide_wm.dylib);
	if (q3ide_wm.fbuf) Z_Free(q3ide_wm.fbuf);
	memset(&q3ide_wm, 0, sizeof(q3ide_wm));
}

void Q3IDE_WM_PollFrames(void)
{
	int i;
	if (!q3ide_wm.cap || !q3ide_wm.cap_get_frame) return;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		Q3ideFrame frame;
		if (!win->active) continue;
		frame = q3ide_wm.cap_get_frame(q3ide_wm.cap, win->capture_id);
		if (!frame.pixels) continue;
		if (frame.source_wid != win->capture_id) {
			if (win->frames < 5)
				Com_Printf("q3ide: MISMATCH [%d] want=%u got=%u\n",
					i, win->capture_id, frame.source_wid);
			continue;
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
}

void Q3IDE_WM_AddPolys(void)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].shader)
			q3ide_add_poly(&q3ide_wm.wins[i]);
}

void Q3IDE_WM_CmdList(void)
{
	char *s;
	if (!q3ide_wm.cap || !q3ide_wm.cap_list_fmt) {
		Com_Printf("q3ide: not ready\n"); return;
	}
	s = q3ide_wm.cap_list_fmt(q3ide_wm.cap);
	if (s) { Com_Printf("%s", s); q3ide_wm.cap_free_str(s); }
}

void Q3IDE_WM_CmdDetachAll(void)
{
	int i, n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		if (!q3ide_wm.wins[i].active) continue;
		if (q3ide_wm.cap_stop) q3ide_wm.cap_stop(q3ide_wm.cap, q3ide_wm.wins[i].capture_id);
		memset(&q3ide_wm.wins[i], 0, sizeof(q3ide_win_t));
		n++;
	}
	q3ide_wm.num_active = 0;
	q3ide_wm.next_slot = 0;
	if (n) Com_Printf("q3ide: detached %d window(s)\n", n);
}

void Q3IDE_WM_CmdStatus(void)
{
	int i;
	Com_Printf("q3ide: %d active  dylib=%s\n",
		q3ide_wm.num_active, q3ide_wm.cap ? "ok" : "not loaded");
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active) continue;
		Com_Printf("  [%d] id=%u slot=%d shader=%d %dx%d frames=%llu\n",
			i, w->capture_id, w->scratch_slot, w->shader,
			w->tex_w, w->tex_h, w->frames);
	}
}
