/*
 * q3ide_aas_fmt.h — AAS binary format constants and types.
 * Shared by q3ide_aas.c, q3ide_aas_query.c, q3ide_aas_face.c.
 */

#ifndef Q3IDE_AAS_FMT_H
#define Q3IDE_AAS_FMT_H

#define AASID      (('S' << 24) + ('A' << 16) + ('A' << 8) + 'E')
#define AASVERSION 5
#define AAS_LUMPS  14

#define AASLUMP_BBOXES       0
#define AASLUMP_VERTEXES     1
#define AASLUMP_PLANES       2
#define AASLUMP_EDGES        3
#define AASLUMP_EDGEINDEX    4
#define AASLUMP_FACES        5
#define AASLUMP_FACEINDEX    6
#define AASLUMP_AREAS        7
#define AASLUMP_AREASETTINGS 8
#define AASLUMP_REACHABILITY 9
#define AASLUMP_NODES        10
#define AASLUMP_PORTALS      11
#define AASLUMP_PORTALINDEX  12
#define AASLUMP_CLUSTERS     13

#define FACE_SOLID  1
#define FACE_GROUND 4
#define FACE_GAP    8

typedef struct {
	int fileofs, filelen;
} aas_lump_t;

typedef struct {
	int ident, version, bspchecksum;
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
	int planenum, faceflags, numedges, firstedge, frontarea, backarea;
} aas_face_t;

typedef struct {
	int areanum, numfaces, firstface;
	float mins[3], maxs[3], center[3];
} aas_area_t;

typedef struct {
	int planenum;
	int children[2]; /* >0: node; 0: solid; <0: -(areanum+1) */
} aas_node_t;

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

#endif /* Q3IDE_AAS_FMT_H */
