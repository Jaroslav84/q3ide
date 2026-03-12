/*
 * q3ide_overlay_keyboard.c — Left overlay frame draw + glyph cache owner.
 *
 * Owns g_glyphs[]; q3ide_overlay_kbcache.c fills it at most 2 Hz.
 * Replay is cheap: transforms each cached glyph and calls AddPolyToScene.
 *
 * Layout (left monitor):
 *   Keyboard   — top-right corner  (g_glyphs replayed shifted left by g_kb_max_right)
 *   Winlist    — bottom-left corner (anchored via q3ide_ovl_pixel_pos)
 *   Notifications (PAUSED/HIDDEN) — below keyboard, right-aligned
 *   Area label — top-left corner
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_params_theme.h"
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
extern void q3ide_ovl_pixel_pos(const refdef_t *fd, float px, float py, float out[3]);

/* q3ide_overlay_lamps.c */
extern void Q3IDE_DrawAreaLabel(float ox, float oy, float oz, const float *rx, const float *ux);

/* q3ide_overlay_winlist.c */
extern void Q3IDE_DrawWinList(const void *refdef_ptr, float ox, float oy, float oz, const float *rx, const float *ux,
                              unsigned long long now);

/* q3ide_overlay_kbcache.c */
extern void q3ide_rebuild_keyboard_cache(void);

/* ── Glyph cache storage — written by kbcache.c, read here ─────────── */
ovl_rel_glyph_t g_glyphs[OVL_MAX_GLYPHS];
int g_glyph_count;
qboolean g_ovl_dirty = qtrue;
unsigned long long g_ovl_last_build_ms = 0;

float g_kb_bot;       /* keyboard bottom ux offset from anchor — set by kbcache.c */
float g_kb_max_right; /* max right extent of glyph cache — set by kbcache.c */

/* ── Main draw (called every frame from Q3IDE_Frame) ────────────────── */
void Q3IDE_DrawLeftOverlay(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	const float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	unsigned long long now;
	int gi;

	q3ide_ovl_init();
	{
		static int once;
		if (!once) {
			Q3IDE_LOGI("ovl_init: chars=%d AddPoly=%s", (int) g_ovl_chars, re.AddPolyToScene ? "ok" : "null");
			once = 1;
		}
	}
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	/* Rate-limited cache rebuild */
	now = (unsigned long long) Sys_Milliseconds();
	if (g_ovl_dirty || (now - g_ovl_last_build_ms) >= OVL_REBUILD_MS) {
		q3ide_rebuild_keyboard_cache();
		g_ovl_last_build_ms = now;
		g_ovl_dirty = qfalse;
		{
			static int once2;
			if (!once2) {
				Q3IDE_LOGI("ovl kbcache built: glyph_count=%d max_right=%.2f kb_bot=%.2f", g_glyph_count,
				           g_kb_max_right, g_kb_bot);
				once2 = 1;
			}
		}
	}

	/* ── Keyboard anchor: top-right corner ─────────────────────────── */
	float kbpos[3];
	q3ide_ovl_pixel_pos(fd, (float) fd->width - Q3IDE_OVL_KB_RIGHT_MARGIN_PX, (float) Q3IDE_OVL_KB_TOP_MARGIN_PX,
	                    kbpos);

	/* Replay glyph cache — shift right by -g_kb_max_right so rightmost
	 * glyph lands at kbpos (right screen edge minus margin). */
	for (gi = 0; gi < g_glyph_count; gi++) {
		const ovl_rel_glyph_t *gp = &g_glyphs[gi];
		float dr = gp->right - g_kb_max_right;
		float cx = kbpos[0] + rx[0] * dr + ux[0] * gp->up;
		float cy = kbpos[1] + rx[1] * dr + ux[1] * gp->up;
		float cz = kbpos[2] + rx[2] * dr + ux[2] * gp->up;
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

	/* ── Notifications: below keyboard, right-aligned ───────────────── */
	{
		float nl = Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE;
		float notif_cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
		int nrow = 0;

		if (q3ide_wm.streams_paused) {
			float row_up = g_kb_bot - nl * (float) (nrow + 1);
			float tw = 6.0f * notif_cw; /* "PAUSED" = 6 chars */
			float lx = kbpos[0] - rx[0] * tw + ux[0] * row_up;
			float ly = kbpos[1] - rx[1] * tw + ux[1] * row_up;
			float lz = kbpos[2] - rx[2] * tw + ux[2] * row_up;
			q3ide_ovl_str_sm(lx, ly, lz, rx, ux, "PAUSED", Q3IDE_CLR_NOTIF_PAUSED);
			nrow++;
		}
		if (q3ide_wm.wins_hidden) {
			float row_up = g_kb_bot - nl * (float) (nrow + 1);
			float tw = 6.0f * notif_cw; /* "HIDDEN" = 6 chars */
			float lx = kbpos[0] - rx[0] * tw + ux[0] * row_up;
			float ly = kbpos[1] - rx[1] * tw + ux[1] * row_up;
			float lz = kbpos[2] - rx[2] * tw + ux[2] * row_up;
			q3ide_ovl_str_sm(lx, ly, lz, rx, ux, "HIDDEN", Q3IDE_CLR_NOTIF_HIDDEN);
			nrow++;
		}
		(void) nrow;
	}

	/* ── Area label: top-left corner ────────────────────────────────── */
	{
		float alpos[3];
		q3ide_ovl_pixel_pos(fd, (float) Q3IDE_OVL_WL_LEFT_MARGIN_PX, (float) Q3IDE_OVL_AREA_LABEL_TOP_PX, alpos);
		Q3IDE_DrawAreaLabel(alpos[0], alpos[1], alpos[2], rx, ux);
	}

	/* ── Window list: left edge, vertically pixel-anchored ──────────── */
	{
		float wlpos[3];
		q3ide_ovl_pixel_pos(fd, (float) Q3IDE_OVL_WL_LEFT_MARGIN_PX, 0.0f, wlpos);
		Q3IDE_DrawWinList(fd, wlpos[0], wlpos[1], wlpos[2], rx, ux, now);
	}
}
