/*
 * q3ide_hotkey.h — Tap-vs-hold key detection.
 *
 * Unified behaviour for every Q3IDE hotkey:
 *
 *   Press when OFF  → ACTIVATE      (feature turns on)
 *   Press when ON   → DEACTIVATE    (second press cancels)
 *   Quick release   → NONE          (tap-lock: feature stays on)
 *   Long release    → DEACTIVATE    (hold-to-dismiss)
 *
 * ── Usage A — hardwired toggle keys (;, H) ──────────────────────────
 *
 *   static q3ide_hotkey_t hk = Q3IDE_HOTKEY_INIT;
 *
 *   if (down) {
 *       if (q3ide_hk_down(&hk, hk.locked) == Q3IDE_HK_ACTIVATE)
 *           feature_on();
 *       else
 *           feature_off();
 *   } else {
 *       if (q3ide_hk_up(&hk, qtrue) == Q3IDE_HK_DEACTIVATE)
 *           feature_off();
 *   }
 *
 *   hk.locked is the "tap-locked on" flag.  Pass it as `is_on` to
 *   q3ide_hk_down so the second press deactivates cleanly.
 *   Pass `qtrue` to q3ide_hk_up — the feature is always on between
 *   key-down and key-up.
 *
 * ── Usage B — command-bound show/hide keys (O, I) ───────────────────
 *
 *   static q3ide_hotkey_t hk = Q3IDE_HOTKEY_INIT;
 *
 *   void cmd_down(void) {
 *       if (q3ide_hk_down(&hk, g_feature_active) == Q3IDE_HK_ACTIVATE)
 *           feature_show();
 *       else
 *           feature_hide();
 *   }
 *   void cmd_up(void) {
 *       if (q3ide_hk_up(&hk, g_feature_active) == Q3IDE_HK_DEACTIVATE)
 *           feature_hide();
 *   }
 */
#pragma once

#include "../qcommon/q_shared.h"

typedef struct {
	unsigned long long press_ms; /* Sys_Milliseconds() at key-down; 0 = idle */
	qboolean           locked;   /* tap-locked on — second press needed to turn off */
} q3ide_hotkey_t;

#define Q3IDE_HOTKEY_INIT \
	{                     \
		0ULL, qfalse      \
	}

typedef enum {
	Q3IDE_HK_NONE       = 0, /* no state change */
	Q3IDE_HK_ACTIVATE   = 1, /* turn feature ON  */
	Q3IDE_HK_DEACTIVATE = 2  /* turn feature OFF */
} q3ide_hk_result_t;

/*
 * Call on key-down.  `is_on` = current "on" state of the feature
 * (hk->locked for Usage A; g_feature_active for Usage B).
 *   is_on false → ACTIVATE  (records press time)
 *   is_on true  → DEACTIVATE (clears lock + press time)
 */
q3ide_hk_result_t q3ide_hk_down(q3ide_hotkey_t *hk, qboolean is_on);

/*
 * Call on key-up.  `is_on` = current "on" state of the feature.
 *   held (>= Q3IDE_SHORTPRESS_MS) → DEACTIVATE
 *   tapped (< Q3IDE_SHORTPRESS_MS) → NONE  (hk->locked set to qtrue)
 */
q3ide_hk_result_t q3ide_hk_up(q3ide_hotkey_t *hk, qboolean is_on);

/*
 * Reset the press timestamp to now.
 * Call AFTER a slow activation (e.g. overview_show, focus3_show) so that
 * the hold-vs-tap measurement starts when the user actually sees the result,
 * not when the key was pressed (which would inflate the elapsed time).
 */
void q3ide_hk_rearm(q3ide_hotkey_t *hk);
