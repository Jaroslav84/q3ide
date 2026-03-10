/*
 * q3ide_laser.c — Red laser beams from player to active windows.
 * Grapple rope rendering: q3ide_rope.c.
 *
 * Color comes from border_shader (slot 63, solid red texture).
 * Vertex colors must be white — rgbGen vertex reads them, not modulate.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

/* q3ide_params singleton — input/keyboard must never write to this. */
const q3ide_params_t q3ide_params = {
    .laserPointerWidth = 2.0f,
    .borderThickness   = 0.421875f,
    .windowDepth       = 1.0f,        /* TV chassis thickness — visible from the side */
    .accentColor       = {160, 0, 0}, /* Quake red — deep crimson */
};

/*
 * Draw a camera-facing ribbon from `from` to `to` using border_shader.
 * half_w: half-width of the ribbon in world units.
 * All vertex colors white — red comes from the solid-red scratch texture.
 */
static void q3ide_draw_beam(const vec3_t from, const vec3_t to, const vec3_t vieworg, float half_w)
{
	vec3_t beam_dir, mid, to_cam, perp, up = {0.0f, 0.0f, 1.0f};
	float len;
	polyVert_t v[4];
	int i;

	if (!q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

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
		CrossProduct(beam_dir, up, perp);
		len = VectorLength(perp);
		if (len < 0.1f) {
			vec3_t right = {1.0f, 0.0f, 0.0f};
			CrossProduct(beam_dir, right, perp);
			len = VectorLength(perp);
		}
	}
	if (len < 0.01f)
		return;
	VectorScale(perp, half_w / len, perp);

	VectorAdd(from, perp, v[0].xyz);
	VectorSubtract(from, perp, v[1].xyz);
	VectorSubtract(to, perp, v[2].xyz);
	VectorAdd(to, perp, v[3].xyz);

	v[0].st[0] = 0.0f; v[0].st[1] = 0.0f;
	v[1].st[0] = 1.0f; v[1].st[1] = 0.0f;
	v[2].st[0] = 1.0f; v[2].st[1] = 1.0f;
	v[3].st[0] = 0.0f; v[3].st[1] = 1.0f;

	/* White verts — color comes from the solid-red scratch texture, not vertex */
	for (i = 0; i < 4; i++) {
		v[i].modulate.rgba[0] = 255;
		v[i].modulate.rgba[1] = 255;
		v[i].modulate.rgba[2] = 255;
		v[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(q3ide_wm.border_shader, 4, v, 1);
}

void Q3IDE_DrawLasers(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	vec3_t fwd, beam_end;
	int hit, i;

	extern qboolean q3ide_laser_active;
	if (!q3ide_laser_active)
		return;
	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;
	if (!re.AddPolyToScene)
		return;

	/* Crosshair ray from camera origin along camera forward axis. */
	fwd[0] = fd->viewaxis[0][0];
	fwd[1] = fd->viewaxis[0][1];
	fwd[2] = fd->viewaxis[0][2];

	hit = Q3IDE_WM_TraceWindowHit(fd->vieworg, fwd, -1);
	if (hit >= 0) {
		/* Thick beam to the aimed-at window. */
		q3ide_draw_beam(fd->vieworg, q3ide_wm.wins[hit].origin, fd->vieworg, q3ide_params.laserPointerWidth);
	} else {
		/* No window in sights — extend 2000u. */
		beam_end[0] = fd->vieworg[0] + fwd[0] * 2000.0f;
		beam_end[1] = fd->vieworg[1] + fwd[1] * 2000.0f;
		beam_end[2] = fd->vieworg[2] + fwd[2] * 2000.0f;
		q3ide_draw_beam(fd->vieworg, beam_end, fd->vieworg, q3ide_params.laserPointerWidth * 0.5f);
	}

	/* Thin spokes to every other active window so user can see them all. */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		if (!q3ide_wm.wins[i].active || i == hit)
			continue;
		q3ide_draw_beam(fd->vieworg, q3ide_wm.wins[i].origin, fd->vieworg, q3ide_params.laserPointerWidth * 0.25f);
	}
}
