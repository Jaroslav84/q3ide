/*
 * q3ide_wall_measure.c — Wall dimension measurement via CM_BoxTrace sweeps.
 */

#include "q3ide_layout.h"
#include "q3ide_aas.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

/* Placement queue — defined in q3ide_layout.c */
extern void q3ide_queue_push(unsigned int id, const vec3_t pos, const vec3_t normal, float ww, float wh,
                             qboolean is_display);

/* Minimum clearance from wall surface (after object probe). */
#define Q3IDE_WALL_OFFSET 3.0f

/* Compute horizontal right vector from a wall normal (inline from q3ide_geom.c) */
static void q3ide_layout_right(const vec3_t normal, vec3_t right)
{
	float nx = normal[0], ny = normal[1];
	float len = sqrtf(nx * nx + ny * ny);
	if (len > 0.01f) {
		right[0] = -ny / len;
		right[1] = nx / len;
		right[2] = 0.0f;
	} else {
		right[0] = 1.0f;
		right[1] = 0.0f;
		right[2] = 0.0f;
	}
}

/* Measure one wall: fills right, width, floor_z, ceil_z.
 * Width is measured at contact height (always on the wall surface).
 * floor_z/ceil_z are used only for TV size calculation. */
void q3ide_measure_wall(const vec3_t contact, const vec3_t normal, q3ide_wall_t *wall, const vec3_t eye)
{
	vec3_t start, end;
	static vec3_t mins = {0, 0, 0}, maxs = {0, 0, 0};
	float right_dist, left_dist, ceil_dist = 0, floor_dist = 0, wall_h;

	VectorCopy(contact, wall->contact);
	VectorCopy(normal, wall->normal);
	q3ide_layout_right(normal, wall->right);

	/* start: 2u off the wall at contact height (eye level).
	 * Measuring at contact[2] keeps the horizontal sweep truly horizontal so
	 * BSP corners block the LOS check properly — diagonal sweeps at 2/3 height
	 * slip past corner geometry and end up measuring into adjacent rooms. */
	start[0] = contact[0] + normal[0] * Q3IDE_WALL_OFFSET;
	start[1] = contact[1] + normal[1] * Q3IDE_WALL_OFFSET;
	start[2] = contact[2]; /* eye level — horizontal sweep */

	/* Floor / ceil: measured separately for TV sizing (not for width sweep). */
	{
		trace_t htr;
		vec3_t hend;
		vec3_t hstart;
		hstart[0] = start[0];
		hstart[1] = start[1];
		hstart[2] = contact[2];
		hend[0] = hstart[0];
		hend[1] = hstart[1];
		hend[2] = hstart[2] + 512.0f;
		CM_BoxTrace(&htr, hstart, hend, mins, maxs, 0, CONTENTS_SOLID, qfalse);
		wall->ceil_z = hstart[2] + htr.fraction * 512.0f;
		hend[2] = hstart[2] - 512.0f;
		CM_BoxTrace(&htr, hstart, hend, mins, maxs, 0, CONTENTS_SOLID, qfalse);
		wall->floor_z = hstart[2] - htr.fraction * 512.0f;
	}

	/* Width: find the continuous wall span in ±right.
	 * Step outward; stop when player can no longer see the position (corner) OR wall is gone. */
	{
		/* Small box for LOS: passes over very thin protrusions (< 4u) */
		static vec3_t lmins = {-4, -4, -4}, lmaxs = {4, 4, 4};
		const float step_sz = 16.0f;
		const float max_reach = 512.0f;
		float d, back_end[3], los_tgt[3];
		trace_t btr, los;

		right_dist = 0.0f;
		for (d = step_sz; d <= max_reach; d += step_sz) {
			end[0] = start[0] + wall->right[0] * d;
			end[1] = start[1] + wall->right[1] * d;
			end[2] = start[2];
			/* LOS from player eye — stops at real corners (not thin protrusions) */
			los_tgt[0] = end[0] + normal[0] * 4.0f;
			los_tgt[1] = end[1] + normal[1] * 4.0f;
			los_tgt[2] = end[2];
			CM_BoxTrace(&los, eye, los_tgt, lmins, lmaxs, 0, CONTENTS_SOLID, qfalse);
			if (!los.startsolid && los.fraction < 0.85f)
				break; /* corner hides this spot from player */
			/* Back-trace: confirm wall still exists here */
			back_end[0] = end[0] - normal[0] * 24.0f;
			back_end[1] = end[1] - normal[1] * 24.0f;
			back_end[2] = end[2];
			CM_BoxTrace(&btr, end, back_end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
			if (btr.fraction >= 1.0f)
				break; /* gap / doorway */
			right_dist = d;
		}

		left_dist = 0.0f;
		for (d = step_sz; d <= max_reach; d += step_sz) {
			end[0] = start[0] - wall->right[0] * d;
			end[1] = start[1] - wall->right[1] * d;
			end[2] = start[2];
			los_tgt[0] = end[0] + normal[0] * 4.0f;
			los_tgt[1] = end[1] + normal[1] * 4.0f;
			los_tgt[2] = end[2];
			CM_BoxTrace(&los, eye, los_tgt, lmins, lmaxs, 0, CONTENTS_SOLID, qfalse);
			if (!los.startsolid && los.fraction < 0.85f)
				break;
			back_end[0] = end[0] - normal[0] * 24.0f;
			back_end[1] = end[1] - normal[1] * 24.0f;
			back_end[2] = end[2];
			CM_BoxTrace(&btr, end, back_end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
			if (btr.fraction >= 1.0f)
				break;
			left_dist = d;
		}
	}

	wall->left_dist = left_dist;
	wall->width = right_dist + left_dist;
	if (wall->width < 64.0f)
		wall->width = 64.0f;
	if (wall->width > 2048.0f)
		wall->width = 2048.0f;

	/* floor_z/ceil_z already measured in the preamble above. Sanity check. */
	wall_h = wall->ceil_z - wall->floor_z;
	if (wall_h < 48.0f) {
		wall->floor_z = start[2] - 96.0f;
		wall->ceil_z = start[2] + 96.0f;
	}
	(void) ceil_dist;
	(void) floor_dist;
}

/* q3ide_room_scan — q3ide_layout_scan.c */
