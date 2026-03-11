/*
 * q3ide_overlay_keyboard.c — Left overlay frame draw + glyph cache owner.
 *
 * Owns g_glyphs[]; q3ide_overlay_kbcache.c fills it at most 2 Hz.
 * Replay is cheap: transforms each cached glyph and calls AddPolyToScene.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"

/* Minimal types shared with q3ide_overlay_kbcache.c — full tables in overlay_keys.h */
#define OVL_MAX_GLYPHS 1024
typedef struct {
	float right, up;
	int ch;
	byte r, g, b;
	byte small;  /* 0=normal, 1=small scale */
	byte rotate; /* 0=none, 1=-45° */
} ovl_rel_glyph_t;

/* Primitives from q3ide_overlay.c */
extern qhandle_t g_ovl_chars;
extern void q3ide_ovl_init(void);
extern void q3ide_ovl_char(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                           byte b);
extern void q3ide_ovl_char_sm(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                              byte b);
extern void q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                             byte g, byte b);

/* q3ide_overlay_winlist.c */
extern void Q3IDE_DrawAreaLabel(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot);
extern void Q3IDE_DrawWinList(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot,
                              unsigned long long now);

/* q3ide_overlay_kbcache.c */
extern void q3ide_rebuild_keyboard_cache(void);

/* ── Panel anchor (world-unit offsets from camera forward axis) ─────── */
#define ANCHOR_RIGHT (-Q3IDE_OVL_DIST * 0.88f - 1.0f) /* far-left, shifted 50px left  */
#define ANCHOR_UP    (Q3IDE_OVL_DIST * 0.46f)          /* shifted 50px down from before */

/* ── Notification corner: top-right of left screen, left-aligned text ── */
/* Offset rightward from anchor to the inner edge of the left monitor.    */
#define NOTIF_RIGHT_OFFSET 5.5f

/* ── Glyph cache storage — written by kbcache.c, read here ─────────── */
ovl_rel_glyph_t g_glyphs[OVL_MAX_GLYPHS];
int g_glyph_count;
qboolean g_ovl_dirty = qtrue;
unsigned long long g_ovl_last_build_ms = 0;

float g_kb_bot; /* keyboard bottom offset from anchor — set by kbcache.c */

/* ── Main draw (called every frame from Q3IDE_Frame) ────────────────── */
void Q3IDE_DrawLeftOverlay(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	const float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	float ox, oy, oz;
	unsigned long long now;
	int gi;

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	ox = fd->vieworg[0] + fd->viewaxis[0][0] * Q3IDE_OVL_DIST + rx[0] * ANCHOR_RIGHT + fd->viewaxis[2][0] * ANCHOR_UP;
	oy = fd->vieworg[1] + fd->viewaxis[0][1] * Q3IDE_OVL_DIST + rx[1] * ANCHOR_RIGHT + fd->viewaxis[2][1] * ANCHOR_UP;
	oz = fd->vieworg[2] + fd->viewaxis[0][2] * Q3IDE_OVL_DIST + rx[2] * ANCHOR_RIGHT + fd->viewaxis[2][2] * ANCHOR_UP;

	/* ── Notifications: top-right corner of left screen, stacking down ── */
	{
		/* Base: anchor shifted right to the inner edge of the left monitor */
		float nx = ox + rx[0] * NOTIF_RIGHT_OFFSET;
		float ny = oy + rx[1] * NOTIF_RIGHT_OFFSET;
		float nz = oz + rx[2] * NOTIF_RIGHT_OFFSET;
		float nl = Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE; /* line step down */
		int nrow = 0;

		/* PAUSED banner */
		if (q3ide_wm.streams_paused) {
			float lx = nx + ux[0] * (-nl * nrow);
			float ly = ny + ux[1] * (-nl * nrow);
			float lz = nz + ux[2] * (-nl * nrow);
			q3ide_ovl_str_sm(lx, ly, lz, rx, ux, "PAUSED", 255, 170, 30);
			nrow++;
		}
		/* HIDDEN banner */
		if (q3ide_wm.wins_hidden) {
			float lx = nx + ux[0] * (-nl * nrow);
			float ly = ny + ux[1] * (-nl * nrow);
			float lz = nz + ux[2] * (-nl * nrow);
			q3ide_ovl_str_sm(lx, ly, lz, rx, ux, "HIDDEN", 120, 120, 255);
			nrow++;
		}
		(void) nrow;
	}

	/* Rate-limited cache rebuild */
	now = (unsigned long long) Sys_Milliseconds();
	if (g_ovl_dirty || (now - g_ovl_last_build_ms) >= OVL_REBUILD_MS) {
		q3ide_rebuild_keyboard_cache();
		g_ovl_last_build_ms = now;
		g_ovl_dirty = qfalse;
	}

	/* Replay glyph cache */
	for (gi = 0; gi < g_glyph_count; gi++) {
		const ovl_rel_glyph_t *gp = &g_glyphs[gi];
		float cx = ox + rx[0] * gp->right + ux[0] * gp->up;
		float cy = oy + rx[1] * gp->right + ux[1] * gp->up;
		float cz = oz + rx[2] * gp->right + ux[2] * gp->up;
		if (gp->rotate) {
			float rot_rx[3], rot_ux[3];
			int k;
			for (k = 0; k < 3; k++) {
				rot_rx[k] = rx[k] * 0.7071f + ux[k] * 0.7071f;
				rot_ux[k] = -rx[k] * 0.7071f + ux[k] * 0.7071f;
			}
			q3ide_ovl_char_sm(cx, cy, cz, rot_rx, rot_ux, gp->ch, gp->r, gp->g, gp->b);
		} else if (gp->small) {
			q3ide_ovl_char_sm(cx, cy, cz, rx, ux, gp->ch, gp->r, gp->g, gp->b);
		} else {
			q3ide_ovl_char(cx, cy, cz, rx, ux, gp->ch, gp->r, gp->g, gp->b);
		}
	}

	Q3IDE_DrawAreaLabel(ox, oy, oz, rx, ux, g_kb_bot);
	Q3IDE_DrawWinList(ox, oy, oz, rx, ux, g_kb_bot, now);
}
