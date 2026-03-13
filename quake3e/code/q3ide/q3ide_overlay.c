/*
 * q3ide_overlay.c — Billboard text rendering primitives.
 * HUD banner + debug overlays: q3ide_overlay_hud.c.
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
		int ch_idx = (unsigned char) *s;
		float col = (float) (ch_idx & 15);
		float row = (float) (ch_idx >> 4);
		float u0 = col / 16.0f, u1 = (col + 1.0f) / 16.0f;
		float t0 = row / 16.0f, t1 = (row + 1.0f) / 16.0f;
		float bx = ox + rx[0] * cw * i;
		float by = oy + rx[1] * cw * i;
		float bz = oz + rx[2] * cw * i;
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
