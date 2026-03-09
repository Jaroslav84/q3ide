/*
 * q3ide_room_scan.c — q3ide_room_scan: AAS-driven + raycast wall discovery.
 * Wall measurement helpers: q3ide_wall_measure.c.
 */

#include "q3ide_layout.h"
#include "q3ide_aas.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Wall measurement — q3ide_layout_algo.c */
extern void q3ide_measure_wall(const vec3_t contact, const vec3_t normal, q3ide_wall_t *wall, const vec3_t eye);

void q3ide_room_scan(vec3_t eye, q3ide_room_t *out)
{
	q3ide_aas_wall_t aas_walls[Q3IDE_LAYOUT_MAX_WALLS];
	int n, i;

	out->n = 0;

	/* Primary path: AAS-derived exact wall geometry. */
	n = Q3IDE_AAS_GetAreaWalls(eye, aas_walls, Q3IDE_LAYOUT_MAX_WALLS);
	if (n > 0) {
		float radius = Cvar_VariableValue("q3ide_placement_radius");
		if (radius < 64.0f)
			radius = 1200.0f; /* default ~30m */
		for (i = 0; i < n && out->n < Q3IDE_LAYOUT_MAX_WALLS; i++) {
			q3ide_wall_t *w = &out->walls[out->n];
			vec3_t los_tgt;
			trace_t los_tr;
			static vec3_t lmins = {-4, -4, -4}, lmaxs = {4, 4, 4};
			{
				vec3_t _d;
				VectorSubtract(eye, aas_walls[i].centroid, _d);
				if (VectorLength(_d) > radius)
					continue;
			}
			los_tgt[0] = aas_walls[i].centroid[0] + aas_walls[i].normal[0] * 2.0f;
			los_tgt[1] = aas_walls[i].centroid[1] + aas_walls[i].normal[1] * 2.0f;
			los_tgt[2] = aas_walls[i].centroid[2];
			CM_BoxTrace(&los_tr, eye, los_tgt, lmins, lmaxs, 0, CONTENTS_SOLID, qfalse);
			if (!los_tr.startsolid && los_tr.fraction < 0.9f)
				continue; /* occluded */
			VectorCopy(aas_walls[i].centroid, w->contact);
			VectorCopy(aas_walls[i].normal, w->normal);
			VectorCopy(aas_walls[i].right, w->right);
			w->width = aas_walls[i].width;
			w->left_dist = aas_walls[i].left_dist;
			w->floor_z = aas_walls[i].floor_z;
			w->ceil_z = aas_walls[i].ceil_z;
			Q3IDE_LOGI("wall[%d] w=%.0f(L=%.0f R=%.0f) floor=%.0f ceil=%.0f"
			           " n=(%.2f,%.2f,%.2f)",
			           out->n, w->width, w->left_dist, w->width - w->left_dist, w->floor_z, w->ceil_z, w->normal[0],
			           w->normal[1], w->normal[2]);
			out->n++;
		}
		Com_Printf("q3ide: room scan (AAS) found %d wall(s)\n", out->n);
		return;
	}

	/* Fallback: raycast scan if AAS not loaded or area not found. */
	{
		int j;
		float yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
		for (i = 0; i < Q3IDE_LAYOUT_SCAN_DIRS && out->n < Q3IDE_LAYOUT_MAX_WALLS; i++) {
			vec3_t dir, wp, wn;
			qboolean dup = qfalse;
			float yt = yaw + (float) i * (2.0f * (float) M_PI / Q3IDE_LAYOUT_SCAN_DIRS);
			dir[0] = cosf(yt);
			dir[1] = sinf(yt);
			dir[2] = 0.0f;
			if (!Q3IDE_WM_TraceWall(eye, dir, wp, wn))
				continue;
			if (fabsf(wn[2]) > 0.4f)
				continue;
			for (j = 0; j < out->n; j++)
				if (DotProduct(wn, out->walls[j].normal) > 0.85f) {
					dup = qtrue;
					break;
				}
			if (dup)
				continue;
			q3ide_measure_wall(wp, wn, &out->walls[out->n], eye);
			out->n++;
		}
		Com_Printf("q3ide: room scan (raycast fallback) found %d wall(s)\n", out->n);
	}
}
