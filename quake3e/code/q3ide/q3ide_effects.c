/*
 * q3ide_effects.c — Portal frame and blood splat rendering.
 * Core window geometry: q3ide_geometry.c.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Basis vectors for a window quad — q3ide_geometry.c */
extern void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up);

void q3ide_add_portal_frame(q3ide_win_t *win, qhandle_t shader)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	float fwd = 2.0f;
	int i;

	if (!shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);
	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = win->origin[0] + win->normal[0] * fwd + right[0] * sx * hw + up[0] * sy * hh;
		verts[i].xyz[1] = win->origin[1] + win->normal[1] * fwd + right[1] * sx * hw + up[1] * sy * hh;
		verts[i].xyz[2] = win->origin[2] + win->normal[2] * fwd + right[2] * sx * hw + up[2] * sy * hh;
		verts[i].st[0] = (sx + 1.0f) * 0.5f;
		verts[i].st[1] = (sy + 1.0f) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = 255;
		verts[i].modulate.rgba[2] = 255;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(shader, 4, verts, 1);
	{
		polyVert_t rv[4];
		rv[0] = verts[3];
		rv[1] = verts[2];
		rv[2] = verts[1];
		rv[3] = verts[0];
		re.AddPolyToScene(shader, 4, rv, 1);
	}
}

static void q3ide_splat_quad(vec3_t center, vec3_t right, vec3_t up, vec3_t normal, float hw, float hh, byte r, byte g,
                             byte b, qhandle_t shader)
{
	polyVert_t v[4];
	int i;
	const float xs[4] = {-hw, hw, hw, -hw};
	const float ys[4] = {-hh, -hh, hh, hh};
	for (i = 0; i < 4; i++) {
		v[i].xyz[0] = center[0] + right[0] * xs[i] + up[0] * ys[i] + normal[0];
		v[i].xyz[1] = center[1] + right[1] * xs[i] + up[1] * ys[i] + normal[1];
		v[i].xyz[2] = center[2] + right[2] * xs[i] + up[2] * ys[i] + normal[2];
		v[i].st[0] = 0.5f;
		v[i].st[1] = 0.5f;
		v[i].modulate.rgba[0] = r;
		v[i].modulate.rgba[1] = g;
		v[i].modulate.rgba[2] = b;
		v[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(shader, 4, v, 1);
}

void q3ide_add_blood_splat(q3ide_win_t *win)
{
	unsigned long long now_ms, elapsed;
	vec3_t right, up;
	static const float drip_r[4] = {28.f, -26.f, 10.f, -18.f};
	static const float drip_u[4] = {20.f, -22.f, -28.f, 24.f};
	int i;

	if (!win->hit_time_ms || !q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

	now_ms = (unsigned long long) Sys_Milliseconds();
	elapsed = now_ms - win->hit_time_ms;
	if (elapsed >= Q3IDE_SPLAT_LIFE_MS) {
		win->hit_time_ms = 0;
		return;
	}

	q3ide_win_basis(win, right, up);
	/* Accent colour comes from scratch slot 63 texture; vertex brightness varies intensity. */
	q3ide_splat_quad(win->hit_pos, right, up, win->normal, 14.f, 4.f, 255, 255, 255, q3ide_wm.border_shader);
	q3ide_splat_quad(win->hit_pos, right, up, win->normal, 4.f, 14.f, 200, 200, 200, q3ide_wm.border_shader);
	for (i = 0; i < 4; i++) {
		vec3_t drip;
		drip[0] = win->hit_pos[0] + right[0] * drip_r[i] + up[0] * drip_u[i];
		drip[1] = win->hit_pos[1] + right[1] * drip_r[i] + up[1] * drip_u[i];
		drip[2] = win->hit_pos[2] + right[2] * drip_r[i] + up[2] * drip_u[i];
		q3ide_splat_quad(drip, right, up, win->normal, 5.f, 5.f, 180, 180, 180, q3ide_wm.border_shader);
	}
}
