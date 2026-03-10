/*
 * q3ide_scene.c — AddPolys, InvalidateShaders, label, list/detach/status commands.
 * Mirror rendering: q3ide_mirror.c.  Frame polling: q3ide_poll.c.
 */

#include "q3ide_wm.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <string.h>

/* Geometry helpers — q3ide_geom.c */
extern void q3ide_add_portal_frame(q3ide_win_t *win, qhandle_t shader);
extern void q3ide_add_depth_quad(q3ide_win_t *win);
extern void q3ide_add_select_border(q3ide_win_t *win);
extern void q3ide_add_hover_border(q3ide_win_t *win);
extern void q3ide_add_poly(q3ide_win_t *win);
extern void q3ide_add_blood_splat(q3ide_win_t *win);

/* Shoot-to-place selection — q3ide_portal.c */
extern int q3ide_selected_win;


/* ── Shader invalidation ─────────────────────────────────────── */

void Q3IDE_WM_InvalidateShaders(void)
{
	int i;
	q3ide_wm.border_shader = 0;
	q3ide_wm.portal_shader = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		q3ide_wm.wins[i].shader = 0;
}

/* ── AddPolys — called before RenderScene each frame ─────────── */

void Q3IDE_WM_AddPolys(void)
{
	int i;

	if (!re.AddPolyToScene)
		return;

	if (!q3ide_wm.border_shader && re.RegisterShader)
		q3ide_wm.border_shader = re.RegisterShader("q3ide/border");
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];

		if (!win->active)
			continue;

		/* LOS cached once per frame in Q3IDE_Frame — no per-pass trace needed. */
		if (!win->los_visible)
			continue;

		if (i == q3ide_selected_win)
			q3ide_add_select_border(win);
		q3ide_add_hover_border(win);
		if (!win->shader)
			continue; /* no texture yet — borders drawn, but skip depth/poly/splat */
		q3ide_add_depth_quad(win);
		q3ide_add_poly(win);
		q3ide_add_blood_splat(win);
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
	if (!q3ide_wm.cap || !q3ide_wm.cap_list_fmt) {
		Com_Printf("q3ide: no capture available\n");
		return;
	}
	{
		char *s = q3ide_wm.cap_list_fmt(q3ide_wm.cap);
		Com_Printf("%s\n", s ? s : "(none)");
		if (s && q3ide_wm.cap_free_str)
			q3ide_wm.cap_free_str(s);
	}
}

void Q3IDE_WM_CmdDetachAll(void)
{
	int i, n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active || !w->is_tunnel)
			continue;
		if (w->owns_stream && q3ide_wm.cap_stop)
			q3ide_wm.cap_stop(q3ide_wm.cap, w->capture_id);
		memset(w, 0, sizeof(q3ide_win_t));
		q3ide_wm.num_active--;
		n++;
	}
	Com_Printf("q3ide: %d tunnel(s) detached\n", n);
}

void Q3IDE_WM_CmdStatus(void)
{
	int i, count = 0;
	static const char *snames[] = {"INACTIVE", "ACTIVE", "IDLE", "ERROR"};
	Com_Printf("q3ide status: dylib=%s cap=%s auto_attach=%d num_active=%d\n", q3ide_wm.dylib ? "yes" : "no",
	           q3ide_wm.cap ? "yes" : "no", q3ide_wm.auto_attach, q3ide_wm.num_active);
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active)
			continue;
		Com_Printf("  [%d] id=%u slot=%d sz=%.0fx%.0f frames=%llu status=%s"
		           " dist=%.0f fps=%d%s%s\n",
		           i, w->capture_id, w->scratch_slot, w->world_w, w->world_h, w->frames,
		           snames[w->status < 4 ? w->status : 3], w->player_dist, w->fps_target, w->label[0] ? " label=" : "",
		           w->label[0] ? w->label : "");
		count++;
	}
	if (!count)
		Com_Printf("  (no active windows)\n");
}
