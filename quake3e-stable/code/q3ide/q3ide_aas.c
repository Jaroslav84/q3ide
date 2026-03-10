/*
 * q3ide_aas.c — AAS file loader (parse + lump extraction).
 * Wall extraction: q3ide_aas_face.c + q3ide_aas_walls.c.
 */

#include "q3ide_aas.h"
#include "q3ide_aas_fmt.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include <string.h>

/* BSP point-area query — defined in q3ide_aas_query.c */
int q3ide_aas_point_area(const float *point);

/* Lump pointer helper */
static void *q3ide_aas_lump_ptr(const void *raw, const aas_lump_t *lump)
{
	return (char *) raw + lump->fileofs;
}

q3ide_aas_t g_aas;
qboolean g_aas_loaded = qfalse;

qboolean Q3IDE_AAS_IsLoaded(void)
{
	return g_aas_loaded;
}

int Q3IDE_AAS_PointArea(const vec3_t point)
{
	if (!g_aas_loaded)
		return 0;
	return q3ide_aas_point_area(point);
}

void Q3IDE_AAS_Free(void)
{
	if (g_aas.raw) {
		Z_Free(g_aas.raw);
		g_aas.raw = NULL;
	}
	memset(&g_aas, 0, sizeof(g_aas));
	g_aas_loaded = qfalse;
}

qboolean Q3IDE_AAS_Load(const char *mapname)
{
	char path[MAX_QPATH];
	void *tmp = NULL;
	void *buf;
	int len;
	aas_header_t *hdr;

	Q3IDE_AAS_Free();

	Com_sprintf(path, sizeof(path), "maps/%s.aas", mapname);
	len = FS_ReadFile(path, &tmp);
	if (len <= 0 || !tmp) {
		Q3IDE_LOGE("aas: failed to load '%s'", path);
		return qfalse;
	}

	hdr = (aas_header_t *) tmp;
	if (hdr->ident != AASID) {
		Q3IDE_LOGE("aas: bad magic in '%s'", path);
		FS_FreeFile(tmp);
		return qfalse;
	}
	if (hdr->version != AASVERSION) {
		Q3IDE_LOGE("aas: version %d (want %d) in '%s'", hdr->version, AASVERSION, path);
		FS_FreeFile(tmp);
		return qfalse;
	}

	/* Copy out of temp-hunk before the engine reclaims it, then free immediately. */
	buf = Z_Malloc(len);
	Com_Memcpy(buf, tmp, len);
	FS_FreeFile(tmp);
	hdr = (aas_header_t *) buf;

	/* AAS v5 encrypts the lump table — decrypt in-place (XOR as in botlib AAS_DData) */
	{
		unsigned char *data = (unsigned char *) buf + 8;
		int dsize = (int) sizeof(aas_header_t) - 8;
		int i;
		for (i = 0; i < dsize; i++)
			data[i] ^= (unsigned char) (i * 119);
	}

	g_aas.raw = buf;

#define LOAD_LUMP(field, type, idx)                                                                                    \
	g_aas.field = (type *) q3ide_aas_lump_ptr(buf, &hdr->lumps[idx]);                                                  \
	g_aas.num_##field = hdr->lumps[idx].filelen / (int) sizeof(type)

	LOAD_LUMP(verts, aas_vertex_t, AASLUMP_VERTEXES);
	LOAD_LUMP(planes, aas_plane_t, AASLUMP_PLANES);
	LOAD_LUMP(edges, aas_edge_t, AASLUMP_EDGES);
	LOAD_LUMP(faces, aas_face_t, AASLUMP_FACES);
	LOAD_LUMP(areas, aas_area_t, AASLUMP_AREAS);
	LOAD_LUMP(nodes, aas_node_t, AASLUMP_NODES);

#undef LOAD_LUMP

	g_aas.edgeindex = (int *) q3ide_aas_lump_ptr(buf, &hdr->lumps[AASLUMP_EDGEINDEX]);
	g_aas.num_edgeindex = hdr->lumps[AASLUMP_EDGEINDEX].filelen / (int) sizeof(int);
	g_aas.faceindex = (int *) q3ide_aas_lump_ptr(buf, &hdr->lumps[AASLUMP_FACEINDEX]);
	g_aas.num_faceindex = hdr->lumps[AASLUMP_FACEINDEX].filelen / (int) sizeof(int);

	g_aas_loaded = qtrue;
	Q3IDE_LOGI("aas: loaded '%s' — %d areas, %d faces, %d nodes", path, g_aas.num_areas, g_aas.num_faces,
	           g_aas.num_nodes);
	return qtrue;
}
