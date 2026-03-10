/*
 * q3ide_geom.c — Window polygon generation (border, strips, main quad).
 * Effects (portal frame, blood splat): q3ide_effects.c.
 * Clamping + ray-hit: q3ide_geom_clamp.c.
 */

#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Compute right/up basis for a window quad. */
void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up)
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

/* Draw an expanded solid-color quad around the window.
 * expand > 0: visible as a border frame (main poly covers the center).
 * Used for depth background (expand=0, black) and selection highlight (expand>0, red). */
static void q3ide_border_quad(q3ide_win_t *win, float expand, byte r, byte g, byte b)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f + expand;
	float hh = win->world_h * 0.5f + expand;
	float depth = 0.5f;
	int i;

	if (!q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);
	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = win->origin[0] + right[0] * sx * hw + up[0] * sy * hh + win->normal[0] * depth;
		verts[i].xyz[1] = win->origin[1] + right[1] * sx * hw + up[1] * sy * hh + win->normal[1] * depth;
		verts[i].xyz[2] = win->origin[2] + right[2] * sx * hw + up[2] * sy * hh + win->normal[2] * depth;
		verts[i].st[0] = 0.5f;
		verts[i].st[1] = 0.5f;
		verts[i].modulate.rgba[0] = r;
		verts[i].modulate.rgba[1] = g;
		verts[i].modulate.rgba[2] = b;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(q3ide_wm.border_shader, 4, verts, 1);
}

void q3ide_add_depth_quad(q3ide_win_t *win)
{
	q3ide_border_quad(win, 0.0f, 0, 0, 0);
}

/*
 * Render one strip. back_face=qtrue: negates right + normal direction,
 * making the strip face the opposite side (visible when viewed from behind).
 */
static void q3ide_border_strip(q3ide_win_t *win, float x0, float y0, float x1, float y1, float depth,
                               qboolean back_face, byte r, byte g, byte b)
{
	polyVert_t verts[4];
	vec3_t right, up;
	int i;
	float s = back_face ? -1.0f : 1.0f;
	const float xs[4] = {s * x0, s * x1, s * x1, s * x0};
	const float ys[4] = {y0, y0, y1, y1};

	if (!q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);
	for (i = 0; i < 4; i++) {
		verts[i].xyz[0] = win->origin[0] + right[0] * xs[i] + up[0] * ys[i] + win->normal[0] * s * depth;
		verts[i].xyz[1] = win->origin[1] + right[1] * xs[i] + up[1] * ys[i] + win->normal[1] * s * depth;
		verts[i].xyz[2] = win->origin[2] + right[2] * xs[i] + up[2] * ys[i] + win->normal[2] * s * depth;
		verts[i].st[0] = 0.5f;
		verts[i].st[1] = 0.5f;
		verts[i].modulate.rgba[0] = r;
		verts[i].modulate.rgba[1] = g;
		verts[i].modulate.rgba[2] = b;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(q3ide_wm.border_shader, 4, verts, 1);
}

/* 4 strips forming a frame at depth along ±normal. Both sides rendered. */
static void q3ide_frame_strips(q3ide_win_t *win, float bw, float front_depth, float back_depth, byte r, byte g, byte b)
{
	float hw = win->world_w * 0.5f;
	float hh = win->world_h * 0.5f;
	/* Front face (toward normal) */
	q3ide_border_strip(win, -hw, -hh, -hw + bw, hh, front_depth, qfalse, r, g, b);
	q3ide_border_strip(win, hw - bw, -hh, hw, hh, front_depth, qfalse, r, g, b);
	q3ide_border_strip(win, -hw + bw, hh - bw, hw - bw, hh, front_depth, qfalse, r, g, b);
	q3ide_border_strip(win, -hw + bw, -hh, hw - bw, -hh + bw, front_depth, qfalse, r, g, b);
	/* Back face (away from normal) */
	q3ide_border_strip(win, -hw, -hh, -hw + bw, hh, back_depth, qtrue, r, g, b);
	q3ide_border_strip(win, hw - bw, -hh, hw, hh, back_depth, qtrue, r, g, b);
	q3ide_border_strip(win, -hw + bw, hh - bw, hw - bw, hh, back_depth, qtrue, r, g, b);
	q3ide_border_strip(win, -hw + bw, -hh, hw - bw, -hh + bw, back_depth, qtrue, r, g, b);
}

void q3ide_add_select_border(q3ide_win_t *win)
{
	q3ide_frame_strips(win, 2.0f, 0.5f, 0.5f, 255, 60, 0);
}

/* Red frame at window edges, visible from front and back. */
void q3ide_add_hover_border(q3ide_win_t *win)
{
	if (win->hover_t <= 0.0f)
		return;
	q3ide_frame_strips(win, 1.0f, 0.5f, 0.5f, 255, 0, 0);
}

void q3ide_add_poly(q3ide_win_t *win)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	float pop = 1.0f;
	int i;

	if (!win->shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);

	{
		float u0 = win->uv_x0, u1 = win->uv_x1;

		/* Front face */
		for (i = 0; i < 4; i++) {
			float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
			float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
			verts[i].xyz[0] = win->origin[0] + right[0] * sx * hw + up[0] * sy * hh + win->normal[0] * pop;
			verts[i].xyz[1] = win->origin[1] + right[1] * sx * hw + up[1] * sy * hh + win->normal[1] * pop;
			verts[i].xyz[2] = win->origin[2] + right[2] * sx * hw + up[2] * sy * hh + win->normal[2] * pop;
			verts[i].st[0] = u0 + (1.0f - (sx + 1.0f) * 0.5f) * (u1 - u0);
			verts[i].st[1] = 1.0f - (sy + 1.0f) * 0.5f;
			verts[i].modulate.rgba[0] = 255;
			verts[i].modulate.rgba[1] = 255;
			verts[i].modulate.rgba[2] = 255;
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
			verts[i].st[0] = u0 + (1.0f - (sx + 1.0f) * 0.5f) * (u1 - u0);
			verts[i].st[1] = 1.0f - (sy + 1.0f) * 0.5f;
			verts[i].modulate.rgba[0] = 255;
			verts[i].modulate.rgba[1] = 255;
			verts[i].modulate.rgba[2] = 255;
			verts[i].modulate.rgba[3] = 255;
		}
		re.AddPolyToScene(win->shader, 4, verts, 1);
	}
}

/* Portal frame and blood splat — q3ide_effects.c */
