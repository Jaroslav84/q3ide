/*
 * q3ide_geom.c — Window polygon generation and blood splat rendering.
 * Clamping + ray-hit: q3ide_geom_clamp.c.
 */

#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../../../q3ide_design.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Compute right/up basis for a window quad. */
static void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up)
{
	float nx = win->normal[0], ny = win->normal[1];
	float horiz_len = sqrtf(nx * nx + ny * ny);
	if (horiz_len > 0.01f) {
		right[0] = -ny / horiz_len;
		right[1] = nx / horiz_len;
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

void q3ide_add_depth_quad(q3ide_win_t *win)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	float hover_lift = win->hover_t > 0.0f ? win->hover_t * 18.0f : 0.0f;
	int i;

	if (!q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);
	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = win->origin[0] + right[0] * sx * hw + up[0] * sy * hh + win->normal[0] * hover_lift;
		verts[i].xyz[1] = win->origin[1] + right[1] * sx * hw + up[1] * sy * hh + win->normal[1] * hover_lift;
		verts[i].xyz[2] = win->origin[2] + right[2] * sx * hw + up[2] * sy * hh + win->normal[2] * hover_lift;
		verts[i].st[0] = 0.5f;
		verts[i].st[1] = 0.5f;
		verts[i].modulate.rgba[0] = 0;
		verts[i].modulate.rgba[1] = 0;
		verts[i].modulate.rgba[2] = 0;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(q3ide_wm.border_shader, 4, verts, 1);
}

void q3ide_add_poly(q3ide_win_t *win)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	float pop = 1.0f + win->hover_t * 4.0f;
	int i;

	if (!win->shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);

	/* Front face */
	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = win->origin[0] + right[0] * sx * hw + up[0] * sy * hh + win->normal[0] * pop;
		verts[i].xyz[1] = win->origin[1] + right[1] * sx * hw + up[1] * sy * hh + win->normal[1] * pop;
		verts[i].xyz[2] = win->origin[2] + right[2] * sx * hw + up[2] * sy * hh + win->normal[2] * pop;
		verts[i].st[0] = 1.0f - (sx + 1.0f) * 0.5f;
		verts[i].st[1] = 1.0f - (sy + 1.0f) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = (byte) (255 - 80.0f * win->hover_t);
		verts[i].modulate.rgba[2] = (byte) (255 - 80.0f * win->hover_t);
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(win->shader, 4, verts, 1);

	/* Back face — visible from both sides */
	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = win->origin[0] - right[0] * sx * hw + up[0] * sy * hh - win->normal[0];
		verts[i].xyz[1] = win->origin[1] - right[1] * sx * hw + up[1] * sy * hh - win->normal[1];
		verts[i].xyz[2] = win->origin[2] - right[2] * sx * hw + up[2] * sy * hh - win->normal[2];
		verts[i].st[0] = 1.0f - (sx + 1.0f) * 0.5f;
		verts[i].st[1] = 1.0f - (sy + 1.0f) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = 255;
		verts[i].modulate.rgba[2] = 255;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(win->shader, 4, verts, 1);
}

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

#define Q3IDE_SPLAT_LIFE_MS 700ULL

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
	q3ide_splat_quad(win->hit_pos, right, up, win->normal, 14.f, 4.f, 220, 0, 0, q3ide_wm.border_shader);
	q3ide_splat_quad(win->hit_pos, right, up, win->normal, 4.f, 14.f, 180, 0, 0, q3ide_wm.border_shader);
	for (i = 0; i < 4; i++) {
		vec3_t drip;
		drip[0] = win->hit_pos[0] + right[0] * drip_r[i] + up[0] * drip_u[i];
		drip[1] = win->hit_pos[1] + right[1] * drip_r[i] + up[1] * drip_u[i];
		drip[2] = win->hit_pos[2] + right[2] * drip_r[i] + up[2] * drip_u[i];
		q3ide_splat_quad(drip, right, up, win->normal, 5.f, 5.f, 200, 10, 10, q3ide_wm.border_shader);
	}
}
