/* q3ide_cmd_attach.c — Q3IDE_WM_CmdAttach: enumerate + attach all windows. */

#include "q3ide_wm.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

extern qboolean q3ide_is_attached(unsigned int id);

#define Q3IDE_ATTACH_WIN_W    100.0f /* default world width for attached windows */
#define Q3IDE_ATTACH_WIN_DIST 200.0f /* distance in front of player eye */

typedef struct {
	unsigned int id;
	float aspect;
	qboolean is_display;
	char label[128];
} q3ide_attach_item_t;

void Q3IDE_WM_CmdAttach(void)
{
	q3ide_attach_item_t items[Q3IDE_MAX_WIN];
	int item_n = 0;
	int i;
	vec3_t eye, fwd, pos, norm;
	float yaw_rad;

	if (!q3ide_wm.cap || !q3ide_wm.cap_list_wins) {
		Com_Printf("q3ide: not ready\n");
		return;
	}

	/* Collect all app windows — size-filtered */
	{
		Q3ideWindowList wlist = q3ide_wm.cap_list_wins(q3ide_wm.cap);
		if (wlist.windows && wlist.count) {
			for (i = 0; i < (int) wlist.count; i++) {
				const Q3ideWindowInfo *w = &wlist.windows[i];
				qboolean dupe = qfalse;
				if (item_n >= Q3IDE_MAX_WIN)
					break;
				if ((int) w->width < Q3IDE_MIN_WIN_W || (int) w->height < Q3IDE_MIN_WIN_H)
					continue;
				if ((w->title && strstr(w->title, "StatusIndicator")) ||
				    (w->app_name && strstr(w->app_name, "StatusIndicator")))
					continue;
				for (int j = 0; j < item_n; j++)
					if (items[j].id == w->window_id) {
						dupe = qtrue;
						break;
					}
				if (dupe)
					continue;
				items[item_n].id = w->window_id;
				items[item_n].aspect = w->height ? (float) w->width / w->height : 16.0f / 9.0f;
				items[item_n].is_display = qfalse;
				Q_strncpyz(items[item_n].label, (w->title && w->title[0]) ? w->title : (w->app_name ? w->app_name : ""),
				           sizeof(items[item_n].label));
				item_n++;
			}
		}
		if (q3ide_wm.cap_free_wlist)
			q3ide_wm.cap_free_wlist(wlist);
	}

	/* Collect displays */
	if (q3ide_wm.cap_list_disp && q3ide_wm.cap_start_disp) {
		Q3ideDisplayList dlist = q3ide_wm.cap_list_disp(q3ide_wm.cap);
		if (dlist.displays && dlist.count) {
			for (i = 0; i < (int) dlist.count && item_n < Q3IDE_MAX_WIN; i++) {
				const Q3ideDisplayInfo *d = &dlist.displays[i];
				items[item_n].id = d->display_id;
				items[item_n].aspect = d->height ? (float) d->width / d->height : 16.0f / 9.0f;
				items[item_n].is_display = qtrue;
				Q_strncpyz(items[item_n].label, va("Display %d", i + 1), sizeof(items[item_n].label));
				item_n++;
			}
		}
		if (q3ide_wm.cap_free_dlist)
			q3ide_wm.cap_free_dlist(dlist);
	}

	if (!item_n) {
		Com_Printf("q3ide: no windows or displays found\n");
		return;
	}

	/* Detach windows not in the new target set. */
	{
		int si, ki;
		for (si = 0; si < Q3IDE_MAX_WIN; si++) {
			qboolean keep = qfalse;
			if (!q3ide_wm.wins[si].active)
				continue;
			for (ki = 0; ki < item_n; ki++)
				if (items[ki].id == q3ide_wm.wins[si].capture_id) {
					keep = qtrue;
					break;
				}
			if (!keep)
				Q3IDE_WM_DetachById(q3ide_wm.wins[si].capture_id);
		}
	}

	Com_Printf("q3ide: attaching %d windows...\n", item_n);

	/* Spawn position: 200u in front of player, all windows stacked at same spot. */
	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw_rad = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0] = cosf(yaw_rad);
	fwd[1] = sinf(yaw_rad);
	fwd[2] = 0.0f;
	pos[0] = eye[0] + fwd[0] * Q3IDE_ATTACH_WIN_DIST;
	pos[1] = eye[1] + fwd[1] * Q3IDE_ATTACH_WIN_DIST;
	pos[2] = eye[2];
	norm[0] = -fwd[0];
	norm[1] = -fwd[1];
	norm[2] = 0.0f;

	for (i = 0; i < item_n; i++) {
		float ww = Q3IDE_ATTACH_WIN_W;
		float wh = (items[i].aspect > 0.001f) ? ww / items[i].aspect : ww * 9.0f / 16.0f;

		if (q3ide_is_attached(items[i].id)) {
			Q3IDE_WM_SetLabel(items[i].id, items[i].label);
			continue;
		}

		if (items[i].is_display) {
			if (!q3ide_wm.cap_start_disp || q3ide_wm.cap_start_disp(q3ide_wm.cap, items[i].id, Q3IDE_CAPTURE_FPS) != 0)
				continue;
			Q3IDE_WM_Attach(items[i].id, pos, norm, ww, wh, qfalse, qfalse);
		} else {
			Q3IDE_WM_Attach(items[i].id, pos, norm, ww, wh, qtrue, qfalse);
		}
		Q3IDE_WM_SetLabel(items[i].id, items[i].label);
	}

	q3ide_wm.auto_attach = qtrue;
	Com_Printf("q3ide: attaching ended, %d windows\n", q3ide_wm.num_active);
	Q3IDE_Eventf("attach_done", "\"attached\":%d,\"total\":%d", q3ide_wm.num_active, item_n);
}
