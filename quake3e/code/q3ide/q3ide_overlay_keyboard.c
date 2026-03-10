/*
 * q3ide_overlay_keyboard.c — QWERTY keyboard UI + window-list panel.
 *
 * Rebuild (≤2 Hz): key colours, label staircase, dot connectors → glyph cache.
 * Replay (every frame): matrix math + AddPolyToScene from flat cache.
 * Per-frame only: area label, hover label, window list (live data).
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_interaction.h"
#include "q3ide_view_modes.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_public.h"
#include "../client/client.h"
#include <string.h>

/* Primitives from q3ide_overlay.c */
extern qhandle_t g_ovl_chars;
extern void      q3ide_ovl_init(void);
extern void      q3ide_ovl_char(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                                byte b);
extern void      q3ide_ovl_char_sm(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r,
                                   byte g, byte b);
extern void      q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                               byte g, byte b);
extern void      q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                                  byte g, byte b);

/* Layout tables, glyph cache, classify(), q3ide_label() */
#include "q3ide_overlay_keys.h"

/* q3ide_overlay_winlist.c */
extern void Q3IDE_DrawAreaLabel(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot);
extern void Q3IDE_DrawWinList(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot,
                              unsigned long long now);

/* ── Panel anchor (world-unit offsets from camera forward axis) ─────── */
#define ANCHOR_RIGHT (-Q3IDE_OVL_DIST * 0.88f - 0.5f) /* far-left edge of left monitor */
#define ANCHOR_UP    (Q3IDE_OVL_DIST * 0.46f + 0.5f)  /* near top of left monitor */

/* ── Small-char keyboard dimensions ────────────────────────────────── */
#define KCW  (Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE) /* 0.084 — small char width  */
#define KCH  (Q3IDE_OVL_CHAR_H * Q3IDE_OVL_SMALL_SCALE) /* 0.120 — small char height */
#define KCELL (KCW * 3.6f)                               /* [X] + gap per key        */

/* ── Diagonal label staircase above keyboard ────────────────────────── */
#define LBL_STEP_R (Q3IDE_OVL_CHAR_W * 2.4f) /* right step per label row  */
#define LBL_STEP_D Q3IDE_OVL_LINE_H           /* down step per label row   */
#define KB_GAP     0.10f                       /* labels → keyboard spacing */

/* Tracks keyboard bottom for per-frame section positioning. */
static float g_kb_bot; /* up-offset of keyboard bottom from anchor (negative) */

/* ── Color lookup ───────────────────────────────────────────────────── */
static void key_color(key_state_t st, byte *r, byte *g, byte *b)
{
	switch (st) {
	case KEY_Q3IDE:       *r = 220; *g = 220; *b = 220; break; /* white              */
	case KEY_QUAKE:       *r = 190; *g = 155; *b = 25;  break; /* amber              */
	case KEY_COLLISION:   *r = 255; *g = 30;  *b = 30;  break; /* bright red         */
	case KEY_PASSTHROUGH: *r = 60;  *g = 200; *b = 120; break; /* green: live        */
	default:              *r = 110; *g = 110; *b = 110; break; /* light gray: unused */
	}
}

/* ── Cache rebuild (≤ 2 Hz) ─────────────────────────────────────────── */
/* passthrough=qtrue: KEYBOARD mode — every bound key shown as KEY_PASSTHROUGH */
static void q3ide_rebuild_keyboard_cache(qboolean passthrough)
{
	int   i, j, lrow = 0, n_labels = 0;
	float kb_top;

	g_glyph_count = 0;

	/* In passthrough mode there are no labels, keyboard starts near anchor. */
	if (!passthrough) {
		for (i = 0; i < 4; i++)
			for (j = 0; j < krow_len[i]; j++)
				if (krows[i][j].display && classify(krows[i][j].keynum) == KEY_Q3IDE)
					n_labels++;
		for (i = 0; i < 4; i++)
			if (classify(kextras[i].keynum) == KEY_Q3IDE)
				n_labels++;
	}

	kb_top   = -(n_labels * LBL_STEP_D + KB_GAP);
	g_kb_bot  = kb_top - 5.0f * Q3IDE_OVL_KEY_ROW_H; /* 4 rows + extras row */

	/* ── Keyboard grid (small [X] keys, left-aligned from anchor) ── */
	for (i = 0; i < 4; i++) {
		float row_up = kb_top - i * Q3IDE_OVL_KEY_ROW_H;
		float indent = krow_indent[i] * KCELL;
		for (j = 0; j < krow_len[i]; j++) {
			float      kx;
			byte       kr, kg, kb;
			key_state_t st;
			if (!krows[i][j].display)
				continue;
			st = passthrough ? (classify(krows[i][j].keynum) != KEY_UNBOUND ? KEY_PASSTHROUGH : KEY_UNBOUND)
			                 : classify(krows[i][j].keynum);
			key_color(st, &kr, &kg, &kb);
			kx = indent + j * KCELL;
			ovl_emit_sm(kx,           row_up, '[',                 kr, kg, kb);
			ovl_emit_sm(kx + KCW,     row_up, krows[i][j].display, kr, kg, kb);
			ovl_emit_sm(kx + KCW * 2, row_up, ']',                 kr, kg, kb);
		}
	}
	/* Extra keys (ESC, SP, M1, M2) */
	for (i = 0; i < 4; i++) {
		float       row_up = kb_top - kextras[i].row * Q3IDE_OVL_KEY_ROW_H;
		float       kx     = kextras[i].col * KCELL;
		const char *dp     = kextras[i].disp;
		byte        kr, kg, kb;
		float       cx;
		int         k;
		key_state_t st;
		st = passthrough ? (classify(kextras[i].keynum) != KEY_UNBOUND ? KEY_PASSTHROUGH : KEY_UNBOUND)
		                 : classify(kextras[i].keynum);
		key_color(st, &kr, &kg, &kb);
		ovl_emit_sm(kx, row_up, '[', kr, kg, kb);
		cx = kx + KCW;
		for (k = 0; dp[k]; k++, cx += KCW)
			ovl_emit_sm(cx, row_up, (unsigned char)dp[k], kr, kg, kb);
		ovl_emit_sm(cx, row_up, ']', kr, kg, kb);
	}

	/* ── Label staircase + dotted connectors (not in passthrough) ─────── */
	if (passthrough)
		return;
	lrow = 0;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < krow_len[i]; j++) {
			char        full_lbl[32];
			float       lbl_r, lbl_u, key_r, key_u, dx, dy, adx, ady, dist;
			float       dot_step;
			int         n_dots, d, llen;
			const char *lbl;
			if (!krows[i][j].display || classify(krows[i][j].keynum) != KEY_Q3IDE)
				continue;

			lbl_r = lrow * LBL_STEP_R;
			lbl_u = -(float)lrow * LBL_STEP_D;
			key_r = krow_indent[i] * KCELL + j * KCELL + KCW; /* centre of [X] */
			key_u = kb_top - i * Q3IDE_OVL_KEY_ROW_H;

			/* Label: "X: Label" rotated -45° (baseline goes up-right).
			 * Text starts exactly where the dotted connector starts. */
			lbl = q3ide_label(krows[i][j].keynum);
			Com_sprintf(full_lbl, sizeof(full_lbl), "%c: %s", krows[i][j].display, lbl);
			ovl_emit_str_rot(lbl_r, lbl_u, full_lbl, 230, 230, 230);

			/* Dotted connector: thin white dots from label anchor to key. */
			dx       = key_r - lbl_r;
			dy       = key_u - lbl_u;
			adx      = dx > 0 ? dx : -dx;
			ady      = dy > 0 ? dy : -dy;
			dist     = adx + ady;
			dot_step = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE * 2.0f;
			n_dots   = (int)(dist / dot_step);
			if (n_dots > 30)
				n_dots = 30;
			for (d = 1; d <= n_dots; d++) {
				float t = (float)d / (float)(n_dots + 1);
				ovl_emit_sm(lbl_r + dx * t, lbl_u + dy * t, '.', 200, 200, 200);
			}

			llen = strlen(full_lbl);
			(void)llen; /* used by extras loop below */
			lrow++;
		}
	}
	for (i = 0; i < 4; i++) {
		char        full_lbl[32];
		float       lbl_r, lbl_u, key_r, key_u, dx, dy, adx, ady, dist;
		float       dot_step;
		int         n_dots, d;
		const char *lbl;
		if (classify(kextras[i].keynum) != KEY_Q3IDE)
			continue;

		lbl_r = lrow * LBL_STEP_R;
		lbl_u = -(float)lrow * LBL_STEP_D;
		key_r = kextras[i].col * KCELL + KCW;
		key_u = kb_top - kextras[i].row * Q3IDE_OVL_KEY_ROW_H;

		lbl = q3ide_label(kextras[i].keynum);
		Com_sprintf(full_lbl, sizeof(full_lbl), "%s: %s", kextras[i].disp, lbl);
		ovl_emit_str_rot(lbl_r, lbl_u, full_lbl, 230, 230, 230);

		dx       = key_r - lbl_r;
		dy       = key_u - lbl_u;
		adx      = dx > 0 ? dx : -dx;
		ady      = dy > 0 ? dy : -dy;
		dist     = adx + ady;
		dot_step = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE * 2.0f;
		n_dots   = (int)(dist / dot_step);
		if (n_dots > 30)
			n_dots = 30;
		for (d = 1; d <= n_dots; d++) {
			float t = (float)d / (float)(n_dots + 1);
			ovl_emit_sm(lbl_r + dx * t, lbl_u + dy * t, '.', 200, 200, 200);
		}
		lrow++;
	}
}

/* ── Main draw (called every frame from Q3IDE_Frame) ────────────────── */
void Q3IDE_DrawLeftOverlay(const void *refdef_ptr)
{
	const refdef_t          *fd   = (const refdef_t *)refdef_ptr;
	const float              rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float              ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	float                    ox, oy, oz;
	unsigned long long       now;
	int                      gi;
	q3ide_interaction_mode_t cur_mode;
	qboolean                 passthrough;
	static q3ide_interaction_mode_t s_last_mode = Q3IDE_MODE_FPS;

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	/* Anchor: top-left corner of left monitor */
	ox = fd->vieworg[0] + fd->viewaxis[0][0] * Q3IDE_OVL_DIST + rx[0] * ANCHOR_RIGHT +
	     fd->viewaxis[2][0] * ANCHOR_UP;
	oy = fd->vieworg[1] + fd->viewaxis[0][1] * Q3IDE_OVL_DIST + rx[1] * ANCHOR_RIGHT +
	     fd->viewaxis[2][1] * ANCHOR_UP;
	oz = fd->vieworg[2] + fd->viewaxis[0][2] * Q3IDE_OVL_DIST + rx[2] * ANCHOR_RIGHT +
	     fd->viewaxis[2][2] * ANCHOR_UP;

	/* Mode banner: "OVW" / "FPS" / "PTR" / "KBD" at anchor top — per-frame */
	cur_mode    = Q3IDE_Interaction_GetMode();
	passthrough = (cur_mode == Q3IDE_MODE_KEYBOARD);
	if (cur_mode != s_last_mode) {
		g_ovl_dirty = qtrue;
		s_last_mode = cur_mode;
	}
	{
		/* index 3 = OVW (overview active), 0-2 = FPS/PTR/KBD */
		static const char *mode_strs[] = {"FPS", "PTR", "KBD", "OVW"};
		static const byte  mode_r[]    = {100, 160, 60, 255};
		static const byte  mode_g[]    = {100, 200, 200, 180};
		static const byte  mode_b[]    = {100, 80, 120, 50};
		int mi = Q3IDE_ViewModes_OverviewActive() ? 3 : ((int)cur_mode < 3 ? (int)cur_mode : 0);
		q3ide_ovl_str_sm(ox, oy, oz, rx, ux, mode_strs[mi], mode_r[mi], mode_g[mi], mode_b[mi]);
	}

	/* Streams-paused banner: "PAUSED" in amber, one line below mode banner */
	if (q3ide_wm.streams_paused) {
		float pbx = ox + ux[0] * (-Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE);
		float pby = oy + ux[1] * (-Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE);
		float pbz = oz + ux[2] * (-Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE);
		q3ide_ovl_str_sm(pbx, pby, pbz, rx, ux, "PAUSED", 255, 170, 30);
	}

	/* Rate-limited rebuild — force on mode change */
	now = (unsigned long long)Sys_Milliseconds();
	if (g_ovl_dirty || (now - g_ovl_last_build_ms) >= OVL_REBUILD_MS) {
		q3ide_rebuild_keyboard_cache(passthrough);
		g_ovl_last_build_ms = now;
		g_ovl_dirty         = qfalse;
	}

	/* Replay glyph cache — cheap per-frame path */
	for (gi = 0; gi < g_glyph_count; gi++) {
		const ovl_rel_glyph_t *gp = &g_glyphs[gi];
		float cx = ox + rx[0] * gp->right + ux[0] * gp->up;
		float cy = oy + rx[1] * gp->right + ux[1] * gp->up;
		float cz = oz + rx[2] * gp->right + ux[2] * gp->up;
		if (gp->rotate) {
			/* -45°: rot_rx = rx*cos45 + ux*cos45   rot_ux = -rx*sin45 + ux*cos45 */
			float rot_rx[3], rot_ux[3];
			int   k;
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

	/* ── Area label + window list (in q3ide_overlay_winlist.c) ── */
	Q3IDE_DrawAreaLabel(ox, oy, oz, rx, ux, g_kb_bot);
	Q3IDE_DrawWinList(ox, oy, oz, rx, ux, g_kb_bot, now);
}
