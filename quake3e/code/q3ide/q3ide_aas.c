/*
 * q3ide_aas.c — AAS file parser for exact wall geometry.
 *
 * Reads the binary AAS navigation file and extracts solid wall faces for
 * the area containing the player. This gives the layout engine exact polygon
 * geometry for every wall — no raycasting approximation, no clipping.
 *
 * AAS format reference: quake3e-orig/code/botlib/aasfile.h
 */

#include "q3ide_aas.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ── AAS binary format structs (from botlib/aasfile.h) ─────────── */

#define AASID (('S' << 24) + ('A' << 16) + ('A' << 8) + 'E')
#define AASVERSION 5
#define AAS_LUMPS 14

#define AASLUMP_BBOXES 0
#define AASLUMP_VERTEXES 1
#define AASLUMP_PLANES 2
#define AASLUMP_EDGES 3
#define AASLUMP_EDGEINDEX 4
#define AASLUMP_FACES 5
#define AASLUMP_FACEINDEX 6
#define AASLUMP_AREAS 7
#define AASLUMP_AREASETTINGS 8
#define AASLUMP_REACHABILITY 9
#define AASLUMP_NODES 10
#define AASLUMP_PORTALS 11
#define AASLUMP_PORTALINDEX 12
#define AASLUMP_CLUSTERS 13

/* Face flags */
#define FACE_SOLID 1
#define FACE_GROUND 4
#define FACE_GAP 8

typedef struct {
	int fileofs, filelen;
} aas_lump_t;

typedef struct {
	int ident;
	int version;
	int bspchecksum;
	aas_lump_t lumps[AAS_LUMPS];
} aas_header_t;

typedef struct {
	float xyz[3];
} aas_vertex_t;

typedef struct {
	float normal[3];
	float dist;
	int type;
} aas_plane_t;

typedef struct {
	int v[2];
} aas_edge_t;

typedef struct {
	int planenum;
	int faceflags;
	int numedges;
	int firstedge;
	int frontarea;
	int backarea;
} aas_face_t;

typedef struct {
	int areanum;
	int numfaces;
	int firstface;
	float mins[3];
	float maxs[3];
	float center[3];
} aas_area_t;

typedef struct {
	int planenum;
	int children[2]; /* >0: node index; 0: solid; <0: -(areanum+1) */
} aas_node_t;

/* ── Loaded data ─────────────────────────────────────────────────── */

typedef struct {
	aas_vertex_t *verts;
	int num_verts;
	aas_plane_t *planes;
	int num_planes;
	aas_edge_t *edges;
	int num_edges;
	int *edgeindex;
	int num_edgeindex;
	aas_face_t *faces;
	int num_faces;
	int *faceindex;
	int num_faceindex;
	aas_area_t *areas;
	int num_areas;
	aas_node_t *nodes;
	int num_nodes;
	void *raw; /* FS_ReadFile buffer — freed on Q3IDE_AAS_Free */
} q3ide_aas_t;

static q3ide_aas_t g_aas;
static qboolean g_aas_loaded = qfalse;

/* ── Helpers ─────────────────────────────────────────────────────── */

/* Horizontal right vector from a wall normal (same formula as layout.c). */
static void aas_right(const float *normal, float *right)
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

/* Lump pointer helper — returns pointer into raw buffer at lump offset. */
static void *aas_lump_ptr(const void *raw, const aas_lump_t *lump)
{
	return (char *) raw + lump->fileofs;
}

static int aas_point_area(const float *point); /* forward decl */

/* ── Public API ─────────────────────────────────────────────────── */

qboolean Q3IDE_AAS_IsLoaded(void)
{
	return g_aas_loaded;
}

int Q3IDE_AAS_PointArea(const vec3_t point)
{
	if (!g_aas_loaded)
		return 0;
	return aas_point_area(point);
}

void Q3IDE_AAS_Free(void)
{
	if (g_aas.raw) {
		FS_FreeFile(g_aas.raw);
		g_aas.raw = NULL;
	}
	memset(&g_aas, 0, sizeof(g_aas));
	g_aas_loaded = qfalse;
}

qboolean Q3IDE_AAS_Load(const char *mapname)
{
	char path[MAX_QPATH];
	void *buf = NULL;
	int len;
	aas_header_t *hdr;

	Q3IDE_AAS_Free();

	Com_sprintf(path, sizeof(path), "maps/%s.aas", mapname);
	len = FS_ReadFile(path, &buf);
	if (len <= 0 || !buf) {
		Q3IDE_LOGE("aas: failed to load '%s'", path);
		return qfalse;
	}

	hdr = (aas_header_t *) buf;
	if (hdr->ident != AASID) {
		Q3IDE_LOGE("aas: bad magic in '%s'", path);
		FS_FreeFile(buf);
		return qfalse;
	}
	if (hdr->version != AASVERSION) {
		Q3IDE_LOGE("aas: version %d (want %d) in '%s'", hdr->version, AASVERSION, path);
		FS_FreeFile(buf);
		return qfalse;
	}

	/* AAS v5 encrypts the lump table — decrypt in-place (same XOR as botlib AAS_DData) */
	{
		unsigned char *data = (unsigned char *) buf + 8;
		int dsize = (int) sizeof(aas_header_t) - 8;
		int i;
		for (i = 0; i < dsize; i++)
			data[i] ^= (unsigned char) (i * 119);
	}

	g_aas.raw = buf;

#define LOAD_LUMP(field, type, idx)                                                                                    \
	g_aas.field = (type *) aas_lump_ptr(buf, &hdr->lumps[idx]);                                                        \
	g_aas.num_##field = hdr->lumps[idx].filelen / (int) sizeof(type)

	LOAD_LUMP(verts, aas_vertex_t, AASLUMP_VERTEXES);
	LOAD_LUMP(planes, aas_plane_t, AASLUMP_PLANES);
	LOAD_LUMP(edges, aas_edge_t, AASLUMP_EDGES);
	LOAD_LUMP(faces, aas_face_t, AASLUMP_FACES);
	LOAD_LUMP(areas, aas_area_t, AASLUMP_AREAS);
	LOAD_LUMP(nodes, aas_node_t, AASLUMP_NODES);

#undef LOAD_LUMP

	/* Edge index and face index are int arrays */
	g_aas.edgeindex = (int *) aas_lump_ptr(buf, &hdr->lumps[AASLUMP_EDGEINDEX]);
	g_aas.num_edgeindex = hdr->lumps[AASLUMP_EDGEINDEX].filelen / (int) sizeof(int);
	g_aas.faceindex = (int *) aas_lump_ptr(buf, &hdr->lumps[AASLUMP_FACEINDEX]);
	g_aas.num_faceindex = hdr->lumps[AASLUMP_FACEINDEX].filelen / (int) sizeof(int);

	g_aas_loaded = qtrue;
	Q3IDE_LOGI("aas: loaded '%s' — %d areas, %d faces, %d nodes", path, g_aas.num_areas, g_aas.num_faces,
	           g_aas.num_nodes);
	return qtrue;
}

/* BSP traversal to find which AAS area contains 'point'.
 * Returns area index (1-based), or 0 if solid / not found. */
static int aas_point_area(const float *point)
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

int Q3IDE_AAS_GetAreaWalls(const vec3_t point, q3ide_aas_wall_t *walls, int maxwalls)
{
	int areanum, fi, nwalls = 0;
	const aas_area_t *area;

	if (!g_aas_loaded || !walls || maxwalls <= 0)
		return 0;

	areanum = aas_point_area(point);
	if (areanum <= 0 || areanum >= g_aas.num_areas) {
		Q3IDE_LOGE("aas: point (%.0f,%.0f,%.0f) not in any area (got %d)", point[0], point[1], point[2], areanum);
		return 0;
	}

	area = &g_aas.areas[areanum];
	Q3IDE_LOGI("aas: player in area %d (%d faces)", areanum, area->numfaces);

	for (fi = 0; fi < area->numfaces && nwalls < maxwalls; fi++) {
		int faceref, facenum;
		qboolean flipped;
		const aas_face_t *face;
		const aas_plane_t *plane;
		float normal[3], dist;
		float verts[64][3];
		int nverts = 0, ei;
		float centroid[3], right[3];
		float min_r, max_r, min_z, max_z, cr;
		int vi;

		if (area->firstface + fi >= g_aas.num_faceindex)
			break;

		faceref = g_aas.faceindex[area->firstface + fi];
		flipped = (faceref < 0);
		facenum = flipped ? -faceref : faceref;

		if (facenum <= 0 || facenum >= g_aas.num_faces)
			continue;

		face = &g_aas.faces[facenum];

		/* Only solid faces are actual walls */
		if (!(face->faceflags & FACE_SOLID))
			continue;

		if (face->planenum < 0 || face->planenum >= g_aas.num_planes)
			continue;

		plane = &g_aas.planes[face->planenum];

		/* Face normal direction: if faceref > 0, plane normal points INTO area.
		 * If flipped (faceref < 0), negate so it points into the area. */
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

		/* Skip floor and ceiling (nearly horizontal normals) */
		if (fabsf(normal[2]) > 0.4f)
			continue;

		/* Collect polygon vertices from edges */
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
			continue;

		/* Centroid = average of vertices, snapped to plane */
		centroid[0] = centroid[1] = centroid[2] = 0.0f;
		for (vi = 0; vi < nverts; vi++) {
			centroid[0] += verts[vi][0];
			centroid[1] += verts[vi][1];
			centroid[2] += verts[vi][2];
		}
		centroid[0] /= nverts;
		centroid[1] /= nverts;
		centroid[2] /= nverts;
		/* Snap to plane */
		{
			float cd = normal[0] * centroid[0] + normal[1] * centroid[1] + normal[2] * centroid[2] - dist;
			centroid[0] -= cd * normal[0];
			centroid[1] -= cd * normal[1];
			centroid[2] -= cd * normal[2];
		}

		aas_right(normal, right);

		/* Compute width (along right) and height (Z) from vertex extents */
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

		/* Dedup: skip if same normal as a wall already in the list */
		{
			int di;
			qboolean dup = qfalse;
			for (di = 0; di < nwalls; di++) {
				float dot =
				    walls[di].normal[0] * normal[0] + walls[di].normal[1] * normal[1] + walls[di].normal[2] * normal[2];
				if (dot > 0.85f) {
					dup = qtrue;
					break;
				}
			}
			if (dup)
				continue;
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
		nwalls++;
	}

	return nwalls;
}
