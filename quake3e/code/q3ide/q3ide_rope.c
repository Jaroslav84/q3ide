/*
 * q3ide_rope.c — Grapple rope rendering (type 1 / pendulum).
 * Laser beams: q3ide_laser.c.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_win_mngr.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

static qhandle_t g_rope_shader;

void Q3IDE_DrawGrappleRope(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	float hw = 0.75f; /* half of 1.5u rope width */
	vec3_t body, gp, beam_dir, to_cam, perp, up = {0.0f, 0.0f, 1.0f};
	float len;
	polyVert_t v[4];
	int i;

	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;
	if (!(cl.snap.ps.pm_flags & PMF_GRAPPLE_PULL))
		return;
	if (Cvar_VariableIntegerValue("g_grappleType") != 1)
		return;
	if (!re.AddPolyToScene)
		return;

	if (!g_rope_shader)
		g_rope_shader = re.RegisterShader("q3ide/laser");

	/* Player mid-body */
	body[0] = cl.snap.ps.origin[0];
	body[1] = cl.snap.ps.origin[1];
	body[2] = cl.snap.ps.origin[2] + (float) cl.snap.ps.viewheight * 0.5f;
	VectorCopy(cl.snap.ps.grapplePoint, gp);

	VectorSubtract(gp, body, beam_dir);
	len = VectorLength(beam_dir);
	if (len < 1.0f)
		return;
	VectorScale(beam_dir, 1.0f / len, beam_dir);

	VectorSubtract(fd->vieworg, body, to_cam);
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
	VectorScale(perp, hw / len, perp);

	VectorAdd(body, perp, v[0].xyz);
	VectorSubtract(body, perp, v[1].xyz);
	VectorSubtract(gp, perp, v[2].xyz);
	VectorAdd(gp, perp, v[3].xyz);
	v[0].st[0] = 0.0f;
	v[0].st[1] = 0.0f;
	v[1].st[0] = 1.0f;
	v[1].st[1] = 0.0f;
	v[2].st[0] = 1.0f;
	v[2].st[1] = 1.0f;
	v[3].st[0] = 0.0f;
	v[3].st[1] = 1.0f;
	for (i = 0; i < 4; i++) {
		v[i].modulate.rgba[0] = 200;
		v[i].modulate.rgba[1] = 140;
		v[i].modulate.rgba[2] = 60;
		v[i].modulate.rgba[3] = 240;
	}
	re.AddPolyToScene(g_rope_shader, 4, v, 1);
}
