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
#include "q3ide_interaction.h"
#include "q3ide_aas.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_public.h"
#include "../client/client.h"
#include <string.h>

/* Character cell — sized for OVL_DIST units from camera at 90° FOV.
 * Rule: apparent_px = OVL_CW / OVL_DIST * screen_half_px.
 * At D=10, OVL_CW=0.35 → ~33px per char on 1920px. */
#define OVL_CW    0.14f /* char width  */
#define OVL_CH    0.20f /* char height */
#define OVL_LH    0.30f /* line pitch  */
#define OVL_GAP   0.22f /* gap between key col and label col */
#define OVL_KEY_W 0.56f /* fixed key column (4 chars * OVL_CW) */

/* Very close to camera so polys are always in front of walls */
#define OVL_DIST 10.0f

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
	const float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	/* Far top-left: push left ~78% of 90° half-FOV, lift ~38% */
	float right_off = -OVL_DIST * 0.78f;
	float up_off = OVL_DIST * 0.38f;
	float ox, oy, oz;
	int i;

	/* key col (gray-white), label col (dim gray), header (very dim) */
	struct {
		const char *key;
		const char *label;
	} entries[] = {
	    {"Q3IDE", ""},   /* header */
	    {"K", "Laser"},  /* hold K → laser beams */
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

	/* Room/area display: cluster + area below the keybinding panel */
	if (cls.state == CA_ACTIVE) {
		int leafnum = CM_PointLeafnum(cl.snap.ps.origin);
		int cluster = CM_LeafCluster(leafnum);
		int area = CM_LeafArea(leafnum);
		char room_buf[32];
		float rx_off = n * OVL_LH + OVL_LH * 0.5f; /* one gap below last entry */
		float rlx = ox - ux[0] * rx_off;
		float rly = oy - ux[1] * rx_off;
		float rlz = oz - ux[2] * rx_off;
		Com_sprintf(room_buf, sizeof(room_buf), "area %d  cls %d", area, cluster);
		q3ide_ovl_str(rlx, rly, rlz, rx, ux, room_buf, 100, 200, 255);

		/* AAS warning: orange text one line below area info */
		if (!Q3IDE_AAS_IsLoaded()) {
			float wlx = rlx - ux[0] * OVL_LH;
			float wly = rly - ux[1] * OVL_LH;
			float wlz = rlz - ux[2] * OVL_LH;
			q3ide_ovl_str(wlx, wly, wlz, rx, ux, "NO AAS - approx layout", 255, 140, 0);
		}
	}

	/* Hover label: show hovered window/entity name at top-right of left monitor */
	{
		const char *lbl = NULL;
		int fw = q3ide_interaction.focused_win;
		if (fw >= 0 && fw < Q3IDE_MAX_WIN && q3ide_wm.wins[fw].active && q3ide_wm.wins[fw].label[0])
			lbl = q3ide_wm.wins[fw].label;
		else if (q3ide_interaction.hovered_entity_name[0])
			lbl = q3ide_interaction.hovered_entity_name;
		if (lbl) {
			int len = (int) strlen(lbl);
			float r2 = OVL_DIST * 0.78f;
			float hx =
			    fd->vieworg[0] + fd->viewaxis[0][0] * OVL_DIST - fd->viewaxis[1][0] * r2 + fd->viewaxis[2][0] * up_off;
			float hy =
			    fd->vieworg[1] + fd->viewaxis[0][1] * OVL_DIST - fd->viewaxis[1][1] * r2 + fd->viewaxis[2][1] * up_off;
			float hz =
			    fd->vieworg[2] + fd->viewaxis[0][2] * OVL_DIST - fd->viewaxis[1][2] * r2 + fd->viewaxis[2][2] * up_off;
			hx -= rx[0] * OVL_CW * len;
			hy -= rx[1] * OVL_CW * len;
			hz -= rx[2] * OVL_CW * len;
			/* amber for game entities, warm white for windows */
			if (q3ide_interaction.hovered_entity_name[0] && lbl == q3ide_interaction.hovered_entity_name)
				q3ide_ovl_str(hx, hy, hz, rx, ux, lbl, 255, 220, 80);
			else
				q3ide_ovl_str(hx, hy, hz, rx, ux, lbl, 220, 200, 120);
		}
	}
}
