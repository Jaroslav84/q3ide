/*
 * q3ide_laser.c — Laser ribbons from player eye to every active window (hold K).
 *
 * Uses q3ide/laser shader: additive blend, $whiteimage, rgbGen vertex.
 * Ribbon is a horizontal quad (perpendicular to the beam in XY) so it
 * faces the player regardless of window orientation.
 */

#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>


void Q3IDE_DrawLasers(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *)refdef_ptr;
	int             i;

	extern qboolean q3ide_laser_active;
	if (!q3ide_laser_active)
		return;
	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;
	if (!re.AddPolyToScene || !q3ide_wm.laser_shader)
		return;

	/* Laser origin: player belly = feet + half viewheight. Same XY as eye. */
	{
	float belly_z = cl.snap.ps.origin[2] + cl.snap.ps.viewheight * 0.5f;

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		polyVert_t v[4];
		float      dx, dy, len, sx, sy;
		int        k;

		if (!q3ide_wm.wins[i].active)
			continue;

		dx  = q3ide_wm.wins[i].origin[0] - fd->vieworg[0];
		dy  = q3ide_wm.wins[i].origin[1] - fd->vieworg[1];
		len = sqrtf(dx * dx + dy * dy);
		if (len < 1.0f)
			continue;

		/* Side vector: perpendicular to beam direction in XY plane. */
		sx = -dy / len * Q3IDE_LASER_HALF_W;
		sy =  dx / len * Q3IDE_LASER_HALF_W;

		v[0].xyz[0] = fd->vieworg[0] + sx;             v[0].xyz[1] = fd->vieworg[1] + sy;             v[0].xyz[2] = belly_z;
		v[1].xyz[0] = fd->vieworg[0] - sx;             v[1].xyz[1] = fd->vieworg[1] - sy;             v[1].xyz[2] = belly_z;
		v[2].xyz[0] = q3ide_wm.wins[i].origin[0] - sx; v[2].xyz[1] = q3ide_wm.wins[i].origin[1] - sy; v[2].xyz[2] = q3ide_wm.wins[i].origin[2];
		v[3].xyz[0] = q3ide_wm.wins[i].origin[0] + sx; v[3].xyz[1] = q3ide_wm.wins[i].origin[1] + sy; v[3].xyz[2] = q3ide_wm.wins[i].origin[2];

		v[0].st[0] = 0.0f; v[0].st[1] = 0.0f;
		v[1].st[0] = 1.0f; v[1].st[1] = 0.0f;
		v[2].st[0] = 1.0f; v[2].st[1] = 1.0f;
		v[3].st[0] = 0.0f; v[3].st[1] = 1.0f;

		for (k = 0; k < 4; k++) {
			v[k].modulate.rgba[0] = q3ide_params.accentColor[0];
			v[k].modulate.rgba[1] = q3ide_params.accentColor[1];
			v[k].modulate.rgba[2] = q3ide_params.accentColor[2];
			v[k].modulate.rgba[3] = 255;
		}

		re.AddPolyToScene(q3ide_wm.laser_shader, 4, v, 1);
	}
	}
}
