/*
 * q3ide_overlay_kbcache.c — Keyboard glyph cache builder.
 *
 * Fills g_glyphs[] (owned by q3ide_overlay_keyboard.c) at most 2 Hz.
 * Labels in a single horizontal row; solid thin lines connect each to its key.
 */

#include "q3ide_params.h"
#include "q3ide_params_theme.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "q3ide_overlay_keys.h"
#include <string.h>

/* keyboard extents — written here, read by keyboard.c */
extern float g_kb_bot;       /* keyboard bottom ux offset from anchor (negative = below) */
extern float g_kb_max_right; /* max right extent across all cached glyphs */

/* ── Key dimensions ─────────────────────────────────────────────────── */
#define KCW    (Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE)
#define KCELL  (KCW * 3.6f)
#define KB_GAP 0.10f

/* ── Color lookup ───────────────────────────────────────────────────── */
static void key_color(key_state_t st, byte *r, byte *g, byte *b)
{
#define SETCLR_X(R, G, B)                                                                                              \
	do {                                                                                                               \
		*r = (R);                                                                                                      \
		*g = (G);                                                                                                      \
		*b = (B);                                                                                                      \
	} while (0)
#define SETCLR(clr) SETCLR_X(clr)
	switch (st) {
	case KEY_Q3IDE:
		SETCLR(Q3IDE_CLR_KEY_Q3IDE);
		break;
	case KEY_QUAKE:
		SETCLR(Q3IDE_CLR_KEY_QUAKE);
		break;
	case KEY_COLLISION:
		SETCLR(Q3IDE_CLR_KEY_COLLISION);
		break;
	case KEY_PASSTHROUGH:
		SETCLR(Q3IDE_CLR_KEY_PASSTHROUGH);
		break;
	default:
		SETCLR(Q3IDE_CLR_KEY_DEFAULT);
		break;
	}
#undef SETCLR
#undef SETCLR_X
}

/* ── Solid thin line: tight glyph chain between two cache points ─────── */
/* Solid connector line — uses '*' (centred glyph, tiles in any direction).
 * Step is intentionally tight so glyphs overlap and appear as a SOLID line.
 * DO NOT use '.' or '-' — those leave visible gaps at diagonal angles. */
static void emit_line(float r0, float u0, float r1, float u1)
{
	float dr = r1 - r0, du = u1 - u0;
	float adx = dr < 0 ? -dr : dr, ady = du < 0 ? -du : du;
	float step = KCW * 0.32f; /* tight overlap — looks solid, not dotted */
	int n = (int) ((adx + ady) / step), k;
	if (n < 2)
		return;
	for (k = 1; k < n; k++) {
		float t = (float) k / (float) n;
		ovl_emit_sm(r0 + dr * t, u0 + du * t, '*', Q3IDE_CLR_KEY_LINE);
	}
}

/* ── Cache rebuild (called at most 2 Hz by q3ide_overlay_keyboard.c) ── */
void q3ide_rebuild_keyboard_cache(void)
{
	int i, j;
	float kb_top;

	g_glyph_count = 0;
	kb_top = -KB_GAP;
	g_kb_bot = kb_top - 5.0f * Q3IDE_OVL_KEY_ROW_H;

	/* Keyboard grid */
	for (i = 0; i < 4; i++) {
		float row_up = kb_top - i * Q3IDE_OVL_KEY_ROW_H;
		float indent = krow_indent[i] * KCELL;
		for (j = 0; j < krow_len[i]; j++) {
			float kx;
			byte kr, kg, kb;
			if (!krows[i][j].display)
				continue;
			key_color(classify(krows[i][j].keynum), &kr, &kg, &kb);
			kx = indent + j * KCELL;
			ovl_emit_sm(kx, row_up, '[', kr, kg, kb);
			ovl_emit_sm(kx + KCW, row_up, krows[i][j].display, kr, kg, kb);
			ovl_emit_sm(kx + KCW * 2, row_up, ']', kr, kg, kb);
		}
	}

	/* Extra keys (ESC, SP, M1, M2) */
	for (i = 0; i < 4; i++) {
		float row_up = kb_top - kextras[i].row * Q3IDE_OVL_KEY_ROW_H;
		float kx = kextras[i].col * KCELL;
		const char *dp = kextras[i].disp;
		byte kr, kg, kb;
		float cx;
		int k;
		key_color(classify(kextras[i].keynum), &kr, &kg, &kb);
		ovl_emit_sm(kx, row_up, '[', kr, kg, kb);
		cx = kx + KCW;
		for (k = 0; dp[k]; k++, cx += KCW)
			ovl_emit_sm(cx, row_up, (unsigned char) dp[k], kr, kg, kb);
		ovl_emit_sm(cx, row_up, ']', kr, kg, kb);
	}

	/* Labels — horizontal row + solid connector to each key */
	{
		float lbl_cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE * 0.7071f;
		float lbl_r = Q3IDE_OVL_CHAR_W * 4.0f;
		for (i = 0; i < 4; i++) {
			for (j = 0; j < krow_len[i]; j++) {
				char lbl_buf[32];
				float key_r, key_u;
				if (!krows[i][j].display || classify(krows[i][j].keynum) != KEY_Q3IDE)
					continue;
				Com_sprintf(lbl_buf, sizeof(lbl_buf), "%c: %s", krows[i][j].display, q3ide_label(krows[i][j].keynum));
				ovl_emit_str_rot(lbl_r, 0.0f, lbl_buf, Q3IDE_CLR_KEY_LABEL);
				key_r = krow_indent[i] * KCELL + j * KCELL + KCW;
				key_u = kb_top - i * Q3IDE_OVL_KEY_ROW_H;
				emit_line(lbl_r, 0.0f, key_r, key_u);
				lbl_r += (float) strlen(lbl_buf) * lbl_cw + 0.25f;
			}
		}
		for (i = 0; i < 4; i++) {
			char lbl_buf[32];
			float key_r, key_u;
			if (classify(kextras[i].keynum) != KEY_Q3IDE)
				continue;
			Com_sprintf(lbl_buf, sizeof(lbl_buf), "%s: %s", kextras[i].disp, q3ide_label(kextras[i].keynum));
			ovl_emit_str_rot(lbl_r, 0.0f, lbl_buf, Q3IDE_CLR_KEY_LABEL);
			key_r = kextras[i].col * KCELL + KCW;
			key_u = kb_top - kextras[i].row * Q3IDE_OVL_KEY_ROW_H;
			emit_line(lbl_r, 0.0f, key_r, key_u);
			lbl_r += (float) strlen(lbl_buf) * lbl_cw + 0.25f;
		}
	}

	/* Record max right extent so keyboard.c can right-align the cache to its anchor */
	{
		float mr = 0.0f;
		int k;
		for (k = 0; k < g_glyph_count; k++)
			if (g_glyphs[k].right > mr)
				mr = g_glyphs[k].right;
		g_kb_max_right = mr;
	}
}
