# Quake 3 BSP Format, Surface Types, and Traces

> **Sources:**
> - [Unofficial Quake 3 Map Specs](http://www.mralligator.com/q3/)
> - [Unofficial Quake 3 BSP Format (Hyrtwol)](http://www.hyrtwol.dk/wolfenstein/unofficial_quake3_bsp_format.md)
> - [flipcode - Simple Quake3 BSP Loader](https://www.flipcode.com/archives/Simple_Quake3_BSP_Loader.shtml)
> - [Quake III Arena Source Code](https://github.com/id-Software/Quake-III-Arena)
> - [GameDev.net - Rendering Quake 3 BSP Geometry](https://gamedev.net/forums/topic/641754-rendering-quake-3-bsp-geometry/5052962/)

## BSP File Format Overview

The Quake 3 BSP file format (`.bsp`) stores all map geometry, textures, lighting, and collision data in a single binary file. The file consists of a header followed by 17 lumps (data sections).

### File Header

```c
typedef struct {
    char    magic[4];      // "IBSP"
    int     version;       // 0x2E (46) for Quake 3
} dheader_t;
```

After the magic number and version, the header contains a directory of 17 lump entries:

```c
typedef struct {
    int     offset;        // Offset from start of file
    int     length;        // Length of lump data in bytes
} lump_t;

// Header contains: lump_t lumps[17];
```

## The 17 Lumps

| Index | Name | Description |
|-------|------|-------------|
| 0 | **Entities** | Game entity descriptions (text format) |
| 1 | **Textures** | Surface texture/shader references with flags |
| 2 | **Planes** | Splitting planes for BSP tree |
| 3 | **Nodes** | BSP tree internal nodes |
| 4 | **Leaves** | BSP tree leaf nodes (convex regions) |
| 5 | **Leaf Faces** | Indices mapping leaves to faces |
| 6 | **Leaf Brushes** | Indices mapping leaves to brushes |
| 7 | **Models** | Brush models (doors, platforms, etc.) |
| 8 | **Brushes** | Convex collision volumes |
| 9 | **Brush Sides** | Planes defining brush boundaries |
| 10 | **Vertices** | Vertex positions, normals, UVs, colors |
| 11 | **Mesh Vertices** | Index offsets for face triangulation |
| 12 | **Effects** | Fog/effect volumes |
| 13 | **Faces** | Renderable surface descriptions |
| 14 | **Lightmaps** | Pre-baked lighting textures (128x128 RGB) |
| 15 | **Light Grid** | Volumetric lighting for entities |
| 16 | **Visibility** | PVS (Potentially Visible Set) data |

## Key Lump Structures

### Textures (Lump 1)

```c
typedef struct {
    char    name[64];      // Shader/texture path (e.g., "textures/base_wall/concrete")
    int     flags;         // Surface flags
    int     contents;      // Content flags
} dtexture_t;
```

#### Surface Flags

```c
#define SURF_NODAMAGE       0x1        // Never give falling damage
#define SURF_SLICK          0x2        // Effects game physics (slippery)
#define SURF_SKY            0x4        // Lighting from environment map
#define SURF_LADDER         0x8        // Climbable surface
#define SURF_NOIMPACT       0x10       // Don't make missile explosion marks
#define SURF_NOMARKS        0x20       // Don't leave impact marks
#define SURF_FLESH          0x40       // Make flesh sounds and effects
#define SURF_NODRAW         0x80       // Don't generate a drawsurface at all
#define SURF_HINT           0x100      // Make a primary BSP splitter
#define SURF_SKIP           0x200      // Completely ignore, allowing non-closed brushes
#define SURF_NOLIGHTMAP     0x400      // Surface doesn't need a lightmap
#define SURF_POINTLIGHT     0x800      // Generate lighting info at vertices
#define SURF_METALSTEPS     0x1000     // Clanking footsteps
#define SURF_NOSTEPS        0x2000     // No footstep sounds
#define SURF_NONSOLID       0x4000     // Don't collide against curves
#define SURF_LIGHTFILTER    0x8000     // Act as a light filter during map compile
#define SURF_ALPHASHADOW    0x10000    // Do per-pixel light shadow casting
#define SURF_NODLIGHT       0x20000    // Don't receive dynamic lights
#define SURF_DUST           0x40000    // Leave a dust trail when walking
```

#### Content Flags

```c
#define CONTENTS_SOLID          1      // Blocks movement and traces
#define CONTENTS_LAVA           8
#define CONTENTS_SLIME          16
#define CONTENTS_WATER          32
#define CONTENTS_FOG            64
#define CONTENTS_PLAYERCLIP     0x10000
#define CONTENTS_MONSTERCLIP    0x20000
#define CONTENTS_TELEPORTER     0x40000
#define CONTENTS_JUMPPAD        0x80000
#define CONTENTS_CLUSTERPORTAL  0x100000
#define CONTENTS_DONOTENTER     0x200000
#define CONTENTS_ORIGIN         0x1000000   // Removed during BSP compile
#define CONTENTS_BODY           0x2000000   // Used for collision
#define CONTENTS_CORPSE         0x4000000
#define CONTENTS_DETAIL         0x8000000   // Brushes not used for BSP splits
#define CONTENTS_STRUCTURAL     0x10000000  // Brushes used for BSP splits
#define CONTENTS_TRANSLUCENT    0x20000000  // Auto-set if any surface has trans
#define CONTENTS_TRIGGER        0x40000000
#define CONTENTS_NODROP         0x80000000  // Don't leave bodies or items
```

### Planes (Lump 2)

```c
typedef struct {
    float   normal[3];     // Plane normal vector
    float   dist;          // Distance from origin along normal
} dplane_t;
```

### Nodes (Lump 3)

```c
typedef struct {
    int     planeNum;      // Index into planes lump (splitting plane)
    int     children[2];   // Children indices (negative = -(leaf+1))
    int     mins[3];       // Bounding box minimum
    int     maxs[3];       // Bounding box maximum
} dnode_t;
```

If `children[i]` is negative, then `-(children[i] + 1)` is the index into the leaves array.

### Leaves (Lump 4)

```c
typedef struct {
    int     cluster;           // Visibility cluster index (-1 = not visible)
    int     area;              // Area portal area
    int     mins[3];           // Bounding box minimum
    int     maxs[3];           // Bounding box maximum
    int     leafface;          // First leaf face index
    int     n_leaffaces;       // Number of leaf faces
    int     leafbrush;         // First leaf brush index
    int     n_leafbrushes;     // Number of leaf brushes
} dleaf_t;
```

### Faces (Lump 13)

```c
typedef struct {
    int     textureIndex;      // Index into textures lump
    int     effectIndex;       // Index into effects lump (-1 = none)
    int     type;              // Face type (1=polygon, 2=patch, 3=mesh, 4=billboard)
    int     vertex;            // First vertex index
    int     n_vertexes;        // Number of vertices
    int     meshvert;          // First meshvert index
    int     n_meshverts;       // Number of meshverts
    int     lm_index;          // Lightmap index (-1 = none)
    int     lm_start[2];      // Lightmap corner in atlas (x, y)
    int     lm_size[2];       // Lightmap size (width, height)
    float   lm_origin[3];     // World space origin of lightmap
    float   lm_vecs[2][3];    // World space lightmap s and t unit vectors
    float   normal[3];        // Surface normal
    int     size[2];          // Patch dimensions (width, height)
} dface_t;
```

## Face Types

There are **four types of faces** in a Quake 3 BSP:

### Type 1: Polygons

Standard planar surfaces. `vertex` and `n_vertexes` describe a set of vertices forming a polygon (always contains a loop of vertices, sometimes with an additional center vertex). `meshvert` and `n_meshverts` describe triangulation -- every three meshverts form a triangle, and each meshvert is an offset from the first vertex.

### Type 2: Patches (Bezier Curves)

Curved surfaces defined by a 2D grid of control points. `vertex` and `n_vertexes` describe the control point grid with dimensions given by `size[0]` (width) and `size[1]` (height). The engine tessellates these at runtime based on `r_subdivisions` cvar.

### Type 3: Meshes (Triangle Soups)

Pre-triangulated meshes. Like polygons, but used for more complex geometry that does not lie on a single plane.

### Type 4: Billboards (Flares)

Billboards/flares that always face the camera. Used for light sources and effects.

## Vertices (Lump 10)

```c
typedef struct {
    float   position[3];      // World space position (x, y, z)
    float   texcoord[2][2];   // [0]=diffuse UV, [1]=lightmap UV
    float   normal[3];        // Vertex normal
    byte    color[4];         // RGBA vertex color
} dvertex_t;
```

### Mesh Vertices (Lump 11)

```c
typedef struct {
    int     offset;            // Offset from first vertex of face
} dmeshvert_t;
```

Meshverts provide triangle indices relative to the face's first vertex. For a face starting at vertex `v` with meshverts `[0, 2, 1, 2, 3, 1]`, the triangles are `(v+0, v+2, v+1)` and `(v+2, v+3, v+1)`.

## Brushes and Collision (Lumps 8-9)

### Brushes (Lump 8)

```c
typedef struct {
    int     brushside;         // First brush side index
    int     n_brushsides;      // Number of brush sides
    int     textureIndex;      // Texture index (for content flags)
} dbrush_t;
```

Brushes are **convex volumes** defined by their surrounding planes. They are used exclusively for collision detection (not rendering).

### Brush Sides (Lump 9)

```c
typedef struct {
    int     planeIndex;        // Index into planes lump
    int     textureIndex;      // Texture index (for surface flags)
} dbrushside_t;
```

## Trace System

The trace system is how Quake 3 performs collision detection -- casting rays and swept volumes against BSP geometry.

### CM_BoxTrace

```c
void CM_BoxTrace(trace_t *results,
                 const vec3_t start,
                 const vec3_t end,
                 const vec3_t mins,       // AABB minimum extents
                 const vec3_t maxs,       // AABB maximum extents
                 clipHandle_t model,
                 int brushmask);          // Content flags to collide with
```

For a simple ray trace (point trace), `mins` and `maxs` are both `vec3_origin` (0,0,0).

### trace_t Result Structure

```c
typedef struct {
    qboolean    allsolid;      // If true, trace started inside a solid
    qboolean    startsolid;    // If true, start point was inside a solid
    float       fraction;      // Time of impact (0.0 to 1.0, 1.0 = no hit)
    vec3_t      endpos;        // Final position of trace
    cplane_t    plane;         // Surface plane at impact point
    int         surfaceFlags;  // Surface flags of hit surface
    int         contents;      // Contents of hit volume
    int         entityNum;     // Entity that was hit
} trace_t;
```

### BSP Tree Traversal for Traces

The trace algorithm:

1. Start at the root node
2. Test the trace segment against the node's splitting plane
3. If entirely on one side, recurse into that child
4. If it crosses the plane, recurse into both children (near side first)
5. At leaf nodes, test against all brushes in the leaf
6. For each brush, test the trace segment against all brush side planes
7. Track the closest intersection (smallest `fraction`)

### Brush Mask Filtering

The `brushmask` parameter controls which brushes are tested:

```c
// Common masks
#define MASK_ALL          (-1)                          // Everything
#define MASK_SOLID        (CONTENTS_SOLID)              // World geometry only
#define MASK_PLAYERSOLID  (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY)
#define MASK_SHOT         (CONTENTS_SOLID | CONTENTS_BODY)
#define MASK_WATER        (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)
```

## Visibility System (Lump 16)

### PVS (Potentially Visible Set)

The visibility data stores which clusters can see which other clusters:

```c
typedef struct {
    int     n_vecs;            // Number of vectors (clusters)
    int     sz_vecs;           // Size of each vector in bytes
    // Followed by n_vecs * sz_vecs bytes of bit vectors
} dvis_t;
```

Each bit in a cluster's visibility vector indicates whether another cluster is potentially visible. The renderer uses this to skip entire sections of the map that cannot be seen.

### Cluster Lookup

1. Find the leaf containing the camera using BSP tree traversal
2. Get the cluster index from the leaf
3. Look up the PVS bit vector for that cluster
4. For each leaf to render, check if its cluster bit is set

## Relevance to Q3IDE

### Wall Finding via Traces

Q3IDE needs to find walls to project captured windows onto. The trace system provides this:

```c
trace_t trace;
vec3_t forward, start, end;

// Get player view direction
AngleVectors(cg.refdefViewAngles, forward, NULL, NULL);
VectorCopy(cg.refdef.vieworg, start);
VectorMA(start, 4096, forward, end);  // Cast 4096 units forward

// Trace against world geometry
trap_CM_BoxTrace(&trace, start, end, vec3_origin, vec3_origin, 0, MASK_SOLID);

if (trace.fraction < 1.0) {
    // trace.endpos = wall hit point
    // trace.plane.normal = wall surface normal
    // trace.surfaceFlags = surface properties
}
```

### Surface Identification

The `surfaceFlags` from the trace result can be used to:
- Avoid placing windows on sky surfaces (`SURF_SKY`)
- Avoid non-drawn surfaces (`SURF_NODRAW`)
- Identify suitable wall surfaces for projection

### Quad Placement

Using `trace.endpos` and `trace.plane.normal`, Q3IDE can construct a quad (4 vertices) aligned to the wall surface. The normal tells you which way the wall faces, and the hit point provides the anchor position. The quad can then be rendered using `trap_R_AddPolyToScene`.
