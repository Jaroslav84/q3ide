/*
 * q3ide_render.c — Multi-monitor viewport rendering.
 *
 * Q3IDE_MultiMonitorRender() replaces the single re.RenderScene() call
 * in cl_cgame.c when r_multiMonitor is enabled.
 *
 * For each display detected by sdl_glimp.c, it renders a sub-viewport
 * of the spanning window with a yaw angle offset matching the physical
 * angle between monitors.
 *
 * Monitor layout cvars (set by sdl_glimp.c):
 *   r_mmNumMon     — number of monitors
 *   r_mmMonX<i>    — window-relative x of monitor i (pixels)
 *   r_mmMonW<i>    — width of monitor i (pixels)
 *   r_mmMonH<i>    — height of monitor i (pixels)
 *   r_monitorAngle — physical angle (degrees) between adjacent monitors
 */

#include "../qcommon/q_shared.h"
#include "q3ide_engine_hooks.h"
#include "q3ide_win_mngr.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/*
 * Sort monitors left-to-right by window-relative x so we can assign
 * yaw offsets correctly (leftmost = most positive yaw, rightmost = most negative).
 */
static void q3ide_sorted_monitors(int n, int *sorted)
{
	int i, j;
	for (i = 0; i < n; i++)
		sorted[i] = i;
	for (i = 0; i < n - 1; i++) {
		for (j = i + 1; j < n; j++) {
			int xi = Cvar_VariableIntegerValue(va("r_mmMonX%d", sorted[i]));
			int xj = Cvar_VariableIntegerValue(va("r_mmMonX%d", sorted[j]));
			if (xj < xi) {
				int tmp = sorted[i];
				sorted[i] = sorted[j];
				sorted[j] = tmp;
			}
		}
	}
}

void Q3IDE_MultiMonitorRender(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	int n, i, sorted[16];
	float angle;
	int center;

	n = Cvar_VariableIntegerValue("r_mmNumMon");
	if (n <= 1) {
		if (!(fd->rdflags & RDF_NOWORLDMODEL)) {
			Q3IDE_WM_AddPolys();
			Q3IDE_DrawLasers(fd);
			Q3IDE_DrawGrappleRope(fd);
		}
		re.RenderScene(fd);
		return;
	}

	/* HUD icon scenes (health cross, armor, face portrait) use RDF_NOWORLDMODEL
	 * with a tiny viewport positioned in cgame's 0..centerW space.
	 * Shift x to center monitor and pass through as a single scene. */
	if (fd->rdflags & RDF_NOWORLDMODEL) {
		refdef_t icon = *fd;
		icon.x += Cvar_VariableIntegerValue("r_mmCenterX");
		re.RenderScene(&icon);
		return;
	}
	if (n > 16)
		n = 16;

	angle = Cvar_VariableValue("r_monitorAngle");
	center = n / 2; /* middle monitor index in sorted order */

	q3ide_sorted_monitors(n, sorted);

	for (i = 0; i < n; i++) {
		refdef_t view = *fd;
		int idx = sorted[i];
		int mon_x = Cvar_VariableIntegerValue(va("r_mmMonX%d", idx));
		int mon_w = Cvar_VariableIntegerValue(va("r_mmMonW%d", idx));
		int mon_h = Cvar_VariableIntegerValue(va("r_mmMonH%d", idx));
		float yaw_offset = (float) (center - i) * angle;

		view.x = mon_x;
		view.y = 0;
		view.width = mon_w;
		view.height = mon_h;
		view.fov_x = 90.0f;
		view.fov_y = 2.0f * RAD2DEG(atanf(tanf(DEG2RAD(45.0f)) * (float) mon_h / (float) mon_w));

		/* Rotate view axis around Z by yaw_offset degrees */
		if (yaw_offset != 0.0f) {
			float rad = yaw_offset * (float) M_PI / 180.0f;
			float c = cosf(rad), s = sinf(rad);
			int a;
			for (a = 0; a < 3; a++) {
				float ax = view.viewaxis[a][0];
				float ay = view.viewaxis[a][1];
				view.viewaxis[a][0] = ax * c - ay * s;
				view.viewaxis[a][1] = ax * s + ay * c;
			}
		}

		/* Tell RE_RenderScene how many passes remain so it preserves entities. */
		Cvar_Set("r_multiViewRemaining", va("%d", n - i - 1));
		Q3IDE_WM_AddPolys();
		Q3IDE_DrawLasers(&view);
		Q3IDE_DrawGrappleRope(&view);
		/* Left monitor (sorted[0]): draw keybinding cheat sheet overlay */
		if (i == 0)
			Q3IDE_DrawLeftOverlay(&view);
		re.RenderScene(&view);
	}
}
