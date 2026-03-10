/*
 * q3ide_aas_walls.c — Q3IDE_AAS_GetAreaWalls: iterate area faces → walls.
 */

#include "q3ide_aas.h"
#include "q3ide_aas_fmt.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"

extern q3ide_aas_t g_aas;
extern qboolean g_aas_loaded;
extern int q3ide_aas_point_area(const float *point);
extern int q3ide_aas_face_wall(int faceref, const float *point, q3ide_aas_wall_t *walls, int nwalls);

int Q3IDE_AAS_GetAreaWalls(const vec3_t point, q3ide_aas_wall_t *walls, int maxwalls)
{
	int fi, nwalls = 0, areanum;
	const aas_area_t *area;

	if (!g_aas_loaded || !walls || maxwalls <= 0)
		return 0;

	areanum = q3ide_aas_point_area(point);
	if (areanum <= 0 || areanum >= g_aas.num_areas) {
		Q3IDE_LOGE("aas: point (%.0f,%.0f,%.0f) not in any area (got %d)", point[0], point[1], point[2], areanum);
		return 0;
	}

	area = &g_aas.areas[areanum];
	Q3IDE_LOGI("aas: player in area %d (%d faces)", areanum, area->numfaces);

	for (fi = 0; fi < area->numfaces && nwalls < maxwalls; fi++) {
		int faceref;
		if (area->firstface + fi >= g_aas.num_faceindex)
			break;
		faceref = g_aas.faceindex[area->firstface + fi];
		if (q3ide_aas_face_wall(faceref, point, walls, nwalls))
			nwalls++;
	}
	return nwalls;
}
