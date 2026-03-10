/*
 * q3ide_aas.h — AAS file parser for exact wall geometry.
 *
 * Parses baseq3/maps/<mapname>.aas to extract precise wall face data
 * for each navigation area. Used by the layout engine instead of raycasting
 * so windows are placed exactly on wall geometry, not approximated.
 */

#ifndef Q3IDE_AAS_H
#define Q3IDE_AAS_H

#include "../qcommon/q_shared.h"

/* A solid wall face extracted from an AAS area. */
typedef struct {
	vec3_t centroid; /* center point on the wall plane */
	vec3_t normal;   /* points INTO the area (away from wall) */
	vec3_t right;    /* horizontal basis in the face plane */
	float width;     /* total face width along right axis */
	float left_dist; /* extent left of centroid */
	float floor_z;   /* min Z of face polygon vertices */
	float ceil_z;    /* max Z of face polygon vertices */
} q3ide_aas_wall_t;

/* Load (or reload) the AAS for the given map name (e.g. "q3dm0"). */
qboolean Q3IDE_AAS_Load(const char *mapname);

/* Free all loaded AAS data. */
void Q3IDE_AAS_Free(void);

/* Returns qtrue if an AAS file is currently loaded. */
qboolean Q3IDE_AAS_IsLoaded(void);

/* Returns the AAS area index containing 'point', or 0 if solid/unknown. */
int Q3IDE_AAS_PointArea(const vec3_t point);

/* Find wall faces of the AAS area containing 'point'.
 * Returns number of walls written into 'walls' (up to maxwalls).
 * Returns 0 if AAS not loaded or point is in solid/unknown area. */
int Q3IDE_AAS_GetAreaWalls(const vec3_t point, q3ide_aas_wall_t *walls, int maxwalls);

#endif /* Q3IDE_AAS_H */
