/*
 * q3ide_aas_query.c — AAS BSP traversal: point-in-area lookup.
 */

#include "q3ide_aas_format.h"
#include "../qcommon/q_shared.h"

extern q3ide_aas_t g_aas;
extern qboolean g_aas_loaded;

/* BSP traversal to find which AAS area contains 'point'.
 * Returns 1-based area index, or 0 if solid / not found. */
int q3ide_aas_point_area(const float *point)
{
	int nodenum = 1; /* root node */
	while (nodenum > 0) {
		const aas_node_t *node = &g_aas.nodes[nodenum];
		const aas_plane_t *plane = &g_aas.planes[node->planenum];
		float d = plane->normal[0] * point[0] + plane->normal[1] * point[1] + plane->normal[2] * point[2] - plane->dist;
		nodenum = node->children[d < 0 ? 1 : 0];
	}
	if (nodenum == 0)
		return 0;        /* solid */
	return -nodenum - 1; /* area index */
}
