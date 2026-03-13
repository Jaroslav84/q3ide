/*
 * q3ide_map_skin_browser.h — Map/skin browser (M key).
 */
#pragma once
#include "../qcommon/q_shared.h"

qboolean Q3IDE_MMenu_IsOpen(void);
qboolean Q3IDE_MMenu_OnKey(int key, qboolean down);
void Q3IDE_MMenu_Draw(const void *refdef_ptr);
void Q3IDE_MMenu_Init(void);
