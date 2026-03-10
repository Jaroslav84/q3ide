/* q3ide_commands_desktop.c — Q3IDE_WM_CmdDesktop: mirror macOS displays onto game monitors. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

void Q3IDE_WM_CmdDesktop(void)
{
	Q3ideDisplayList dlist;
	Q3ideDisplayInfo sorted[Q3IDE_MAX_WIN];
	vec3_t eye, dir, wpos, wnorm, pos;
	float yaw, angle;
	int n_disp, n_mon, center, i, j, attached = 0;

	if (!q3ide_win_mngr.cap || !q3ide_win_mngr.cap_list_disp || !q3ide_win_mngr.cap_start_disp) {
		Q3IDE_LOGI("display capture not available");
		return;
	}
	Q3IDE_WM_CmdDetachAll();
	dlist = q3ide_win_mngr.cap_list_disp(q3ide_win_mngr.cap);
	if (!dlist.displays || !dlist.count) {
		Q3IDE_LOGI("no displays found");
		return;
	}
	n_disp = (int) dlist.count;
	if (n_disp > Q3IDE_MAX_WIN)
		n_disp = Q3IDE_MAX_WIN;
	for (i = 0; i < n_disp; i++)
		sorted[i] = dlist.displays[i];
	for (i = 0; i < n_disp - 1; i++) /* sort displays left→right by macOS x */
		for (j = i + 1; j < n_disp; j++)
			if (sorted[j].x < sorted[i].x) {
				Q3ideDisplayInfo t = sorted[i];
				sorted[i]         = sorted[j];
				sorted[j]         = t;
			}
	if (q3ide_win_mngr.cap_free_dlist)
		q3ide_win_mngr.cap_free_dlist(dlist);

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw    = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	n_mon  = Cvar_VariableIntegerValue("r_mmNumMon");
	if (n_mon < 1)
		n_mon = 1;
	center = n_mon / 2;
	angle  = Cvar_VariableValue("r_monitorAngle");

	for (i = 0; i < n_disp; i++) {
		Q3ideDisplayInfo *d  = &sorted[i];
		float yt = yaw + (float) (center - i) * angle * (float) M_PI / 180.0f;
		float oh = Q3IDE_WIN_INCHES;
		float ow = d->height ? oh * (float) d->width / (float) d->height : oh * 16.0f / 9.0f;
		dir[0]   = cosf(yt);
		dir[1]   = sinf(yt);
		dir[2]   = 0.0f;
		if (!Q3IDE_WM_TraceWall(eye, dir, wpos, wnorm)) {
			wpos[0]  = eye[0] + dir[0] * 512.0f;
			wpos[1]  = eye[1] + dir[1] * 512.0f;
			wpos[2]  = eye[2];
			wnorm[0] = -dir[0];
			wnorm[1] = -dir[1];
			wnorm[2] = 0.0f;
		}
		if (q3ide_win_mngr.cap_start_disp(q3ide_win_mngr.cap, d->display_id, Q3IDE_CAPTURE_FPS) != 0) {
			Q3IDE_LOGI("display %u start failed", d->display_id);
			continue;
		}
		VectorCopy(wpos, pos);
		pos[2] = eye[2];
		if (Q3IDE_WM_Attach(d->display_id, pos, wnorm, ow, oh, qfalse, qfalse)) {
			Q3IDE_WM_SetLabel(d->display_id, va("Display %d", i + 1));
			attached++;
		}
	}
	Q3IDE_LOGI("mirror: %d/%d display(s)", attached, n_disp);
}
