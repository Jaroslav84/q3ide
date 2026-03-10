/*
 * q3ide_interaction.c — Interaction state + Init + crosshair helpers.
 * Frame/key/mode API: q3ide_interaction_frame.c.
 */

#include "q3ide_interaction.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

q3ide_interaction_state_t q3ide_interaction;

void q3ide_do_click(unsigned int wid, float uv_x, float uv_y)
{
	if (q3ide_win_mngr.cap_inject_click)
		q3ide_win_mngr.cap_inject_click(q3ide_win_mngr.cap, wid, uv_x, uv_y);
	else
		Q3IDE_LOGW("click wid=%u uv=(%.2f,%.2f) - inject unavailable", wid, uv_x, uv_y);
}

/* Compute UV, distance, and 3D hit position for the window under the crosshair. */
int q3ide_crosshair_window(float *out_uv, float *out_dist, vec3_t out_hit_pos)
{
	vec3_t eye, fwd;
	float p, y;
	int hit;

	if (cls.state != CA_ACTIVE)
		return -1;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	p = cl.snap.ps.viewangles[PITCH] * (float) M_PI / 180.0f;
	y = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0] = cosf(p) * cosf(y);
	fwd[1] = cosf(p) * sinf(y);
	fwd[2] = -sinf(p);

	hit = Q3IDE_WM_TraceWindowHit(eye, fwd, -1);
	if (hit < 0)
		return -1;

	{
		q3ide_win_t *win = &q3ide_wm.wins[hit];
		vec3_t right, up, diff, hit_point;
		float t, denom, hw, hh, lx, ly;

		denom = DotProduct(fwd, win->normal);
		if (fabsf(denom) < 0.001f)
			return -1;

		VectorSubtract(win->origin, eye, diff);
		t = DotProduct(diff, win->normal) / denom;
		if (t < 0)
			return -1;

		hit_point[0] = eye[0] + fwd[0] * t;
		hit_point[1] = eye[1] + fwd[1] * t;
		hit_point[2] = eye[2] + fwd[2] * t;
		VectorCopy(hit_point, out_hit_pos);

		{
			float nx2 = win->normal[0], ny2 = win->normal[1];
			float hlen = sqrtf(nx2 * nx2 + ny2 * ny2);
			if (hlen > 0.01f) {
				right[0] = -ny2 / hlen;
				right[1] = nx2 / hlen;
				right[2] = 0.0f;
			} else {
				right[0] = 1.0f;
				right[1] = 0.0f;
				right[2] = 0.0f;
			}
			up[0] = 0.0f;
			up[1] = 0.0f;
			up[2] = 1.0f;
		}

		VectorSubtract(hit_point, win->origin, diff);
		lx = DotProduct(diff, right);
		ly = DotProduct(diff, up);
		hw = win->world_w * 0.5f;
		hh = win->world_h * 0.5f;

		out_uv[0] = 1.0f - (lx / hw + 1.0f) * 0.5f;
		out_uv[1] = 1.0f - (ly / hh + 1.0f) * 0.5f;
		*out_dist = t;

		Com_DPrintf("q3ide UV win=%d lx=%.2f ly=%.2f hw=%.2f hh=%.2f uv=(%.3f,%.3f)\n", hit, lx, ly, hw, hh, out_uv[0],
		            out_uv[1]);
		return hit;
	}
}

void Q3IDE_Interaction_Init(void)
{
	memset(&q3ide_interaction, 0, sizeof(q3ide_interaction));
	q3ide_interaction.focused_win = -1;
	q3ide_interaction.dwell_start_ms = -1.0f;
	q3ide_interaction.mode = Q3IDE_MODE_FPS;
	q3ide_interaction.pain_sfx[0] = S_RegisterSound("sound/player/Sarge/pain25_1.wav", qfalse);
	q3ide_interaction.pain_sfx[1] = S_RegisterSound("sound/player/Sarge/pain50_1.wav", qfalse);
	q3ide_interaction.pain_sfx[2] = S_RegisterSound("sound/player/Sarge/pain75_1.wav", qfalse);
}
