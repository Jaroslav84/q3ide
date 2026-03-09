/* q3ide_cmd_reflow.c — Q3IDE_WM_Reflow: reposition all active windows. */

#include "q3ide_wm.h"
#include "q3ide_layout.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <string.h>

/* Reflow queue push — defined in q3ide_cmd.c */
extern void q3ide_reflow_push(int slot, const vec3_t pos, const vec3_t normal, float ww, float wh);

void Q3IDE_WM_Reflow(void)
{
	vec3_t eye;
	int i, nw, wi;
	int win_idx[Q3IDE_MAX_WIN];
	q3ide_room_t room;
	int assigned[Q3IDE_LAYOUT_MAX_WALLS][Q3IDE_MAX_WIN];
	int assigned_cnt[Q3IDE_LAYOUT_MAX_WALLS];
	float wall_roff[Q3IDE_LAYOUT_MAX_WALLS][Q3IDE_MAX_WIN];
	float wall_hw[Q3IDE_LAYOUT_MAX_WALLS][Q3IDE_MAX_WIN];
	const float margin = 24.0f, gap = 16.0f;
	int queued = 0;

	if (!q3ide_wm.num_active)
		return;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;

	nw = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active)
			win_idx[nw++] = i;
	if (!nw)
		return;

	q3ide_room_scan(eye, &room);
	if (!room.n)
		return;

	/* Optimal greedy: assign each window to the wall giving the largest TV */
	{
		int max_slots[Q3IDE_LAYOUT_MAX_WALLS];
		memset(max_slots, 0, sizeof(max_slots));
		for (wi = 0; wi < room.n; wi++) {
			float wall_h = room.walls[wi].ceil_z - room.walls[wi].floor_z;
			float usable = room.walls[wi].width - 2.0f * margin;
			if (wall_h < 96.0f || usable < 48.0f)
				continue;
			max_slots[wi] = (int) (usable / (70.0f + gap));
			if (max_slots[wi] < 1)
				max_slots[wi] = 1;
		}

		memset(assigned_cnt, 0, sizeof(assigned_cnt));
		for (i = 0; i < nw; i++) {
			int slot = win_idx[i];
			const q3ide_win_t *win = &q3ide_wm.wins[slot];
			float asp = (win->world_h > 0.0f) ? win->world_w / win->world_h : 16.0f / 9.0f;
			int best = -1;
			float best_score = -1.0f;
			for (wi = 0; wi < room.n; wi++) {
				float wall_h, usable, u_per_win, tv_h, tv_w, score;
				if (assigned_cnt[wi] >= max_slots[wi])
					continue;
				wall_h = room.walls[wi].ceil_z - room.walls[wi].floor_z;
				usable = room.walls[wi].width - 2.0f * margin;
				u_per_win = (assigned_cnt[wi] > 0)
				                ? (usable - (float) assigned_cnt[wi] * gap) / (float) (assigned_cnt[wi] + 1)
				                : usable;
				tv_h = wall_h * 0.85f;
				if (tv_h < 70.0f)
					tv_h = 70.0f;
				tv_w = tv_h * asp;
				if (u_per_win > 0.0f && tv_w > u_per_win) {
					tv_w = u_per_win;
					tv_h = (asp > 0.0f) ? tv_w / asp : tv_w;
				}
				score = tv_w * tv_h;
				if (score > best_score) {
					best_score = score;
					best = wi;
				}
			}
			if (best < 0)
				break;
			assigned[best][assigned_cnt[best]++] = slot;
		}
	}

	memset(wall_roff, 0, sizeof(wall_roff));
	memset(wall_hw, 0, sizeof(wall_hw));

	for (wi = 0; wi < room.n; wi++) {
		const q3ide_wall_t *wall = &room.walls[wi];
		int cnt = assigned_cnt[wi];
		float wall_h = wall->ceil_z - wall->floor_z;
		float center_z = (wall->floor_z + wall->ceil_z) * 0.5f;
		float usable = wall->width - 2.0f * margin;
		float right_dist = wall->width - wall->left_dist;
		int k;

		if (!cnt)
			continue;

		for (k = 0; k < cnt; k++) {
			int slot = assigned[wi][k];
			const q3ide_win_t *win = &q3ide_wm.wins[slot];
			float aspect = (win->world_h > 0.0f) ? win->world_w / win->world_h : 16.0f / 9.0f;
			float tv_h = wall_h * 0.85f;
			float tv_w, max_slot_w, roff;
			vec3_t pos;

			if (tv_h < 70.0f)
				tv_h = 70.0f;
			tv_w = tv_h * aspect;

			max_slot_w = (usable - (float) (cnt - 1) * gap) / (float) cnt;
			if (tv_w > max_slot_w && max_slot_w > 0.0f) {
				tv_w = max_slot_w;
				tv_h = tv_w / aspect;
				if (tv_h < 70.0f) {
					tv_h = 70.0f;
					tv_w = tv_h * aspect;
				}
			}

			{
				float step = usable / (float) cnt;
				roff = -wall->left_dist + margin + ((float) k + 0.5f) * step;
			}
			{
				float min_roff = -wall->left_dist + margin + tv_w * 0.5f;
				float max_roff = right_dist - margin - tv_w * 0.5f;
				if (max_roff < min_roff)
					max_roff = min_roff;
				if (roff < min_roff)
					roff = min_roff;
				if (roff > max_roff)
					roff = max_roff;
			}

			wall_roff[wi][k] = roff;
			wall_hw[wi][k] = tv_w * 0.5f;

			pos[0] = wall->contact[0] + wall->right[0] * roff + wall->normal[0] * 4.0f;
			pos[1] = wall->contact[1] + wall->right[1] * roff + wall->normal[1] * 4.0f;
			pos[2] = center_z;

			q3ide_reflow_push(slot, pos, wall->normal, tv_w, tv_h);
			Q3IDE_LOGI("reflow: queued slot=%d id=%u size=%.0fx%.0f roff=%.0f pos=(%.0f,%.0f,%.0f)", slot,
			           win->capture_id, tv_w, tv_h, roff, pos[0], pos[1], pos[2]);
			queued++;
		}
	}
	Q3IDE_LOGI("reflow: queued %d/%d window(s)", queued, nw);
}
