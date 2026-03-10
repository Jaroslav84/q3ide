/*
 * q3ide_aas_face.c — Per-face wall geometry extraction from AAS data.
 */

#include "q3ide_aas.h"
#include "q3ide_aas_format.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include <math.h>
#include <string.h>

extern q3ide_aas_t g_aas;

/* Horizontal right vector from wall normal. */
static void q3ide_aas_right_vec(const float *normal, float *right)
{
	float nx = normal[0], ny = normal[1];
	float len = sqrtf(nx * nx + ny * ny);
	if (len > 0.01f) {
		right[0] = -ny / len;
		right[1] = nx / len;
		right[2] = 0.0f;
	} else {
		right[0] = 1.0f;
		right[1] = 0.0f;
		right[2] = 0.0f;
	}
}

/*
 * q3ide_aas_face_wall — extract one wall from a faceref entry.
 * Returns 1 if walls[nwalls] was populated, 0 if this face should be skipped.
 */
int q3ide_aas_face_wall(int faceref, const float *point, q3ide_aas_wall_t *walls, int nwalls)
{
	qboolean flipped;
	int facenum, ei, vi;
	const aas_face_t *face;
	const aas_plane_t *plane;
	float normal[3], dist;
	float verts[64][3];
	int nverts = 0;
	float centroid[3], right[3];
	float min_r, max_r, min_z, max_z, cr;

	flipped = (faceref < 0);
	facenum = flipped ? -faceref : faceref;
	if (facenum <= 0 || facenum >= g_aas.num_faces)
		return 0;

	face = &g_aas.faces[facenum];
	if (!(face->faceflags & FACE_SOLID))
		return 0;
	if (face->planenum < 0 || face->planenum >= g_aas.num_planes)
		return 0;

	plane = &g_aas.planes[face->planenum];
	if (flipped) {
		normal[0] = -plane->normal[0];
		normal[1] = -plane->normal[1];
		normal[2] = -plane->normal[2];
		dist = -plane->dist;
	} else {
		normal[0] = plane->normal[0];
		normal[1] = plane->normal[1];
		normal[2] = plane->normal[2];
		dist = plane->dist;
	}

	/* Skip floors and ceilings */
	if (fabsf(normal[2]) > 0.4f)
		return 0;

	/* Collect polygon vertices from edge list */
	for (ei = 0; ei < face->numedges && nverts < 64; ei++) {
		int eidx, edgenum;
		qboolean eflipped;
		const aas_edge_t *edge;

		if (face->firstedge + ei >= g_aas.num_edgeindex)
			break;
		eidx = g_aas.edgeindex[face->firstedge + ei];
		eflipped = (eidx < 0);
		edgenum = eflipped ? -eidx : eidx;
		if (edgenum <= 0 || edgenum >= g_aas.num_edges)
			continue;

		edge = &g_aas.edges[edgenum];
		vi = eflipped ? edge->v[1] : edge->v[0];
		if (vi < 0 || vi >= g_aas.num_verts)
			continue;

		verts[nverts][0] = g_aas.verts[vi].xyz[0];
		verts[nverts][1] = g_aas.verts[vi].xyz[1];
		verts[nverts][2] = g_aas.verts[vi].xyz[2];
		nverts++;
	}

	if (nverts < 3)
		return 0;

	/* Centroid = vertex average, snapped to plane */
	centroid[0] = centroid[1] = centroid[2] = 0.0f;
	for (vi = 0; vi < nverts; vi++) {
		centroid[0] += verts[vi][0];
		centroid[1] += verts[vi][1];
		centroid[2] += verts[vi][2];
	}
	centroid[0] /= nverts;
	centroid[1] /= nverts;
	centroid[2] /= nverts;
	{
		float cd = normal[0] * centroid[0] + normal[1] * centroid[1] + normal[2] * centroid[2] - dist;
		centroid[0] -= cd * normal[0];
		centroid[1] -= cd * normal[1];
		centroid[2] -= cd * normal[2];
	}

	/* Sanity: flip normal toward player regardless of AAS faceref convention */
	{
		float to_eye[3];
		to_eye[0] = point[0] - centroid[0];
		to_eye[1] = point[1] - centroid[1];
		to_eye[2] = point[2] - centroid[2];
		if (normal[0] * to_eye[0] + normal[1] * to_eye[1] + normal[2] * to_eye[2] < 0.0f) {
			normal[0] = -normal[0];
			normal[1] = -normal[1];
			normal[2] = -normal[2];
		}
	}

	q3ide_aas_right_vec(normal, right);

	/* Vertex extents along right axis and Z */
	min_r = min_z = 1e9f;
	max_r = max_z = -1e9f;
	for (vi = 0; vi < nverts; vi++) {
		float r = right[0] * verts[vi][0] + right[1] * verts[vi][1] + right[2] * verts[vi][2];
		if (r < min_r)
			min_r = r;
		if (r > max_r)
			max_r = r;
		if (verts[vi][2] < min_z)
			min_z = verts[vi][2];
		if (verts[vi][2] > max_z)
			max_z = verts[vi][2];
	}
	cr = right[0] * centroid[0] + right[1] * centroid[1] + right[2] * centroid[2];

	/* Dedup: skip if same normal already in output list */
	{
		int di;
		for (di = 0; di < nwalls; di++) {
			float dot =
			    walls[di].normal[0] * normal[0] + walls[di].normal[1] * normal[1] + walls[di].normal[2] * normal[2];
			if (dot > 0.85f)
				return 0;
		}
	}

	/* Floor/ceil from vertical trace (face strip Z is only partial height) */
	{
		vec3_t hstart, hend;
		trace_t htr;
		static vec3_t hmins = {0, 0, 0}, hmaxs = {0, 0, 0};
		hstart[0] = centroid[0] + normal[0] * 2.0f;
		hstart[1] = centroid[1] + normal[1] * 2.0f;
		hstart[2] = centroid[2];
		hend[0] = hstart[0];
		hend[1] = hstart[1];
		hend[2] = hstart[2] + 512.0f;
		CM_BoxTrace(&htr, hstart, hend, hmins, hmaxs, 0, CONTENTS_SOLID, qfalse);
		max_z = hstart[2] + htr.fraction * 512.0f;
		hend[2] = hstart[2] - 512.0f;
		CM_BoxTrace(&htr, hstart, hend, hmins, hmaxs, 0, CONTENTS_SOLID, qfalse);
		min_z = hstart[2] - htr.fraction * 512.0f;
	}

	walls[nwalls].centroid[0] = centroid[0];
	walls[nwalls].centroid[1] = centroid[1];
	walls[nwalls].centroid[2] = centroid[2];
	walls[nwalls].normal[0] = normal[0];
	walls[nwalls].normal[1] = normal[1];
	walls[nwalls].normal[2] = normal[2];
	walls[nwalls].right[0] = right[0];
	walls[nwalls].right[1] = right[1];
	walls[nwalls].right[2] = right[2];
	walls[nwalls].width = max_r - min_r;
	walls[nwalls].left_dist = cr - min_r;
	walls[nwalls].floor_z = min_z;
	walls[nwalls].ceil_z = max_z;

	Q3IDE_LOGI("aas: wall[%d] face=%d n=(%.2f,%.2f,%.2f) w=%.0f h=%.0f", nwalls, facenum, normal[0], normal[1],
	           normal[2], max_r - min_r, max_z - min_z);
	return 1;
}
