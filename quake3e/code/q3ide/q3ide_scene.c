/*
 * q3ide_scene.c — AddPolys, InvalidateShaders, label, list/detach/status commands.
 * Mirror rendering: q3ide_mirror.c.  Frame polling: q3ide_poll.c.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <string.h>

/* Runtime params singleton — read-only everywhere else. */
const q3ide_params_t q3ide_params = {
    .borderThickness = 0.421875f,
    .windowDepth = 1.0f,
    .accentColor = {160, 0, 0},
};

/* Geometry helpers — q3ide_geometry.c / q3ide_geometry_border.c */
extern void q3ide_add_frame(q3ide_win_t *win, int border_mode, vec3_t origin, vec3_t normal);
extern void q3ide_add_poly(q3ide_win_t *win, vec3_t origin, vec3_t normal);
extern void q3ide_add_bg(q3ide_win_t *win, vec3_t origin, vec3_t normal);

/* Shoot-to-place selection — q3ide_engine.c */
extern int q3ide_selected_win;
extern int q3ide_aimed_win;


/* ── Shader invalidation ─────────────────────────────────────── */

void Q3IDE_WM_InvalidateShaders(void)
{
	int i;
	q3ide_wm.border_shader = 0;
	q3ide_wm.edge_shader = 0;
	q3ide_wm.bg_shader = 0;
	q3ide_wm.ov_green_shader = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		q3ide_wm.wins[i].shader = 0;
}

/* ── AddPolys — called before RenderScene each frame ─────────── */

void Q3IDE_WM_AddPolys(void)
{
	int i;
	int highlight_win;

	if (!re.AddPolyToScene)
		return;

	if (q3ide_wm.wins_hidden)
		return;

	if (!q3ide_wm.border_shader && re.RegisterShader && re.UploadCinematic) {
		/* Slot 63: accent colour (Quake red) — borders.
		 * Bytes in BGRA order so the VK_FORMAT_B8G8R8A8_UNORM override path
		 * is triggered (format=GL_BGRA). Passthrough in resample_image_data
		 * keeps bytes as-is: byte[2]=R=160 → GPU reads red correctly.
		 * Two calls: first creates the scratch image, second dirty-updates it
		 * so OpenGL also gets the correct GL_BGRA path (R_CreateImage ignores format). */
		byte accent_bgra[4] = {q3ide_params.accentColor[2], q3ide_params.accentColor[1], q3ide_params.accentColor[0],
		                       255};
		re.UploadCinematic(1, 1, 1, 1, accent_bgra, 63, qfalse, 0x80E1 /* GL_BGRA */);
		re.UploadCinematic(1, 1, 1, 1, accent_bgra, 63, qtrue, 0x80E1 /* GL_BGRA */);
		q3ide_wm.border_shader = re.RegisterShader("q3ide/win63");
		/* Slot 62: solid black — TV chassis edge quads. */
		byte black_bgra[4] = {0, 0, 0, 255};
		re.UploadCinematic(1, 1, 1, 1, black_bgra, 62, qfalse, 0x80E1 /* GL_BGRA */);
		re.UploadCinematic(1, 1, 1, 1, black_bgra, 62, qtrue, 0x80E1 /* GL_BGRA */);
		q3ide_wm.edge_shader = re.RegisterShader("q3ide/win62");
		q3ide_wm.bg_shader = re.RegisterShader("q3ide/bg");
		/* Slot 61: solid green — wall-placed windows shown in arc. */
		byte green_bgra[4] = {0, 128, 0, 255};
		re.UploadCinematic(1, 1, 1, 1, green_bgra, 61, qfalse, 0x80E1 /* GL_BGRA */);
		re.UploadCinematic(1, 1, 1, 1, green_bgra, 61, qtrue, 0x80E1 /* GL_BGRA */);
		q3ide_wm.ov_green_shader = re.RegisterShader("q3ide/win61");
	}

	highlight_win = (q3ide_aimed_win >= 0) ? q3ide_aimed_win : q3ide_selected_win;

	{
		extern int Q3IDE_DragResize_GetDragWin(void);
		int drag_win = Q3IDE_DragResize_GetDragWin();

		/* Two-pass: all normal windows first, drag_win last (always on top). */
		for (i = 0; i < Q3IDE_MAX_WIN + 1; i++) {
			int idx = (i < Q3IDE_MAX_WIN) ? i : drag_win;
			q3ide_win_t *win;

			if (idx < 0)
				continue; /* no drag win */
			if (i < Q3IDE_MAX_WIN && idx == drag_win)
				continue; /* skip drag_win in first pass */

			win = &q3ide_wm.wins[idx];
			if (!win->active)
				continue;

			/* ── Wall render ── */
			if (win->wall_placed && win->los_visible) {
				int bm = (idx == highlight_win) ? 1 : 0;
				q3ide_add_bg(win, win->origin, win->normal);
#if !Q3IDE_DISABLE_EDGE_QUADS
				q3ide_add_frame(win, bm, win->origin, win->normal);
#endif
				if (win->shader)
					q3ide_add_poly(win, win->origin, win->normal);
			}

			/* ── Arc render ── */
			if (win->in_overview) {
				int arc_bm = (idx == highlight_win) ? 1 : (win->wall_placed ? 2 : 0);
				/* Use base (unzoomed) size for arc geometry — zoom must not affect I/O modes. */
				float saved_w = win->world_w, saved_h = win->world_h;
				win->world_w = win->base_world_w;
				win->world_h = win->base_world_h;
				q3ide_add_bg(win, win->ov_origin, win->ov_normal);
#if !Q3IDE_DISABLE_EDGE_QUADS
				q3ide_add_frame(win, arc_bm, win->ov_origin, win->ov_normal);
#endif
				if (win->shader)
					q3ide_add_poly(win, win->ov_origin, win->ov_normal);
				win->world_w = saved_w;
				win->world_h = saved_h;
			}
		}
	}
}

/* ── Label ───────────────────────────────────────────────────── */

void Q3IDE_WM_SetLabel(unsigned int capture_id, const char *label)
{
	int idx = Q3IDE_WM_FindById(capture_id);
	if (idx < 0)
		return;
	Q_strncpyz(q3ide_wm.wins[idx].label, label ? label : "", sizeof(q3ide_wm.wins[idx].label));
}

/* ── Simple console commands ─────────────────────────────────── */

void Q3IDE_WM_CmdList(void)
{
	if (!q3ide_win_mngr.cap || !q3ide_win_mngr.cap_list_fmt) {
		Com_Printf("q3ide: no capture available\n");
		return;
	}
	{
		char *s = q3ide_win_mngr.cap_list_fmt(q3ide_win_mngr.cap);
		Com_Printf("%s\n", s ? s : "(none)");
		if (s && q3ide_win_mngr.cap_free_str)
			q3ide_win_mngr.cap_free_str(s);
	}
}

void Q3IDE_WM_CmdDetachAll(void)
{
	int i, n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active || !w->is_tunnel)
			continue;
		/* Always stop — display captures have owns_stream=qfalse but their
		 * stream lives in Rust display_streams; leaving it running causes
		 * AlreadyCapturing on the next cap_start_disp call. */
		if (q3ide_win_mngr.cap_stop)
			q3ide_win_mngr.cap_stop(q3ide_win_mngr.cap, w->capture_id);
		memset(w, 0, sizeof(q3ide_win_t));
		q3ide_wm.num_active--;
		n++;
	}
	Com_Printf("q3ide: %d tunnel(s) detached\n", n);
}

/*
 * Soft-detach: clear C-side geometry without calling cap_stop.
 * Rust streams stay warm so the next shoot-to-place reuses them and content
 * appears on the very first frame instead of after the SCK cold-start delay.
 * Used by the K key; hard detach (CmdDetachAll) is reserved for dylib reload.
 */
void Q3IDE_WM_CmdSoftDetachAll(void)
{
	int i, n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active || !w->is_tunnel)
			continue;
		/* intentionally skip cap_stop — streams stay live in Rust */
		memset(w, 0, sizeof(q3ide_win_t));
		q3ide_wm.num_active--;
		n++;
	}
	Com_Printf("q3ide: %d tunnel(s) soft-detached (streams warm)\n", n);
}

void Q3IDE_WM_CmdStatus(void)
{
	int i, count = 0;
	static const char *snames[] = {"INACTIVE", "ACTIVE", "IDLE", "ERROR"};
	Com_Printf("q3ide status: dylib=%s cap=%s num_active=%d\n", q3ide_wm.dylib ? "yes" : "no",
	           q3ide_win_mngr.cap ? "yes" : "no", q3ide_wm.num_active);
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active)
			continue;
		Com_Printf("  [%d] id=%u slot=%d sz=%.0fx%.0f frames=%llu status=%s"
		           " dist=%.0f wall=%d ov=%d%s%s\n",
		           i, w->capture_id, w->scratch_slot, w->world_w, w->world_h, w->frames,
		           snames[w->status < 4 ? w->status : 3], w->player_dist, (int) w->wall_placed, (int) w->in_overview,
		           w->label[0] ? " label=" : "", w->label[0] ? w->label : "");
		count++;
	}
	if (!count)
		Com_Printf("  (no active windows)\n");
}
