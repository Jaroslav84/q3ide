/*
 * q3ide_overlay_winlist.c — Area label + window list panel.
 *
 * Called per-frame from Q3IDE_DrawLeftOverlay.
 * Separate TU so q3ide_overlay_keyboard.c stays under 400 L.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_interaction.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_public.h"
#include "../client/client.h"
#include <string.h>

extern void q3ide_ovl_char(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                           byte b);
extern void q3ide_ovl_char_sm(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                              byte b);
extern void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                          byte g, byte b);
extern void q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                             byte g, byte b);

/* Area + hover label below the keyboard grid. */
void Q3IDE_DrawAreaLabel(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot)
{
    if (cls.state != CA_ACTIVE)
        return;
    {
        int   leafnum = CM_PointLeafnum(cl.snap.ps.origin);
        char  room_buf[32];
        float area_u = kb_bot - 0.12f;
        float alx    = ox + ux[0] * area_u;
        float aly    = oy + ux[1] * area_u;
        float alz    = oz + ux[2] * area_u;
        Com_sprintf(room_buf, sizeof(room_buf), "area %d cls %d", CM_LeafArea(leafnum), CM_LeafCluster(leafnum));
        q3ide_ovl_str(alx, aly, alz, rx, ux, room_buf, 100, 200, 255);

        /* Hover / focused label one line below */
        {
            const char *lbl = NULL;
            int         fw  = q3ide_interaction.focused_win;
            if (fw >= 0 && fw < Q3IDE_MAX_WIN && q3ide_wm.wins[fw].active && q3ide_wm.wins[fw].label[0])
                lbl = q3ide_wm.wins[fw].label;
            else if (q3ide_interaction.hovered_entity_name[0])
                lbl = q3ide_interaction.hovered_entity_name;
            if (lbl) {
                float hx = alx + ux[0] * (-Q3IDE_OVL_LINE_H);
                float hy = aly + ux[1] * (-Q3IDE_OVL_LINE_H);
                float hz = alz + ux[2] * (-Q3IDE_OVL_LINE_H);
                if (lbl == q3ide_interaction.hovered_entity_name)
                    q3ide_ovl_str(hx, hy, hz, rx, ux, lbl, 255, 220, 80);
                else
                    q3ide_ovl_str(hx, hy, hz, rx, ux, lbl, 220, 200, 120);
            }
        }
    }
}

/* Window list panel below area label. */
void Q3IDE_DrawWinList(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot,
                       unsigned long long now)
{
    float wl_top  = kb_bot - 0.12f - Q3IDE_OVL_LINE_H * 2.2f;
    int   n_active = q3ide_wm.num_active;
    float sm_lh   = (n_active > 10) ? Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE * 0.65f
                                     : Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE;
    int   wrow = 0, wi;

    /* Header: "WINS N sN [ndN]" */
    {
        char hdr[48];
        int  streams = 0, nd = 0, _si;
        for (_si = 0; _si < Q3IDE_MAX_WIN; _si++) {
            const q3ide_win_t *_w = &q3ide_wm.wins[_si];
            if (_w->active && _w->owns_stream) {
                if (_w->stream_active)
                    streams++;
                else
                    nd++;
            }
        }
        if (nd > 0)
            Com_sprintf(hdr, sizeof(hdr), "WINS %d s%d nd%d", n_active, streams, nd);
        else
            Com_sprintf(hdr, sizeof(hdr), "WINS %d s%d", n_active, streams);
        {
            float lx = ox + ux[0] * wl_top;
            float ly = oy + ux[1] * wl_top;
            float lz = oz + ux[2] * wl_top;
            q3ide_ovl_str(lx, ly, lz, rx, ux, hdr, 255, 255, 255);
        }
    }
    wrow++;

    for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
        q3ide_win_t *w = &q3ide_wm.wins[wi];
        char         entry[22];
        float        row_u, lx, ly, lz;
        qboolean     failing_now;

        if (!w->active)
            continue;

        Q_strncpyz(entry, w->label, sizeof(entry));
        if (strlen(w->label) > 20) {
            entry[19] = '~';
            entry[20] = '\0';
        }

        row_u = wl_top - (float)wrow * sm_lh;
        lx    = ox + ux[0] * row_u;
        ly    = oy + ux[1] * row_u;
        lz    = oz + ux[2] * row_u;

        failing_now = (w->owns_stream && !w->stream_active) ||
                      (w->last_throttle_ms > 0 && (now - w->last_throttle_ms) < 2000ULL);

        /* Health lamps: normal-size '*' (visually 3× bigger than small) */
        q3ide_ovl_char(lx, ly, lz, rx, ux, '*', w->ever_failed ? 255 : 40, w->ever_failed ? 50 : 200,
                       w->ever_failed ? 50 : 70);
        {
            float l2x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 1.6f;
            float l2y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 1.6f;
            float l2z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 1.6f;
            float llx = lx + rx[0] * Q3IDE_OVL_CHAR_W * 3.2f;
            float lly = ly + rx[1] * Q3IDE_OVL_CHAR_W * 3.2f;
            float llz = lz + rx[2] * Q3IDE_OVL_CHAR_W * 3.2f;
            q3ide_ovl_char(l2x, l2y, l2z, rx, ux, '*', failing_now ? 255 : 40, failing_now ? 30 : 200,
                           failing_now ? 30 : 70);
            if (wi == q3ide_interaction.focused_win)
                q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 255, 230, 140);
            else if (w->owns_stream && !w->stream_active)
                q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 255, 80, 40);
            else
                q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 210, 210, 210);
        }
        wrow++;
    }

    /* Stream failure alert */
    {
        int dead = 0;
        for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
            q3ide_win_t *w = &q3ide_wm.wins[wi];
            if (w->active && w->owns_stream && !w->stream_active)
                dead++;
        }
        if (dead > 0) {
            char  alert[32];
            float al_u = wl_top - (float)(wrow + 1) * sm_lh;
            float alx  = ox + ux[0] * al_u;
            float aly  = oy + ux[1] * al_u;
            float alz  = oz + ux[2] * al_u;
            Com_sprintf(alert, sizeof(alert), "! %d STREAM%s DEAD", dead, dead == 1 ? "" : "S");
            q3ide_ovl_str(alx, aly, alz, rx, ux, alert, 255, 50, 50);
        }
    }
}
