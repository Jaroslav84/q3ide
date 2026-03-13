/* q3ide_los_cache.c — Per-frame LOS visibility cache and crosshair aim trace. */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

extern int q3ide_aimed_win;

void Q3IDE_UpdateLOS(const vec3_t eye)
{
	/* Cache LOS visibility once per frame — reused across all monitor render passes.
	 * Pixel-perfect: trace to center + 4 corners using the same right/up basis as
	 * the renderer. Visible if any of the 5 points has an unobstructed path.
	 * Works from both sides — no normal-offset tricks needed. */
	if (q3ide_wm.num_active) {
		int _li;
		for (_li = 0; _li < Q3IDE_MAX_WIN; _li++) {
			q3ide_win_t *_w = &q3ide_wm.wins[_li];
			if (!_w->active) {
				_w->los_visible = qfalse;
				continue;
			}
#if Q3IDE_DISABLE_LOS_CHECK
			_w->los_visible = qtrue;
#else
			{
				/* Build same right/up basis as q3ide_win_basis(). */
				float _nx = _w->normal[0], _ny = _w->normal[1];
				float _hl = sqrtf(_nx * _nx + _ny * _ny);
				float _rx, _ry, _hw, _hh;
				vec3_t _pt;
				trace_t _tr;
				int _pi;
				/* 5 sample points: center + 4 corners inset by Q3IDE_LOS_CORNER_INSET. */
				static const float _cx[5] = {0.0f, -Q3IDE_LOS_CORNER_INSET, Q3IDE_LOS_CORNER_INSET,
				                             Q3IDE_LOS_CORNER_INSET, -Q3IDE_LOS_CORNER_INSET};
				static const float _cy[5] = {0.0f, -Q3IDE_LOS_CORNER_INSET, -Q3IDE_LOS_CORNER_INSET,
				                             Q3IDE_LOS_CORNER_INSET, Q3IDE_LOS_CORNER_INSET};
				static vec3_t _mins = {0, 0, 0}, _maxs = {0, 0, 0};

				_rx = (_hl > 0.01f) ? -_ny / _hl : 1.0f;
				_ry = (_hl > 0.01f) ? _nx / _hl : 0.0f;
				_hw = _w->world_w * 0.5f;
				_hh = _w->world_h * 0.5f;

				/* Proximity exemption: within N× diagonal, always show.
				 * Hysteresis: already-visible windows use larger threshold
				 * to avoid flicker at the boundary. */
				{
					float _diag = sqrtf(_hw * _hw + _hh * _hh);
					float _thresh =
					    _diag * (_w->los_visible ? Q3IDE_LOS_PROXIMITY_HYST_MULT : Q3IDE_LOS_PROXIMITY_MULT);
					if (_w->player_dist < _thresh) {
						_w->los_visible = qtrue;
						continue;
					}
				}

				_w->los_visible = qfalse;
				for (_pi = 0; _pi < 5; _pi++) {
					_pt[0] = _w->origin[0] + _rx * _cx[_pi] * _hw;
					_pt[1] = _w->origin[1] + _ry * _cx[_pi] * _hw;
					_pt[2] = _w->origin[2] + _cy[_pi] * _hh;
					CM_BoxTrace(&_tr, eye, _pt, _mins, _maxs, 0, CONTENTS_SOLID, qfalse);
					if (_tr.fraction >= Q3IDE_LOS_HIT_THRESHOLD) {
						_w->los_visible = qtrue;
						break;
					}
				}
			}
#endif /* Q3IDE_DISABLE_LOS_CHECK */
		}
	}

	/* Per-frame crosshair hover — red border on aimed window */
	if (q3ide_wm.num_active) {
		vec3_t h_eye, h_fwd;
		float _hp, _hy;
		VectorCopy(cl.snap.ps.origin, h_eye);
		h_eye[2] += cl.snap.ps.viewheight;
		_hp = cl.snap.ps.viewangles[PITCH] * ((float) M_PI / 180.0f);
		_hy = cl.snap.ps.viewangles[YAW] * ((float) M_PI / 180.0f);
		h_fwd[0] = cosf(_hp) * cosf(_hy);
		h_fwd[1] = cosf(_hp) * sinf(_hy);
		h_fwd[2] = -sinf(_hp);
		q3ide_aimed_win = Q3IDE_WM_TraceWindowHit(h_eye, h_fwd, -1);
	} else {
		q3ide_aimed_win = -1;
	}
}
