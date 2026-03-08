/*
 * q3ide_overlay.c — Left-monitor keybinding cheat sheet.
 *
 * Small, macOS-style keybinding panel anchored to the far top-left of the
 * left monitor viewport. Rendered as 3D billboard bigchars quads so it
 * always faces the left monitor camera regardless of player orientation.
 * Called from Q3IDE_MultiMonitorRender for i==0 (left monitor) only.
 */

#include "q3ide_hooks.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"

/* Character cell — small and slick */
#define OVL_CW 5.0f     /* char width  */
#define OVL_CH 7.0f     /* char height */
#define OVL_LH 9.5f     /* line pitch  */
#define OVL_GAP 8.0f    /* gap between key col and label col */
#define OVL_KEY_W 20.0f /* fixed key column width (4 chars * OVL_CW) */

/* Panel distance from camera */
#define OVL_DIST 300.0f

static qhandle_t g_ovl_chars;

static void q3ide_ovl_init(void)
{
	if (!g_ovl_chars)
		g_ovl_chars = re.RegisterShader("gfx/2d/bigchars");
}

/*
 * Render one character as a billboard quad at world (ox,oy,oz).
 * rx = camera-right axis, ux = camera-up axis.
 */
static void q3ide_ovl_char(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                           byte b)
{
	polyVert_t v[4];
	float col = (float) (ch & 15);
	float row = (float) (ch >> 4);
	float u0 = col / 16.0f, u1 = (col + 1.0f) / 16.0f;
	float t0 = row / 16.0f, t1 = (row + 1.0f) / 16.0f;
	static const float rsc[4] = {0.0f, OVL_CW, OVL_CW, 0.0f};
	static const float usc[4] = {0.0f, 0.0f, -OVL_CH, -OVL_CH};
	float uv_s[4] = {u0, u1, u1, u0};
	float uv_t[4] = {t0, t0, t1, t1};
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

/* Render string at (ox,oy,oz), stepping right by OVL_CW per char. */
static void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r, byte g,
                          byte b)
{
	int i = 0;
	for (; *s; s++, i++)
		q3ide_ovl_char(ox + rx[0] * OVL_CW * i, oy + rx[1] * OVL_CW * i, oz + rx[2] * OVL_CW * i, rx, ux,
		               (unsigned char) *s, r, g, b);
}

/*
 * Draw keybinding cheat sheet for the left monitor.
 * refdef_ptr is the (const refdef_t *) for the left viewport.
 * Must be called BEFORE re.RenderScene so polys enter the scene.
 */
void Q3IDE_DrawLeftOverlay(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	/* viewaxis[1] = LEFT in Q3; negate → right. viewaxis[2] = up. */
	float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	/* Far top-left: push left ~80% of 90° half-FOV, lift ~40% */
	float right_off = -OVL_DIST * 0.80f;
	float up_off = OVL_DIST * 0.40f;
	float ox, oy, oz;
	int i;

	/* key col (gray-white), label col (dim gray), header (very dim) */
	struct {
		const char *key;
		const char *label;
	} entries[] = {
	    {"Q3IDE", ""},   /* header */
	    {"L", "Focus"},  /* enter Pointer Mode */
	    {"M1", "Click"}, /* click in Pointer Mode */
	    {"ESC", "Exit"}, /* exit mode */
	};
	int n = (int) (sizeof(entries) / sizeof(entries[0]));

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	/* Panel top-left anchor */
	ox = fd->vieworg[0] + fd->viewaxis[0][0] * OVL_DIST - fd->viewaxis[1][0] * right_off + fd->viewaxis[2][0] * up_off;
	oy = fd->vieworg[1] + fd->viewaxis[0][1] * OVL_DIST - fd->viewaxis[1][1] * right_off + fd->viewaxis[2][1] * up_off;
	oz = fd->vieworg[2] + fd->viewaxis[0][2] * OVL_DIST - fd->viewaxis[1][2] * right_off + fd->viewaxis[2][2] * up_off;

	for (i = 0; i < n; i++) {
		float lx = ox - ux[0] * OVL_LH * i;
		float ly = oy - ux[1] * OVL_LH * i;
		float lz = oz - ux[2] * OVL_LH * i;

		if (i == 0) {
			/* Header: very dim gray */
			q3ide_ovl_str(lx, ly, lz, rx, ux, entries[i].key, 65, 65, 65);
		} else {
			/* Key: medium gray (macOS key cap feel) */
			q3ide_ovl_str(lx, ly, lz, rx, ux, entries[i].key, 190, 190, 190);
			/* Label: dim gray, offset past fixed key column */
			if (entries[i].label[0]) {
				float labx = lx + rx[0] * (OVL_KEY_W + OVL_GAP);
				float laby = ly + rx[1] * (OVL_KEY_W + OVL_GAP);
				float labz = lz + rx[2] * (OVL_KEY_W + OVL_GAP);
				q3ide_ovl_str(labx, laby, labz, rx, ux, entries[i].label, 95, 95, 95);
			}
		}
	}
}
