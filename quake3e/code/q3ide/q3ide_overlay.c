/*
 * q3ide_overlay.c — Billboard text primitives + HUD message banner.
 * Keyboard UI + window list: q3ide_overlay_keyboard.c.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_params_theme.h"
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

/* Scaled variant — arbitrary scale multiplier, used by map switcher */
void q3ide_ovl_str_sc(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, float scale,
                      byte r, byte g, byte b)
{
	float cw = Q3IDE_OVL_CHAR_W * scale;
	float ch = Q3IDE_OVL_CHAR_H * scale;
	static const float rsc[4] = {0.0f, 1.0f, 1.0f, 0.0f};
	static const float usc[4] = {0.0f, 0.0f, -1.0f, -1.0f};
	int i = 0;
	for (; *s; s++, i++) {
		polyVert_t v[4];
		int        ch_idx = (unsigned char) *s;
		float      col    = (float) (ch_idx & 15);
		float      row    = (float) (ch_idx >> 4);
		float      u0 = col / 16.0f, u1 = (col + 1.0f) / 16.0f;
		float      t0 = row / 16.0f, t1 = (row + 1.0f) / 16.0f;
		float      bx = ox + rx[0] * cw * i;
		float      by = oy + rx[1] * cw * i;
		float      bz = oz + rx[2] * cw * i;
		int        k;
		for (k = 0; k < 4; k++) {
			v[k].xyz[0]          = bx + rx[0] * cw * rsc[k] + ux[0] * ch * usc[k];
			v[k].xyz[1]          = by + rx[1] * cw * rsc[k] + ux[1] * ch * usc[k];
			v[k].xyz[2]          = bz + rx[2] * cw * rsc[k] + ux[2] * ch * usc[k];
			v[k].st[0]           = (k == 0 || k == 3) ? u0 : u1;
			v[k].st[1]           = (k < 2) ? t0 : t1;
			v[k].modulate.rgba[0] = r;
			v[k].modulate.rgba[1] = g;
			v[k].modulate.rgba[2] = b;
			v[k].modulate.rgba[3] = 255;
		}
		re.AddPolyToScene(g_ovl_chars, 4, v, 1);
	}
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

/*
 * q3ide_ovl_pixel_pos — convert a screen pixel to a world-space position
 * on the overlay plane (at Q3IDE_OVL_DIST units from the camera).
 *
 * Coordinate convention:
 *   px=0,         py=0          → top-left  corner of the viewport
 *   px=fd->width, py=fd->height → bottom-right corner
 *
 * Uses the actual fov_x / fov_y and viewport size from the refdef, so the
 * result is pixel-perfect at any FOV or resolution.
 *
 * Usage: call this to get the anchor position for any overlay element by
 * specifying where on screen (in pixels) you want its top-left origin.
 */
void q3ide_ovl_pixel_pos(const refdef_t *fd, float px, float py, float out[3])
{
	float D = Q3IDE_OVL_DIST;
	float half_w, half_h, off_r, off_u;
	int i;

	if (!fd->width || !fd->height)
		return;

	/* half-extents of the viewport in world units at distance D */
	half_w = D * tanf(fd->fov_x * (M_PI / 360.0f));
	half_h = D * tanf(fd->fov_y * (M_PI / 360.0f));

	/* signed offset from screen centre, in world units:
	 *   off_r > 0  →  left  of centre  (rx = -viewaxis[1] = left direction)
	 *   off_u > 0  →  above centre     (ux =  viewaxis[2]  = up direction)   */
	off_r = (0.5f - px / (float) fd->width) * 2.0f * half_w;
	off_u = (0.5f - py / (float) fd->height) * 2.0f * half_h;

	for (i = 0; i < 3; i++)
		out[i] = fd->vieworg[i] + fd->viewaxis[0][i] * D /* forward */
		         - fd->viewaxis[1][i] * off_r            /* right axis, negated → rx dir  */
		         + fd->viewaxis[2][i] * off_u;           /* up axis */
}

/* ── Monitor corner debug crosses ──────────────────────────────── */

/* Big '+' centred on world pos (cx,cy,cz) in red — 3× normal glyph scale. */
static void draw_corner_cross(float cx, float cy, float cz, const float *rx, const float *ux)
{
	polyVert_t v[4];
	const float scale = 3.0f;
	const float hw = Q3IDE_OVL_CHAR_W * scale * 0.5f;
	const float hh = Q3IDE_OVL_CHAR_H * scale * 0.5f;
	/* UV for '+' (ASCII 43 = 0x2B → col 11, row 2 in bigchars) */
	const float u0 = 11.0f / 16.0f, u1 = 12.0f / 16.0f;
	const float t0 = 2.0f / 16.0f, t1 = 3.0f / 16.0f;
	const float rsc[4] = {-hw, hw, hw, -hw};
	const float usc[4] = {hh, hh, -hh, -hh};
	const float uv_s[4] = {u0, u1, u1, u0};
	const float uv_t[4] = {t0, t0, t1, t1};
	int i;

	for (i = 0; i < 4; i++) {
		v[i].xyz[0] = cx + rx[0] * rsc[i] + ux[0] * usc[i];
		v[i].xyz[1] = cy + rx[1] * rsc[i] + ux[1] * usc[i];
		v[i].xyz[2] = cz + rx[2] * rsc[i] + ux[2] * usc[i];
		v[i].st[0] = uv_s[i];
		v[i].st[1] = uv_t[i];
		v[i].modulate.rgba[0] = 255;
		v[i].modulate.rgba[1] = 0;
		v[i].modulate.rgba[2] = 0;
		v[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(g_ovl_chars, 4, v, 1);
}

/* Draw 4 red crosses at the 4 corners of the current monitor viewport.
 * Call once per monitor pass — 3 monitors → 12 crosses total. */
void Q3IDE_DrawMonitorCorners(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	const float rx[3] = {fd->viewaxis[1][0], fd->viewaxis[1][1], fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	/* (norm_x, norm_y): 0=left/top edge, 1=right/bottom edge */
	static const float cpx[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	static const float cpy[4] = {0.0f, 0.0f, 1.0f, 1.0f};
	int c;

	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;
	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	for (c = 0; c < 4; c++) {
		float pos[3];
		q3ide_ovl_pixel_pos(fd, cpx[c] * (float) fd->width, cpy[c] * (float) fd->height, pos);
		draw_corner_cross(pos[0], pos[1], pos[2], rx, ux);
	}
}

/* ── Calibration overlay (right screen) ────────────────────────── */

/*
 * Q3IDE_DrawCalibration — draws measurement rulers on the right monitor.
 *
 * Layout (all centered horizontally at mid-screen):
 *   Row 0: normal-scale ruler:  |====================|  (22 chars, red)
 *   Row 1: label "NRM 22CH" small grey
 *   Row 2: small-scale ruler:   |====================|  (22 chars, green)
 *   Row 3: label "SM 22CH" small grey
 *   Row 4: "Hello World?" normal scale (yellow)
 *   Row 5: "Hello World?" small scale (cyan)
 *
 * Measure pixel distance between the two | marks on each ruler to get
 * the real pixel width of one character at each scale.
 */
void Q3IDE_DrawCalibration(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	const float rx[3] = {fd->viewaxis[1][0], fd->viewaxis[1][1], fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	float mid[3];
	float lh = Q3IDE_OVL_LINE_H;
	float cw_n = Q3IDE_OVL_CHAR_W;
	float cw_s = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
	int i;
	char ruler[23];

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	/* Screen centre */
	q3ide_ovl_pixel_pos(fd, (float) fd->width * 0.5f, (float) fd->height * 0.5f, mid);

	/* Build ruler string: |====================| (22 chars) */
	ruler[0] = '|';
	for (i = 1; i <= 20; i++)
		ruler[i] = '=';
	ruler[21] = '|';
	ruler[22] = '\0';

/* Helper: draw string centred on mid at vertical offset row*lh */
#define DRAW_CTR_N(str, roff, r, g, b)                                                                                 \
	do {                                                                                                               \
		int _len = (int) strlen(str);                                                                                  \
		float _w = (float) _len * cw_n;                                                                                \
		float _bx = mid[0] - rx[0] * _w * 0.5f + ux[0] * (-(roff) * lh);                                               \
		float _by = mid[1] - rx[1] * _w * 0.5f + ux[1] * (-(roff) * lh);                                               \
		float _bz = mid[2] - rx[2] * _w * 0.5f + ux[2] * (-(roff) * lh);                                               \
		q3ide_ovl_str(_bx, _by, _bz, rx, ux, str, r, g, b);                                                            \
	} while (0)
#define DRAW_CTR_S(str, roff, r, g, b)                                                                                 \
	do {                                                                                                               \
		int _len = (int) strlen(str);                                                                                  \
		float _w = (float) _len * cw_s;                                                                                \
		float _bx = mid[0] - rx[0] * _w * 0.5f + ux[0] * (-(roff) * lh);                                               \
		float _by = mid[1] - rx[1] * _w * 0.5f + ux[1] * (-(roff) * lh);                                               \
		float _bz = mid[2] - rx[2] * _w * 0.5f + ux[2] * (-(roff) * lh);                                               \
		q3ide_ovl_str_sm(_bx, _by, _bz, rx, ux, str, r, g, b);                                                         \
	} while (0)

	/* Row 0: normal ruler (red) */
	DRAW_CTR_N(ruler, 0.0f, 255, 80, 80);
	/* Row 1: label */
	DRAW_CTR_S("NRM: 22 chars above", 1.0f, 160, 160, 160);
	/* Row 2: small ruler (green) */
	DRAW_CTR_S(ruler, 2.0f, 80, 255, 80);
	/* Row 3: label */
	DRAW_CTR_S("SM: 22 chars above", 3.0f, 160, 160, 160);
	/* Row 4: "Hello World?" normal (yellow) */
	DRAW_CTR_N("Hello World?", 4.5f, 255, 255, 80);
	/* Row 5: "Hello World?" small (cyan) */
	DRAW_CTR_S("Hello World?", 5.5f, 80, 255, 255);
	/* Row 6: label */
	DRAW_CTR_S("Hello World? NRM above | SM below", 6.5f, 160, 160, 160);

#undef DRAW_CTR_N
#undef DRAW_CTR_S
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
	const float rx[3] = {fd->viewaxis[1][0], fd->viewaxis[1][1], fd->viewaxis[1][2]};
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

	q3ide_ovl_str(ox, oy, oz, rx, ux, g_hud_msg, Q3IDE_CLR_HUD_AMBER);
}
