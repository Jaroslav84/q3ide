/* q3ide_cmd_attach.c — Q3IDE_WM_CmdAttach: enumerate + place all windows. */

#include "q3ide_wm.h"
#include "q3ide_layout.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

extern qboolean q3ide_is_attached(unsigned int id);

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
	vec3_t eye;

	if (!q3ide_wm.cap || !q3ide_wm.cap_list_wins) {
		Com_Printf("q3ide: not ready\n");
		return;
	}

	q3ide_layout_queue_reset();

	/* Collect all app windows — size-filtered, no app whitelist */
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
				Com_Printf("q3ide: app [%d] wid=%u \"%s\" %ux%u\n", item_n, w->window_id, w->app_name, w->width,
				           w->height);
				Q3IDE_Eventf("window_found", "\"wid\":%u,\"w\":%u,\"h\":%u", w->window_id, w->width, w->height);
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
				Com_Printf("q3ide: disp [%d] id=%u %ux%u\n", item_n, d->display_id, d->width, d->height);
				Q3IDE_Eventf("display_found", "\"id\":%u,\"w\":%u,\"h\":%u", d->display_id, d->width, d->height);
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

	/* Detach windows not in the new target set; survivors are moved in-place. */
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

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;

	{
		q3ide_room_t room;
		unsigned int ids[Q3IDE_MAX_WIN];
		float aspects[Q3IDE_MAX_WIN];
		int is_disp[Q3IDE_MAX_WIN];
		memset(ids, 0, sizeof(ids));
		memset(aspects, 0, sizeof(aspects));
		memset(is_disp, 0, sizeof(is_disp));
		q3ide_room_scan(eye, &room);
		if (room.n) {
			for (i = 0; i < item_n; i++) {
				ids[i] = items[i].id;
				aspects[i] = items[i].aspect;
				is_disp[i] = items[i].is_display ? 1 : 0;
			}
			q3ide_room_layout(&room, ids, aspects, is_disp, item_n);
		} else {
			Com_Printf("q3ide: no walls found\n");
		}

		for (i = 0; i < item_n; i++)
			Q3IDE_WM_SetLabel(items[i].id, items[i].label);
	}

	q3ide_wm.auto_attach = qtrue;
	Q3IDE_LOGI("attached %d/%d items (windows+displays)", q3ide_wm.num_active, item_n);
	Q3IDE_Eventf("attach_done", "\"attached\":%d,\"total\":%d", q3ide_wm.num_active, item_n);
}
