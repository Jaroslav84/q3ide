# Portal Rendering in Q3 Engine

## Overview
Portal rendering enables alternate camera views within the 3D scene (minimap, rear-view, view from another room). This doc covers the engine-level mechanics required for presentation style 6 (Portal).

## Core Structures

### refdef_t (render definition)
Passed to `RE_RenderScene()`, defines a single viewpoint:
```c
typedef struct {
	int x, y, width, height;        // viewport rectangle in screen pixels
	float fov_x, fov_y;             // field of view angles (degrees)
	vec3_t vieworg;                 // camera position in world space
	vec3_t viewaxis[3];             // rotation matrix: forward, right, up
	int time;                       // frame time in milliseconds
	int rdflags;                    // RDF_NOWORLDMODEL, etc
	byte areamask[MAX_MAP_AREA_BYTES]; // PVS visibility mask
	char text[8][32];               // shader deform text
} refdef_t;
```

### viewParms_t (view parameters, internal)
Computed from refdef_t, drives rendering pipeline:
```c
typedef struct {
	orientationr_t or;              // camera frame (origin + axis)
	orientationr_t world;           // world frame
	vec3_t pvsOrigin;               // PVS test point (may differ from or.origin)
	portalView_t portalView;        // PV_NONE, PV_PORTAL, or PV_MIRROR
	int viewportX, viewportY, viewportWidth, viewportHeight;
	int scissorX, scissorY, scissorWidth, scissorHeight;
	float fovX, fovY;
	float projectionMatrix[16];     // computed projection matrix
	cplane_t frustum[5];            // view frustum planes
	vec3_t visBounds[2];            // visible geometry bounds
	float zFar;
	stereoFrame_t stereoFrame;
} viewParms_t;
```

### orientationr_t
Coordinate frame (world position + orthonormal basis):
```c
typedef struct {
	vec3_t origin;                  // position in world space
	vec3_t axis[3];                 // forward, right, up vectors
	vec3_t viewOrigin;              // origin transformed to local coords
	float modelMatrix[16];          // 4x4 transform matrix
} orientationr_t;
```

## RE_RenderScene Call Flow

1. **Setup refdef_t** (game code)
   - Set `vieworg` and `viewaxis[0/1/2]` (forward/right/up unit vectors)
   - Set `x, y, width, height` for viewport
   - Set `fov_x, fov_y` (90° is standard)
   - Populate `areamask` for PVS culling

2. **RE_RenderScene(const refdef_t *fd)** (tr_scene.c:379)
   - Copies fd into `tr.refdef`
   - Sets up initial `viewParms_t` with `parms.portalView = PV_NONE`
   - Calls `R_RenderView(&parms)` (tr_main.c)

3. **R_RenderView(viewParms_t *parms)** (tr_main.c:1100)
   - Stores parms in `tr.viewParms` (global)
   - Calls `R_SetupFrustum()` to compute frustum planes
   - Calls `R_MarkLeaves()` for PVS culling
   - Iterates world BSP and adds visible surfaces to drawsurf list
   - Calls `RB_RenderView()` backend to rasterize drawsurfs

## Multi-viewport Rendering Pattern (Q3IDE)

For side-monitor views in multi-monitor mode, re-render with different cameras:

```c
// After main view renders, call RE_RenderScene again with different vieworg/viewaxis
// The cvar guard preserves entities/dlights for all passes:

if (ri.Cvar_VariableIntegerValue("r_multiViewRemaining") > 0) {
	// Multi-viewport mode: keep entities/dlights, only reset polys
	r_firstScenePoly = r_numpolys;  // tr_scene.c:518
} else {
	// Single view: reset all
	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;
}
```

## Portal/Mirror Rendering (Recursive Views)

Q3's native mirror/portal system (for in-world surfaces):

1. **Mark portal surfaces** during BSP traversal
2. **Compute mirror point** for reflection (R_MirrorPoint in tr_main.c)
3. **Recursive call** to `R_RenderView()` with inverted viewaxis
4. **Portal plane clip** to prevent infinite recursion

For Q3IDE, do NOT use this. Instead: render to texture, then composite.

## Rendering to Texture (FBO Approach)

Q3e has FBO support (tr_local.h: `#define USE_FBO`):

```c
// Pseudocode for portal window rendering:

// 1. Create or reuse target texture/framebuffer
//    (Use scratchImage[0-15] slots for video/texture frames)
image_t *target = tr.scratchImage[textureSlot];  // existing slot from RE_UploadCinematic

// 2. Set refdef for secondary viewpoint
refdef_t portalRefdef = {0};
portalRefdef.x = 0;
portalRefdef.y = 0;
portalRefdef.width = target->width;
portalRefdef.height = target->height;
portalRefdef.fov_x = 90.0f;
portalRefdef.fov_y = 90.0f;
VectorCopy(cameraPos, portalRefdef.vieworg);
VectorCopy(cameraForward, portalRefdef.viewaxis[0]);
VectorCopy(cameraRight, portalRefdef.viewaxis[1]);
VectorCopy(cameraUp, portalRefdef.viewaxis[2]);
// Copy areamask from main view (or compute via CM_LeafAreaNum)

// 3. Bind FBO, render scene
FBO_Bind(targetFBO);
RE_RenderScene(&portalRefdef);
FBO_Unbind();

// 4. Draw textured quad in main scene
//    (polys with shader pointing to target texture)
//    Use RE_AddPolyToScene() with the portal texture shader
```

## Viewport Math

Setting `refdef.x/y/width/height` controls where scene renders within framebuffer:

- **x, y**: Top-left corner in pixels (0-based, y=0 is top)
- **width, height**: Dimensions in pixels
- **FOV**: `fov_x` typically 90° for square aspect; `fov_y` adjusted for non-1:1 aspect

For portrait displays:
```c
// Example: 400x600 portal on 1600x900 main view
refdef.x = 100;
refdef.y = 150;
refdef.width = 400;
refdef.height = 600;
float aspect = (float)refdef.width / refdef.height;
refdef.fov_x = 90.0f;
refdef.fov_y = refdef.fov_x / aspect;  // narrower FOV for tall window
```

## Mirror Surface Handling (Reference)

Native Q3 mirror surfaces use `surfaceparm portal`:
- Shader declares with `portal` keyword in `.shader` file
- Engine detects during surface loading (tr_world.c)
- At render time: clips geometry, inverts normals, recursively renders

For Q3IDE portals: **ignore this**—use FBO + explicit camera control instead.

## Integration Checklist

1. **Capture camera state** (minimap view direction, rear angle offset, room origin)
2. **Compute secondary vieworg/viewaxis** from game state
3. **Setup refdef_t** with target dimensions and FoV
4. **Call RE_RenderScene()** with portal refdef
5. **Composite result** back to main framebuffer via shader poly
6. **Handle viewport clipping** if portal window is partially off-screen

Key files:
- `/root/Projects/q3ide/quake3e/code/renderer/tr_scene.c` — RE_RenderScene entry
- `/root/Projects/q3ide/quake3e/code/renderer/tr_main.c` — R_RenderView, R_MirrorPoint (reference only)
- `/root/Projects/q3ide/quake3e/code/q3ide/q3ide_hooks.c` — engine integration hook point
