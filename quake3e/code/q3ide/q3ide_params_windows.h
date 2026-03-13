/*
 * q3ide_params_windows.h — App lists for capture routing.
 *
 * Defines which windows go through which capture path.
 * Included by q3ide_params.h — do not include directly.
 */
#pragma once

/* ── App lists — capture routing ─────────────────────────────────── */

/* Windows blocked from standalone tunnel capture (still visible in display captures).
 * Glob patterns, case-insensitive. *word* = substring match. */
#define Q3IDE_DEDICATED_WIN_BLACKLIST                                                                                  \
	"Dock", "LPSpringboard", "loginwindow", "*Accessibility Services*", "Spotlight", "Notification Center",            \
	    "NotificationCenter", "*Screen & System Audio Recording*", "WindowServer", "Desktop", "*Little Snitch Agent*", \
	    "*Keyboard Maestro Engine*", "*Quake*", "*Open and Save Panel*", "*Folder X*", "*Clipboard*", NULL

/* CPU windows: composite display-crop stream. Good for: terminals, text editors (CPU-rendered). */
#define Q3IDE_CPU_WINDOWS_LIST "iTerm2", "Terminal", NULL

/* GPU windows: dedicated per-window SCStream. Good for: browsers, Electron apps (GPU-rendered). */
#define Q3IDE_GPU_WINDOWS_LIST                                                                                         \
	"Google Chrome", "Chromium", "Safari", "Firefox", "Arc", "Brave Browser", "Opera", "Microsoft Edge", NULL
