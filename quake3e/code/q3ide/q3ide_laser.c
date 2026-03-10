/*
 * q3ide_laser.c — Laser lines from player eye to every active window.
 *
 * Approach: fat world-aligned horizontal ribbon (no camera math).
 * Black first (edge_shader slot 62) — confirmed working via chassis borders.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* q3ide_params singleton */
const q3ide_params_t q3ide_params = {
	.laserPointerWidth = 2.0f,
	.borderThickness   = 0.421875f,
	.windowDepth       = 1.0f,
	.accentColor       = {160, 0, 0},
};

void Q3IDE_DrawLasers(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	int i;

	extern qboolean q3ide_laser_active;
	if (!q3ide_laser_active)
		return;
	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;

	Q3IDE_LOGI("laser: edge_shader=%d active_wins=%d", q3ide_wm.edge_shader, q3ide_wm.num_active);

	if (!re.AddPolyToScene || !q3ide_wm.edge_shader)
		return;

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		polyVert_t v[4];
		float from[3], to[3], dx, dy, len, half_w = 8.0f;
		float sx, sy;
		int k;

		if (!q3ide_wm.wins[i].active)
			continue;

		from[0] = fd->vieworg[0];
		from[1] = fd->vieworg[1];
		from[2] = fd->vieworg[2];
		to[0]   = q3ide_wm.wins[i].origin[0];
		to[1]   = q3ide_wm.wins[i].origin[1];
		to[2]   = q3ide_wm.wins[i].origin[2];

		/* Horizontal side vector: perp to beam in XY plane */
		dx = to[0] - from[0];
		dy = to[1] - from[1];
		len = sqrtf(dx * dx + dy * dy);
		if (len < 1.0f)
			continue;
		sx = -dy / len * half_w;
		sy =  dx / len * half_w;

		v[0].xyz[0] = from[0] + sx; v[0].xyz[1] = from[1] + sy; v[0].xyz[2] = from[2];
		v[1].xyz[0] = from[0] - sx; v[1].xyz[1] = from[1] - sy; v[1].xyz[2] = from[2];
		v[2].xyz[0] = to[0]   - sx; v[2].xyz[1] = to[1]   - sy; v[2].xyz[2] = to[2];
		v[3].xyz[0] = to[0]   + sx; v[3].xyz[1] = to[1]   + sy; v[3].xyz[2] = to[2];

		v[0].st[0] = 0.0f; v[0].st[1] = 0.0f;
		v[1].st[0] = 1.0f; v[1].st[1] = 0.0f;
		v[2].st[0] = 1.0f; v[2].st[1] = 1.0f;
		v[3].st[0] = 0.0f; v[3].st[1] = 1.0f;

		for (k = 0; k < 4; k++) {
			v[k].modulate.rgba[0] = 255;
			v[k].modulate.rgba[1] = 255;
			v[k].modulate.rgba[2] = 255;
			v[k].modulate.rgba[3] = 255;
		}

		re.AddPolyToScene(q3ide_wm.edge_shader, 4, v, 1);
	}
}
