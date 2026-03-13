/* q3ide_drag_resize.c — CMD + aim-to-move and scroll-resize for tunnel windows.
 *
 * Hold CMD, aim at a window:
 *   mouse move  → window follows crosshair (ray-plane intersection with wall).
 *   scroll up   → smaller.  scroll down → bigger.
 * Release key to stick.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_view_modes.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

extern int q3ide_aimed_win;

static qboolean s_cmd_held = qfalse;
static int s_drag_win = -1;       /* window being dragged/resized, -1 = none */
static vec3_t s_wall_org;         /* point on wall plane (fixed for session) */
static vec3_t s_wall_normal;      /* wall normal (fixed for session) */
static float s_base_w = 0.0f;     /* world_w when resize session started */
static float s_base_h = 0.0f;     /* world_h when resize session started */
static float s_base_diag = 0.0f;  /* diagonal when resize session started */
static int s_scroll_acc = 0;      /* accumulated scroll ticks this session */
static float s_prev_scale = 1.0f; /* scale applied on previous scroll tick */

static void drag_start(int win_idx)
{
	q3ide_win_t *w = &q3ide_wm.wins[win_idx];
	s_drag_win = win_idx;
	VectorCopy(w->origin, s_wall_org);
	VectorCopy(w->normal, s_wall_normal);
	s_base_w = w->world_w;
	s_base_h = w->world_h;
	s_base_diag = sqrtf(w->world_w * w->world_w + w->world_h * w->world_h);
	s_scroll_acc = 0;
	s_prev_scale = 1.0f;
	Q3IDE_LOGI("drag start win[%d] diag=%.0f", win_idx, s_base_diag);
}

static void drag_end(void)
{
	if (s_drag_win >= 0) {
		q3ide_win_t *w = &q3ide_wm.wins[s_drag_win];
		if (w->active) {
			/* Final move with clamp — snap window within wall bounds. */
			Q3IDE_WM_MoveWindow(s_drag_win, w->origin, w->normal, qfalse);
			Q3IDE_LOGI("drag end win[%d] pos=(%.0f,%.0f,%.0f)", s_drag_win, w->origin[0], w->origin[1], w->origin[2]);
		}
		s_drag_win = -1;
	}
}

void Q3IDE_DragResize_OnCmdKey(qboolean down)
{
	s_cmd_held = down;
	if (!down)
		drag_end();
}

int Q3IDE_DragResize_GetDragWin(void)
{
	return s_drag_win;
}

/* Called from Q3IDE_Frame after q3ide_aimed_win is set.
 * Projects player aim onto the window's wall plane and moves the window there. */
void Q3IDE_DragResize_Frame(void)
{
	vec3_t eye, fwd, diff, hit;
	float p, y, denom, t;
	q3ide_win_t *w;

	if (!s_cmd_held)
		return;

	/* Claim the aimed window as drag target if not yet started. */
	if (s_drag_win < 0 && q3ide_aimed_win >= 0)
		drag_start(q3ide_aimed_win);

	if (s_drag_win < 0)
		return;

	w = &q3ide_wm.wins[s_drag_win];
	if (!w->active) {
		drag_end();
		return;
	}

	/* Eye + forward from player state. */
	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	p = cl.snap.ps.viewangles[PITCH] * (float) M_PI / 180.0f;
	y = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0] = cosf(p) * cosf(y);
	fwd[1] = cosf(p) * sinf(y);
	fwd[2] = -sinf(p);

	/* Ray-plane intersection: plane defined by s_wall_normal through s_wall_org. */
	denom = DotProduct(fwd, s_wall_normal);
	if (fabsf(denom) < 0.01f)
		return; /* aiming nearly parallel to wall — skip this frame */

	VectorSubtract(s_wall_org, eye, diff);
	t = DotProduct(diff, s_wall_normal) / denom;
	if (t < 1.0f)
		return; /* wall is behind the player */

	VectorMA(eye, t, fwd, hit);

	/* skip_clamp=qtrue — no ray traces mid-drag, applied on release. */
	Q3IDE_WM_MoveWindow(s_drag_win, hit, s_wall_normal, qtrue);
}

/* Q3IDE_OnMouseEvent — no longer used for drag (aim drives movement now).
 * Kept for cl_input.c hook; always returns qfalse so mouselook is not suppressed. */
qboolean Q3IDE_OnMouseEvent(int dx, int dy)
{
	(void) dx;
	(void) dy;
	return qfalse;
}

/* Called from key event handler on scroll wheel.
 * dir: +1 = scroll up (smaller), -1 = scroll down (bigger).
 * Returns qtrue if consumed (drag active), qfalse to fall through to overview scroll. */
qboolean Q3IDE_DragResize_OnScroll(int dir)
{
	float new_diag, scale;
	q3ide_win_t *w;

	if (s_drag_win < 0 || s_base_diag < 1.0f)
		return qfalse;
	w = &q3ide_wm.wins[s_drag_win];
	if (!w->active)
		return qfalse;

	s_scroll_acc += dir;
	new_diag = s_base_diag + (float) s_scroll_acc * Q3IDE_RESIZE_SCROLL_STEP;
	if (new_diag < Q3IDE_RESIZE_MIN_DIAG)
		new_diag = Q3IDE_RESIZE_MIN_DIAG;
	if (new_diag > Q3IDE_RESIZE_MAX_DIAG)
		new_diag = Q3IDE_RESIZE_MAX_DIAG;

	scale = new_diag / s_base_diag;
	w->world_w = s_base_w * scale;
	w->world_h = s_base_h * scale;

	/* Focus3 (I): resize all display captures together.
	 * Apply delta (scale / prev_scale) so companions stay in sync without
	 * needing their own base sizes stored. */
	if (Q3IDE_ViewModes_Focus3Active() && s_prev_scale > 0.001f) {
		float delta = scale / s_prev_scale;
		int i;
		for (i = 0; i < Q3IDE_MAX_WIN; i++) {
			q3ide_win_t *fw = &q3ide_wm.wins[i];
			if (!fw->active || fw->owns_stream || i == s_drag_win)
				continue;
			fw->world_w *= delta;
			fw->world_h *= delta;
		}
	}
	s_prev_scale = scale;
	return qtrue;
}
