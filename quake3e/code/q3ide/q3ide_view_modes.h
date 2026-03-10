/*
 * q3ide_view_modes.h — Overview (hold O) and Focus3 (press I) view layouts.
 */
#pragma once

#include "../qcommon/q_shared.h"

/* Register +q3ide_overview / -q3ide_overview / q3ide_focus3 commands. */
void Q3IDE_ViewModes_Init(void);

/* Remove commands and clear state on shutdown. */
void Q3IDE_ViewModes_Shutdown(void);

/* Returns qtrue while the O key is held and overview grid is active. */
qboolean Q3IDE_ViewModes_OverviewActive(void);
