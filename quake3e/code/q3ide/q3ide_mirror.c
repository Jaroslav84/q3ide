/*
 * q3ide_mirror.c — Mirror portal rendering and placement API.
 * Window scene rendering: q3ide_scene.c.  Frame polling: q3ide_poll.c.
 */

#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "../renderercommon/tr_public.h"
#include <math.h>

static void q3ide_wm_render_mirror(void)
{
	polyVert_t verts[4];
	vec3_t right;
	float hw, hh, nx, ny, len;
	int i;

	if (!q3ide_wm.mirror_active)
		return;
	if (!q3ide_wm.mirror_energy_shader && re.RegisterShader)
		q3ide_wm.mirror_energy_shader = re.RegisterShader("models/mapobjects/teleporter/energy");
	if (!q3ide_wm.mirror_energy_shader || !re.AddPolyToScene)
		return;

	nx = q3ide_wm.mirror_normal[0];
	ny = q3ide_wm.mirror_normal[1];
	len = sqrtf(nx * nx + ny * ny);
	if (len > 0.01f) {
		right[0] = -ny / len;
		right[1] = nx / len;
		right[2] = 0.0f;
	} else {
		right[0] = 1.0f;
		right[1] = 0.0f;
		right[2] = 0.0f;
	}

	hw = q3ide_wm.mirror_w * 0.5f;
	hh = q3ide_wm.mirror_h * 0.5f;

	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = q3ide_wm.mirror_origin[0] + right[0] * sx * hw;
		verts[i].xyz[1] = q3ide_wm.mirror_origin[1] + right[1] * sx * hw;
		verts[i].xyz[2] = q3ide_wm.mirror_origin[2] + sy * hh;
		verts[i].st[0] = (sx + 1.0f) * 0.5f;
		verts[i].st[1] = (sy + 1.0f) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = 255;
		verts[i].modulate.rgba[2] = 255;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(q3ide_wm.mirror_energy_shader, 4, verts, 1);
	{
		polyVert_t rv[4];
		rv[0] = verts[3];
		rv[1] = verts[2];
		rv[2] = verts[1];
		rv[3] = verts[0];
		re.AddPolyToScene(q3ide_wm.mirror_energy_shader, 4, rv, 1);
	}
}

static void q3ide_wm_render_mirror2(void)
{
	polyVert_t verts[4];
	vec3_t right;
	float hw, hh, nx, ny, len;
	int i;

	if (!q3ide_wm.mirror2_active)
		return;
	if (!q3ide_wm.mirror_energy_shader && re.RegisterShader)
		q3ide_wm.mirror_energy_shader = re.RegisterShader("models/mapobjects/teleporter/energy");
	if (!q3ide_wm.mirror_energy_shader || !re.AddPolyToScene)
		return;

	nx = q3ide_wm.mirror2_normal[0];
	ny = q3ide_wm.mirror2_normal[1];
	len = sqrtf(nx * nx + ny * ny);
	if (len > 0.01f) {
		right[0] = -ny / len;
		right[1] = nx / len;
		right[2] = 0.0f;
	} else {
		right[0] = 1.0f;
		right[1] = 0.0f;
		right[2] = 0.0f;
	}

	hw = q3ide_wm.mirror2_w * 0.5f;
	hh = q3ide_wm.mirror2_h * 0.5f;

	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = q3ide_wm.mirror2_origin[0] + right[0] * sx * hw;
		verts[i].xyz[1] = q3ide_wm.mirror2_origin[1] + right[1] * sx * hw;
		verts[i].xyz[2] = q3ide_wm.mirror2_origin[2] + sy * hh;
		verts[i].st[0] = (sx + 1.0f) * 0.5f;
		verts[i].st[1] = (sy + 1.0f) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = 255;
		verts[i].modulate.rgba[2] = 255;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(q3ide_wm.mirror_energy_shader, 4, verts, 1);
	{
		polyVert_t rv[4];
		rv[0] = verts[3];
		rv[1] = verts[2];
		rv[2] = verts[1];
		rv[3] = verts[0];
		re.AddPolyToScene(q3ide_wm.mirror_energy_shader, 4, rv, 1);
	}
}

/* Called from q3ide_wm_cmds.c before window polys */
void q3ide_wm_render_mirrors(void)
{
	q3ide_wm_render_mirror();
	q3ide_wm_render_mirror2();
}

/* ── Mirror placement API ─────────────────────────────────────── */

void Q3IDE_WM_PlaceMirror(vec3_t origin, vec3_t normal, float ww, float wh)
{
	VectorCopy(origin, q3ide_wm.mirror_origin);
	VectorCopy(normal, q3ide_wm.mirror_normal);
	q3ide_wm.mirror_w = ww;
	q3ide_wm.mirror_h = wh;
	q3ide_wm.mirror_active = qtrue;
}

void Q3IDE_WM_ClearMirror(void)
{
	q3ide_wm.mirror_active = qfalse;
}

qboolean Q3IDE_WM_MirrorActive(void)
{
	return q3ide_wm.mirror_active;
}

void Q3IDE_WM_GetMirrorOrigin(vec3_t out_origin, vec3_t out_normal, float *out_w, float *out_h)
{
	VectorCopy(q3ide_wm.mirror_origin, out_origin);
	VectorCopy(q3ide_wm.mirror_normal, out_normal);
	if (out_w)
		*out_w = q3ide_wm.mirror_w;
	if (out_h)
		*out_h = q3ide_wm.mirror_h;
}

void Q3IDE_WM_PlaceMirror2(vec3_t origin, vec3_t normal, float ww, float wh)
{
	VectorCopy(origin, q3ide_wm.mirror2_origin);
	VectorCopy(normal, q3ide_wm.mirror2_normal);
	q3ide_wm.mirror2_w = ww;
	q3ide_wm.mirror2_h = wh;
	q3ide_wm.mirror2_active = qtrue;
}

void Q3IDE_WM_ClearMirror2(void)
{
	q3ide_wm.mirror2_active = qfalse;
}

qboolean Q3IDE_WM_Mirror2Active(void)
{
	return q3ide_wm.mirror2_active;
}

void Q3IDE_WM_GetMirror2Origin(vec3_t out_origin, vec3_t out_normal, float *out_w, float *out_h)
{
	VectorCopy(q3ide_wm.mirror2_origin, out_origin);
	VectorCopy(q3ide_wm.mirror2_normal, out_normal);
	if (out_w)
		*out_w = q3ide_wm.mirror2_w;
	if (out_h)
		*out_h = q3ide_wm.mirror2_h;
}
