/*
 * q3ide_hotkey.c — Tap-vs-hold key detection.  See q3ide_hotkey.h for API.
 */

#include "q3ide_hotkey.h"
#include "q3ide_params.h"
#include "../qcommon/qcommon.h"

q3ide_hk_result_t q3ide_hk_down(q3ide_hotkey_t *hk, qboolean is_on)
{
	if (is_on) {
		hk->locked = qfalse;
		hk->press_ms = 0;
		return Q3IDE_HK_DEACTIVATE;
	}
	hk->press_ms = (unsigned long long) Sys_Milliseconds();
	return Q3IDE_HK_ACTIVATE;
}

q3ide_hk_result_t q3ide_hk_up(q3ide_hotkey_t *hk, qboolean is_on)
{
	if (!is_on || hk->press_ms == 0)
		return Q3IDE_HK_NONE;

	if ((unsigned long long) Sys_Milliseconds() - hk->press_ms >= Q3IDE_SHORTPRESS_MS) {
		/* held long enough → dismiss on release */
		hk->press_ms = 0;
		hk->locked = qfalse;
		return Q3IDE_HK_DEACTIVATE;
	}

	/* quick tap → stay active; second press will deactivate */
	hk->locked = qtrue;
	hk->press_ms = 0;
	return Q3IDE_HK_NONE;
}

void q3ide_hk_rearm(q3ide_hotkey_t *hk)
{
	hk->press_ms = (unsigned long long) Sys_Milliseconds();
}
