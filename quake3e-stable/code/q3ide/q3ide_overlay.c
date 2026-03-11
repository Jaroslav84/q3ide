/*
 * q3ide_overlay.c — Billboard text primitives + HUD message banner.
 * Keyboard UI + window list: q3ide_overlay_keyboard.c.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <string.h>

/* Character cell — sized for Q3IDE_OVL_DIST units from camera at 90° FOV. */

qhandle_t g_ovl_chars;

void q3ide_ovl_init(void)
{
	if (!g_ovl_chars)
		g_ovl_chars = re.RegisterShader("gfx/2d/bigchars");
}

/*
 * Render one character as a billboard quad at world (ox,oy,oz).
 * rx = camera-right axis, ux = camera-up axis.
 */
void q3ide_ovl_char(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g, byte b)
{
	polyVert_t v[4];
	float col = (float) (ch & 15);
	float row = (float) (ch >> 4);
	float u0 = col / 16.0f, u1 = (col + 1.0f) / 16.0f;
	float t0 = row / 16.0f, t1 = (row + 1.0f) / 16.0f;
	static const float rsc[4] = {0.0f, Q3IDE_OVL_CHAR_W, Q3IDE_OVL_CHAR_W, 0.0f};
	static const float usc[4] = {0.0f, 0.0f, -Q3IDE_OVL_CHAR_H, -Q3IDE_OVL_CHAR_H};
	const float uv_s[4] = {u0, u1, u1, u0};
	const float uv_t[4] = {t0, t0, t1, t1};
	int i;

	for (i = 0; i < 4; i++) {
		v[i].xyz[0] = ox + rx[0] * rsc[i] + ux[0] * usc[i];
		v[i].xyz[1] = oy + rx[1] * rsc[i] + ux[1] * usc[i];
		v[i].xyz[2] = oz + rx[2] * rsc[i] + ux[2] * usc[i];
		v[i].st[0] = uv_s[i];
		v[i].st[1] = uv_t[i];
		v[i].modulate.rgba[0] = r;
		v[i].modulate.rgba[1] = g;
		v[i].modulate.rgba[2] = b;
		v[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(g_ovl_chars, 4, v, 1);
}

/* Render string at (ox,oy,oz), stepping right by Q3IDE_OVL_CHAR_W per char. */
void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r, byte g,
                   byte b)
{
	int i = 0;
	for (; *s; s++, i++)
		q3ide_ovl_char(ox + rx[0] * Q3IDE_OVL_CHAR_W * i, oy + rx[1] * Q3IDE_OVL_CHAR_W * i,
		               oz + rx[2] * Q3IDE_OVL_CHAR_W * i, rx, ux, (unsigned char) *s, r, g, b);
}

/* Single character at Q3IDE_OVL_SMALL_SCALE — used by glyph cache replay. */
void q3ide_ovl_char_sm(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g, byte b)
{
	polyVert_t v[4];
	float cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
	float chh = Q3IDE_OVL_CHAR_H * Q3IDE_OVL_SMALL_SCALE;
	float col = (float) (ch & 15);
	float row = (float) (ch >> 4);
	float u0 = col / 16.0f, u1 = (col + 1.0f) / 16.0f;
	float t0 = row / 16.0f, t1 = (row + 1.0f) / 16.0f;
	static const float rsc[4] = {0.0f, 1.0f, 1.0f, 0.0f};
	static const float usc[4] = {0.0f, 0.0f, -1.0f, -1.0f};
	int k;
	for (k = 0; k < 4; k++) {
		v[k].xyz[0] = ox + rx[0] * cw * rsc[k] + ux[0] * chh * usc[k];
		v[k].xyz[1] = oy + rx[1] * cw * rsc[k] + ux[1] * chh * usc[k];
		v[k].xyz[2] = oz + rx[2] * cw * rsc[k] + ux[2] * chh * usc[k];
		v[k].st[0] = (k == 0 || k == 3) ? u0 : u1;
		v[k].st[1] = (k < 2) ? t0 : t1;
		v[k].modulate.rgba[0] = r;
		v[k].modulate.rgba[1] = g;
		v[k].modulate.rgba[2] = b;
		v[k].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(g_ovl_chars, 4, v, 1);
}

/* Small variant — Q3IDE_OVL_SMALL_SCALE scale for secondary info */
void q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r, byte g,
                      byte b)
{
	float cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
	float ch = Q3IDE_OVL_CHAR_H * Q3IDE_OVL_SMALL_SCALE;
	int i = 0;
	for (; *s; s++, i++) {
		polyVert_t v[4];
		int ch_idx = (unsigned char) *s;
		float col = (float) (ch_idx & 15);
		float row = (float) (ch_idx >> 4);
		float u0 = col / 16.0f, u1 = (col + 1.0f) / 16.0f;
		float t0 = row / 16.0f, t1 = (row + 1.0f) / 16.0f;
		float bx = ox + rx[0] * cw * i;
		float by = oy + rx[1] * cw * i;
		float bz = oz + rx[2] * cw * i;
		static const float rsc[4] = {0.0f, 1.0f, 1.0f, 0.0f};
		static const float usc[4] = {0.0f, 0.0f, -1.0f, -1.0f};
		int k;
		for (k = 0; k < 4; k++) {
			v[k].xyz[0] = bx + rx[0] * cw * rsc[k] + ux[0] * ch * usc[k];
			v[k].xyz[1] = by + rx[1] * cw * rsc[k] + ux[1] * ch * usc[k];
			v[k].xyz[2] = bz + rx[2] * cw * rsc[k] + ux[2] * ch * usc[k];
			v[k].st[0] = (k == 0 || k == 3) ? u0 : u1;
			v[k].st[1] = (k < 2) ? t0 : t1;
			v[k].modulate.rgba[0] = r;
			v[k].modulate.rgba[1] = g;
			v[k].modulate.rgba[2] = b;
			v[k].modulate.rgba[3] = 255;
		}
		re.AddPolyToScene(g_ovl_chars, 4, v, 1);
	}
}

/* ── HUD message banner ─────────────────────────────────────────── */

static char g_hud_msg[64];
static unsigned long long g_hud_msg_expire_ms;

void Q3IDE_SetHudMsg(const char *msg, int duration_ms)
{
	Q_strncpyz(g_hud_msg, msg, sizeof(g_hud_msg));
	g_hud_msg_expire_ms = (unsigned long long) Sys_Milliseconds() + (unsigned long long) duration_ms;
}

void Q3IDE_DrawHudMsg(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	unsigned long long now_ms;
	int len;
	float ox, oy, oz, total_w;
	const float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};

	if (!g_hud_msg[0])
		return;
	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;

	now_ms = (unsigned long long) Sys_Milliseconds();
	if (now_ms >= g_hud_msg_expire_ms) {
		g_hud_msg[0] = '\0';
		return;
	}

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	len = (int) strlen(g_hud_msg);
	total_w = (float) len * Q3IDE_OVL_CHAR_W;
	ox = fd->vieworg[0] + fd->viewaxis[0][0] * Q3IDE_OVL_DIST - rx[0] * total_w * 0.5f +
	     fd->viewaxis[2][0] * Q3IDE_OVL_DIST * 0.42f;
	oy = fd->vieworg[1] + fd->viewaxis[0][1] * Q3IDE_OVL_DIST - rx[1] * total_w * 0.5f +
	     fd->viewaxis[2][1] * Q3IDE_OVL_DIST * 0.42f;
	oz = fd->vieworg[2] + fd->viewaxis[0][2] * Q3IDE_OVL_DIST - rx[2] * total_w * 0.5f +
	     fd->viewaxis[2][2] * Q3IDE_OVL_DIST * 0.42f;

	q3ide_ovl_str(ox, oy, oz, rx, ux, g_hud_msg, 255, 220, 80); /* amber */
}
