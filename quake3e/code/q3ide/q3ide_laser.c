/*
 * q3ide_laser.c — Red laser beams from player to all active windows.
 *
 * Hold K → q3ide_laser_active set to 1.
 * Release K → cleared.
 * Q3IDE_DrawLasers() draws two crossed billboard quads per beam so they are
 * visible from any angle, including head-on from the camera.
 * Beams start 30u in front of the eye (not at the exact eye position) so the
 * ribbon is not degenerate when viewed from the camera.
 */

#include "q3ide_hooks.h"
#include "q3ide_params.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

/* q3ide_params singleton — input/keyboard must never write to this. */
const q3ide_params_t q3ide_params = {
    .laserPointerWidth = 2.0f, /* ~2 px at typical viewing distance */
};

static qhandle_t g_laser_shader;

static void q3ide_laser_shader_init(void)
{
	if (!g_laser_shader)
		g_laser_shader = re.RegisterShader("q3ide/laser");
}

/*
 * Draw a camera-facing ribbon from `from` to `to`.
 * vieworg is used to compute a billboard perp so the ribbon always faces the
 * camera regardless of beam direction.
 */
static void q3ide_draw_beam(const vec3_t from, const vec3_t to, const vec3_t vieworg, byte r, byte g, byte b, byte a)
{
	vec3_t beam_dir, mid, to_cam, perp, perp2, up = {0.0f, 0.0f, 1.0f};
	float len;

	VectorSubtract(to, from, beam_dir);
	len = VectorLength(beam_dir);
	if (len < 1.0f)
		return;
	VectorScale(beam_dir, 1.0f / len, beam_dir);

	/* Camera-facing perp: cross(beam_dir, dir_from_mid_to_cam) */
	mid[0] = (from[0] + to[0]) * 0.5f;
	mid[1] = (from[1] + to[1]) * 0.5f;
	mid[2] = (from[2] + to[2]) * 0.5f;
	VectorSubtract(vieworg, mid, to_cam);
	len = VectorLength(to_cam);
	if (len > 0.1f)
		VectorScale(to_cam, 1.0f / len, to_cam);
	else
		VectorCopy(up, to_cam);

	CrossProduct(beam_dir, to_cam, perp);
	len = VectorLength(perp);
	if (len < 0.1f) {
		/* Beam is pointing directly at camera — fall back to Z cross */
		CrossProduct(beam_dir, up, perp);
		len = VectorLength(perp);
		if (len < 0.1f) {
			vec3_t right = {1.0f, 0.0f, 0.0f};
			CrossProduct(beam_dir, right, perp);
			len = VectorLength(perp);
		}
	}
	VectorScale(perp, (q3ide_params.laserPointerWidth * 0.5f) / len, perp);

	/* Second ribbon perpendicular to first (cross shape for full visibility) */
	CrossProduct(beam_dir, perp, perp2);
	len = VectorLength(perp2);
	if (len > 0.1f)
		VectorScale(perp2, (q3ide_params.laserPointerWidth * 0.5f) / len, perp2);

	/* Helper: emit one ribbon quad with given offset vector */
	{
		int q, i;
		polyVert_t v[4];
		for (q = 0; q < 2; q++) {
			vec3_t *pv = (q == 0) ? &perp : &perp2;
			if (VectorLength(*pv) < 0.01f)
				continue;
			VectorAdd(from, *pv, v[0].xyz);
			VectorSubtract(from, *pv, v[1].xyz);
			VectorSubtract(to, *pv, v[2].xyz);
			VectorAdd(to, *pv, v[3].xyz);

			v[0].st[0] = 0.0f;
			v[0].st[1] = 0.0f;
			v[1].st[0] = 1.0f;
			v[1].st[1] = 0.0f;
			v[2].st[0] = 1.0f;
			v[2].st[1] = 1.0f;
			v[3].st[0] = 0.0f;
			v[3].st[1] = 1.0f;

			for (i = 0; i < 4; i++) {
				v[i].modulate.rgba[0] = r;
				v[i].modulate.rgba[1] = g;
				v[i].modulate.rgba[2] = b;
				v[i].modulate.rgba[3] = a;
			}
			re.AddPolyToScene(g_laser_shader, 4, v, 1);
		}
	}
}

void Q3IDE_DrawLasers(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	vec3_t beam_start;
	int i;

	extern qboolean q3ide_laser_active;
	if (!q3ide_laser_active)
		return;
	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;

	q3ide_laser_shader_init();
	if (!g_laser_shader || !re.AddPolyToScene)
		return;

	/* Start beams from the player's heart (body center, ~40u above feet). */
	beam_start[0] = cl.snap.ps.origin[0];
	beam_start[1] = cl.snap.ps.origin[1];
	beam_start[2] = cl.snap.ps.origin[2] + 24.0f; /* +0.5m up */

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active)
			continue;
		q3ide_draw_beam(beam_start, w->origin, fd->vieworg, 255, 20, 20, 220);
	}
}
