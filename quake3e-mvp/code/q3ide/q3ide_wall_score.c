/*
 * q3ide_wall_score.c — Wall scoring helpers: facing sort, TV-size scoring.
 * Room scan: q3ide_room_scan.c.  Room layout: q3ide_room_layout.c.
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

#define Q3IDE_WALL_OFFSET 3.0f

void q3ide_sort_walls_by_facing(q3ide_room_t *room, float yaw)
{
	float scores[Q3IDE_LAYOUT_MAX_WALLS];
	float fx = cosf(yaw), fy = sinf(yaw);
	int i;

	for (i = 0; i < room->n; i++)
		scores[i] = -(room->walls[i].normal[0] * fx + room->walls[i].normal[1] * fy);

	for (i = 1; i < room->n; i++) {
		q3ide_wall_t tmp = room->walls[i];
		float ts = scores[i];
		int j = i - 1;
		while (j >= 0 && scores[j] < ts) {
			room->walls[j + 1] = room->walls[j];
			scores[j + 1] = scores[j];
			j--;
		}
		room->walls[j + 1] = tmp;
		scores[j + 1] = ts;
	}
}

/* TV area score if `k` windows are placed on `wall`.
 * Returns the area (w*h) of each TV — larger is better.
 * Used by the size-maximizing greedy assignment. */
float q3ide_tv_score(const q3ide_wall_t *wall, int k, float aspect)
{
	const float margin = 24.0f, gap = 16.0f;
	float wall_h = wall->ceil_z - wall->floor_z;
	float usable = wall->width - 2.0f * margin;
	float u_per_win = (k > 1) ? (usable - (float) (k - 1) * gap) / (float) k : usable;
	float tv_h = wall_h * 0.85f;
	float tv_w;
	if (tv_h < 70.0f)
		tv_h = 70.0f;
	tv_w = tv_h * aspect;
	if (u_per_win > 0.0f && tv_w > u_per_win) {
		tv_w = u_per_win;
		tv_h = (aspect > 0.0f) ? tv_w / aspect : tv_w;
	}
	return tv_w * tv_h;
}

/* q3ide_room_layout — q3ide_layout_rooms.c */
