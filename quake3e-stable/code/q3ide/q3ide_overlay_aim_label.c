/*
 * q3ide_overlay_aim_label.c — Aimed window name label drawn on all monitors.
 * Centred at the top of each viewport when the crosshair is over a window.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <string.h>

extern int q3ide_aimed_win;

extern void q3ide_ovl_init(void);
extern qhandle_t g_ovl_chars;
extern void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r, byte g,
                          byte b);
extern void q3ide_ovl_pixel_pos(const refdef_t *fd, float px, float py, float out[3]);

void Q3IDE_DrawAimLabel(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	const char *lbl;
	int len;
	float total_w, pos[3], ox, oy, oz;
	const float rx[3] = {fd->viewaxis[1][0], fd->viewaxis[1][1], fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};

	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;
	if (q3ide_aimed_win < 0 || q3ide_aimed_win >= Q3IDE_MAX_WIN || !q3ide_wm.wins[q3ide_aimed_win].active)
		return;

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	lbl = q3ide_wm.wins[q3ide_aimed_win].label;
	if (!lbl[0])
		lbl = "(no label)";
	len = (int) strlen(lbl);
	total_w = (float) len * Q3IDE_OVL_CHAR_W;

	q3ide_ovl_pixel_pos(fd, (float) fd->width * 0.5f, Q3IDE_AIM_LABEL_TOP_PX, pos);
	ox = pos[0] - rx[0] * total_w * 0.5f;
	oy = pos[1] - rx[1] * total_w * 0.5f;
	oz = pos[2] - rx[2] * total_w * 0.5f;

	q3ide_ovl_str(ox, oy, oz, rx, ux, lbl, 255, 220, 0);
}
