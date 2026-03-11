# Q3IDE — Architecture

> **READ THIS FIRST. Every AI agent reads this before touching any file.**
> This is the contract for all 150+ features across 24 batches.
> `spatial/` is the brain. It never calls engine internals directly. Ever.

---

## Principles

### FOUNDATION
1. **Typed enums over boolean soup** — LLM reads enums like English, reasons about boolean combinations wrong
2. **Clean flags** — organized under typed objects, not a flat struct
3. **Compiler-enforced contracts** — missing enum cases = build error, compiler is your checklist
4. **-Wswitch compiler flag** — one Makefile line, catches every missing enum case forever
5. **200 line file limit** — every file stays small, AI-friendly
6. **lint.sh extended** — hard build failure on 200 line violations + magic numbers in .c files
7. **q3ide_params.h discipline** — no magic numbers in .c files ever, enforced by lint

### STRUCTURE
8. **Scene graph base object** — everything in batches 1-24 is a scene object, get this wrong and refactor 150 features
9. **Object lifecycle contract** — every object has Create/Destroy/Update/Render, same pattern everywhere
10. **Scene object ID system** — stable unique IDs across frames, required for spawning/multiplayer/agents
11. **Window type system** — Portal, Ornament, Widget, Billboard all land in batches 6-9, do it now
12. **Window visibility scope** — PUBLIC/PRIVATE/TEAM flag on Window, required for multiplayer and pair programming
13. **SpaceWindowView** — same window different position per space, batch 7 will hurt without this
14. **Subclassing** — Window has subtypes, Scene objects have subtypes, extendable
15. **Separation of concerns** — placement code doesn't touch render, render doesn't touch capture
16. **Placeholder slots** — architecture defines the contract even if implementation is TBD
17. **Stub files for all future object types** — empty Portal, Widget, Ornament, UMLNode, RuntimeGeometry, Agent, Audio, Space files
18. **Wall cache** — owned by Space, CachedWall_t + WallSlot_t, built on area entry, invalidated on exit
19. **Multi-monitor render contract** — render dispatch is monitor-agnostic, scene graph renders once, viewport loop handles monitor offsets

### EXTENSIBILITY
20. **No spaghetti wiring** — no 6-file tax per feature
21. **Feature registration** — new feature = new file, nothing else touched (dev-time)
22. **Runtime registration** — objects spawn into scene graph at runtime, no recompile (runtime)
23. **Flexibility** — add new object types without touching existing code

### RUNTIME
24. **On-the-fly spawning** — AI creates objects at runtime, no restart
25. **Error state on every object** — every scene object has error flag + message, AI objects can't fail silently
26. **Debug visualization layer** — /q3ide_debug draws bounding boxes, normals, flags on all scene objects

### QUALITY
27. **UI design system** — AI follows fixed primitives, can't freestyle raw quads
28. **Rendering standards** — shader rules, mipmaps, normals, visual contract for all 3D objects

---

## Three-Layer Architecture

```
┌──────────────────────────────────────────────────────┐
│                  CAPTURE LAYER                        │
│   capture/  —  Rust dylib, ScreenCaptureKit           │
│   Exposes only C-ABI via q3ide_capture.h              │
│   Never imports engine or spatial headers             │
└─────────────────────┬────────────────────────────────┘
                      │ C-ABI frames only
┌─────────────────────▼────────────────────────────────┐
│                  Q3IDE BRAIN                          │
│   spatial/  —  100% engine-agnostic C99               │
│   Scene graph, Windows, Spaces, Placement,            │
│   Features, UML, Agents, Audio, quakeOS               │
│   Calls engine ONLY via engine/adapter.h              │
│   Calls capture ONLY via q3ide_capture.h              │
│   NEVER calls trap_*, re.*, CG_*, CM_*, qgl*          │
└─────────────────────┬────────────────────────────────┘
                      │ adapter calls only
┌─────────────────────▼────────────────────────────────┐
│                  ENGINE ADAPTER                       │
│   engine/adapter.h     — abstract interface           │
│   engine/quake3e/      — current implementation      │
│   engine/quake4/       — future                       │
│   engine/unreal/       — future                       │
│   engine/ar/           — future                       │
│   Minimal hooks in quake3e guarded by #ifdef Q3IDE    │
└──────────────────────────────────────────────────────┘
```

**Portability rule:** Swap `engine/quake3e/` for `engine/unreal/` and the Q3IDE brain
works unchanged. `spatial/` never changes when the engine changes.

---

## How Q3IDE Hooks Into Quake3e

### Quake3e Module Boundaries

Quake3e has three loadable VM modules and one engine executable. Understanding which layer
Q3IDE lives in is critical — it determines what we can call directly vs. what must go through
the trap/VM boundary.

```
quake3e (executable)
├── code/renderer/     — OpenGL renderer, statically linked on macOS
│   ├── tr_main.c      — Frontend: scene rendering, culling, entity processing
│   ├── tr_backend.c   — Backend: OpenGL state, draw calls, shader execution
│   ├── tr_image.c     — Texture creation, Upload32, R_UploadSubImage
│   ├── tr_scene.c     — RE_RenderScene, RE_AddPolyToScene
│   ├── tr_shader.c    — Shader script parsing, R_FindShader
│   └── tr_init.c      — Renderer init, refexport_t population
├── code/client/
│   ├── cl_main.c      — CL_Init(), CL_Frame(), CL_ShutdownAll()
│   ├── cl_cgame.c     — cgame VM loader, CL_CgameSystemCalls() dispatcher
│   └── cl_scrn.c      — SCR_UpdateScreen()
├── cgame  (QVM/DLL)   — Client game logic — Q3IDE BYPASSES THIS ENTIRELY
├── game   (QVM/DLL)   — Server game logic
└── q3_ui  (QVM/DLL)   — Menus
```

**Q3IDE is engine-side code** — it lives in `quake3e` (the executable), not in any QVM.
This means:
- Direct access to the renderer via `refexport_t re` function table
- Direct access to collision via `CM_BoxTrace()` in `cm_trace.c`
- No VM trap boundary overhead on any per-frame call
- Works with every mod (baseq3, CPMA, OSP) because it bypasses cgame entirely

### Renderer Access: refexport_t and refimport_t

The renderer is a static library on macOS (no `USE_RENDERER_DLOPEN`). The engine and
renderer exchange two function tables at init time inside `CL_InitRef()`:

```c
// Engine constructs this and passes to renderer
refimport_t ri;
ri.Cmd_AddCommand = Cmd_AddCommand;
ri.Cvar_Get       = Cvar_Get;
ri.FS_ReadFile    = FS_ReadFile;
ri.Hunk_Alloc     = Hunk_Alloc;
ri.Printf         = CL_RefPrintf;
// ... more function pointers

// Renderer fills this and returns it to the engine
refexport_t re;
re = GetRefAPI( REF_API_VERSION, &ri );

// Now the engine (and Q3IDE adapter) can call:
//   re.RegisterShader()      — tr_shader.c
//   re.AddPolyToScene()      — tr_scene.c
//   re.RenderScene()         — tr_main.c
//   re.BeginFrame()          — tr_backend.c
//   re.EndFrame()            — tr_backend.c
```

The `re` global is the only legitimate way `engine/quake3e/q3ide_adapter.c` calls the
renderer. `spatial/` never touches `re` directly.

### Hook Point 1: CL_Init() → Q3IDE_Init()

```c
// cl_main.c
void CL_Init( void ) {
    // ... engine cvar/command registration ...

    Q3IDE_Init();   // ← Q3IDE: load capture dylib, register cvars and commands
                    //   Cmd_AddCommand("q3ide_list",   Q3IDE_List_f)
                    //   Cmd_AddCommand("q3ide_attach", Q3IDE_Attach_f)
                    //   Cmd_AddCommand("q3ide_detach", Q3IDE_Detach_f)
                    //   Cmd_AddCommand("q3ide_status", Q3IDE_Status_f)
                    //   Cmd_AddCommand("q3ide_debug",  Q3IDE_Debug_f)
                    //   Does NOT create textures yet — renderer not ready
}
```

### Hook Point 2: CL_InitRenderer() → Q3IDE_InitTextures()

```c
// cl_main.c
static void CL_InitRenderer( void ) {
    CL_InitRef();                        // Loads renderer, populates re.*
    re.BeginRegistration( &cls.glconfig );

    cls.charSetShader = re.RegisterShader( "gfx/2d/bigchars" );
    cls.whiteShader   = re.RegisterShader( "white" );

    Q3IDE_InitTextures();  // ← Q3IDE: renderer is now ready
                           //   R_CreateImage("*q3ide_0", ...) for each window slot
                           //   re.RegisterShader("q3ide/window0") etc.
}
```

### Hook Point 3: CL_Frame() → Q3IDE_UpdateCaptures()

The exact sequence in `CL_Frame()` every frame:

```c
void CL_Frame( int msec, int realMsec ) {
    // ... timing, networking, send commands ...

    cls.framecount++;

    Q3IDE_UpdateCaptures();  // ← Q3IDE: drain ring buffer, upload to GPU
                             //   For each window with a new frame:
                             //     R_UploadSubImage(frame_data, 0, 0, w, h, image)
                             //     → qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                             //                        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data)

    SCR_UpdateScreen();      // ← Full render pipeline fires with fresh textures
    S_Update( realMsec );
    Con_RunConsole();
}
```

**Why before SCR_UpdateScreen():** The GPU texture must be updated before the render pipeline
reads it. `Q3IDE_UpdateCaptures()` uploads texture data; `SCR_UpdateScreen()` uses it.

### Hook Point 4: CL_ShutdownAll() → Q3IDE_Shutdown()

```c
void CL_ShutdownAll( void ) {
    // ... curl, audio, VMs, renderer shutdown ...
    Q3IDE_Shutdown();  // ← Q3IDE: free capture textures, unload dylib
}
```

### SCR_UpdateScreen Call Chain

Full call chain from `CL_Frame()` to where Q3IDE injects quads:

```
CL_Frame()
  → Q3IDE_UpdateCaptures()           ← upload textures here
  → SCR_UpdateScreen()               cl_scrn.c
    → re.BeginFrame()                tr_backend.c
    → CL_DrawActiveGame()            cl_cgame.c
      → VM_Call(cgvm, CG_DRAW_ACTIVE_FRAME)
        → CG_DrawActiveFrame()       cgame/cg_main.c (in QVM, not Q3IDE)
          → trap_R_RenderScene()     → CL_CgameSystemCalls() → re.RenderScene()
            → RE_RenderScene()       tr_main.c / tr_scene.c
              ← Q3IDE injects quads here via re.AddPolyToScene()
              ← before RE_RenderScene processes the command queue
    → SCR_DrawConsole()
    → re.EndFrame()                  tr_backend.c — swap buffers
```

Q3IDE's `Q3IDE_DrawPanels()` calls `re.AddPolyToScene(shader, 4, polyVert_t[4])` before
`RE_RenderScene()` flushes the command queue. The quads render as part of the normal 3D scene.

---

## Texture System: Scratch Slots

### Exact API

Q3IDE uses `tr_image.c` functions directly through the adapter:

```c
// At init — one call per window slot
image_t *img = R_CreateImage(
    "*q3ide_0",                          // Name — asterisk = internal/dynamic
    NULL,                                // name2 (unused for dynamic textures)
    blank_pixels,                        // Initial pixel data
    width, height,
    IMGFLAG_CLAMPTOEDGE |               // GL_CLAMP_TO_EDGE — no tiling
    IMGFLAG_NOSCALE     |               // Don't force power-of-2
    IMGFLAG_NOLIGHTSCALE                // Don't gamma-shift screen content
    // NOT IMGFLAG_MIPMAP  — no mipmap chain on live captures
    // NOT IMGFLAG_PICMIP  — never reduce capture resolution
);

// Per frame — called from Q3IDE_UpdateCaptures() for each window
R_UploadSubImage(
    frame_data,                          // BGRA pixels from SCK ring buffer
    0, 0,                               // x, y offset (full update)
    width, height,
    img
);
// Internally calls: Upload32(data, 0, 0, w, h, image, subImage=qtrue)
// Which calls:      qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
//                                    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data)
```

### Why GL_BGRA + GL_UNSIGNED_INT_8_8_8_8_REV

ScreenCaptureKit delivers frames natively as BGRA. `GL_BGRA` with `GL_UNSIGNED_INT_8_8_8_8_REV`
is the GPU fast path on macOS — zero per-pixel swizzle cost, direct DMA path. Using GL_RGBA
would require a full pixel swizzle pass on the CPU.

### Analogy: RE_UploadCinematic

Q3IDE's scratch slot system is directly analogous to Q3's cinematic texture path:
- ROQ video: `RE_UploadCinematic(w, h, cols, rows, data, client, dirty)` → `glTexSubImage2D`
- Q3IDE: `R_UploadSubImage(data, 0, 0, w, h, img)` → `glTexSubImage2D`
- Same mechanism, different data source (ROQ decoder vs SCK ring buffer)

### Scratch Slot Naming

Each Window has a `scratch_slot` (integer N). The naming chain:

```
scratch_slot N
    → image name    "*q3ide_N"           (in renderer image hash table)
    → shader name   "q3ide/windowN"      (registered via re.RegisterShader)
    → shader file   baseq3/shaders/q3ide/windows.shader
    → qhandle_t     stored in Window_t.shader
```

---

## Wall Geometry: From Trace to Quad

### CM_BoxTrace

The adapter wraps `CM_BoxTrace()` from `cm_trace.c`:

```c
// Point ray trace against world geometry
trace_t trace;
CM_BoxTrace(
    &trace,
    start,          // vec3_t ray origin (player position)
    end,            // vec3_t ray end (player position + forward * 4096)
    vec3_origin,    // mins — zero for point trace
    vec3_origin,    // maxs — zero for point trace
    0,              // model — 0 = world BSP
    MASK_SOLID      // brushmask = CONTENTS_SOLID only
);

// Results:
// trace.fraction      < 1.0  → hit something
// trace.endpos        → world-space hit point on wall surface
// trace.plane.normal  → outward normal of the hit surface
// trace.surfaceFlags  → BSP surface flags for the hit brush side
// trace.contents      → content flags of the hit volume
```

### Surface Filtering

`spatial/space/wall_scanner.c` uses `surfaceFlags` and `contents` from the trace to
reject invalid surfaces. The flag values come from `q3ide_params.h` (mirrored from
`surfaceflags.h` — `spatial/` never includes engine headers):

```c
// Surfaces to SKIP — not valid wall targets for window projection
// SURF_SKY    0x4   — environment map, not real geometry
// SURF_NODRAW 0x80  — invisible brushes (clips, triggers)
// SURF_HINT   0x100 — BSP hint planes, not renderable
// SURF_SKIP   0x200 — non-closed brush helpers
#define Q3IDE_SKIP_SURFFLAGS  (SURF_SKY | SURF_NODRAW | SURF_HINT | SURF_SKIP)

qboolean Wall_IsValidSurface(int surfaceFlags, int contents) {
    if (surfaceFlags & Q3IDE_SKIP_SURFFLAGS) return qfalse;
    if (!(contents & CONTENTS_SOLID))        return qfalse;
    return qtrue;
}
```

### Quad Submission

Once a valid wall is found, the 4 quad corners are computed from `trace.endpos` and
`trace.plane.normal`. The quad is submitted via `re.AddPolyToScene()`:

```c
// polyVert_t is the Q3 renderer polygon vertex struct
typedef struct {
    vec3_t  xyz;           // World-space 3D position
    float   st[2];         // Texture coordinates (0.0–1.0)
    byte    modulate[4];   // RGBA color modulation (255,255,255,255 = no tint)
} polyVert_t;

polyVert_t verts[4];
// ... compute corners from trace.endpos + trace.plane.normal ...
re.AddPolyToScene( window->shader, 4, verts );
```

---

## Shader System Integration

### Window Shader File

All window shaders live in `baseq3/shaders/q3ide/windows.shader`. One entry per slot:

```
q3ide/window0
{
    nomipmaps
    nopicmip
    {
        map *q3ide_0
        rgbGen identity
        blendfunc GL_ONE GL_ZERO
    }
}
```

- `nomipmaps` — no mipmap chain generated; screen content must never blur
- `nopicmip` — `r_picmip` cvar does not reduce live capture resolution
- `map *q3ide_N` — references the dynamic scratch texture by name
- `rgbGen identity` — no color transformation on the captured pixels
- `blendfunc GL_ONE GL_ZERO` — opaque, replaces what's behind
- `cull disable` added for windows visible from behind (overview mode, debug)

Shader sort order defaults to `opaque` (3) — renders as solid world geometry, correct
depth ordering against BSP surfaces.

### Shader Registration

Shaders are registered at `Q3IDE_InitTextures()` time:

```c
// re.RegisterShader returns a qhandle_t — the only reference spatial/ holds
qhandle_t h = re.RegisterShader("q3ide/window0");
// Stored as Window_t.shader
// Passed to re.AddPolyToScene(h, 4, verts) each frame
```

---

## Engine Adapter Interface

`engine/adapter.h` — the ONLY surface between Q3IDE Brain and any engine.
Implemented in `engine/quake3e/q3ide_adapter.c`.
All `re.*` and `CM_*` calls stay inside this file. `spatial/` sees only this interface.

```c
typedef struct { vec3_t pos; vec3_t normal; float fraction; int area; } trace_result_t;
typedef struct { int id; char name[64]; vec3_t pos; } player_info_t;

typedef struct {

    // — RENDERING (wraps refexport_t re.*) —
    void (*upload_texture)(int slot, int w, int h, const void *data, int format);
    int  (*create_texture)(int w, int h, int format);
    void (*add_quad)(const vec3_t corners[4], const vec2_t uvs[4], int slot, float alpha);
    void (*draw_quad_2d)(float x, float y, float w, float h, int slot, float alpha);
    void (*render_scene)(const void *refdef);
    void (*set_area_light)(int area, float r, float g, float b, float intensity);

    // — COLLISION (wraps CM_BoxTrace) —
    qboolean (*trace)(const vec3_t start, const vec3_t end, trace_result_t *out);
    qboolean (*box_trace)(const vec3_t start, const vec3_t end,
                          const vec3_t mins, const vec3_t maxs, trace_result_t *out);

    // — PLAYER STATE —
    void (*get_player_pos)(vec3_t out);
    void (*get_player_angles)(vec3_t out);
    int  (*get_player_area)(void);
    int  (*get_player_id)(void);

    // — COMMANDS / CVARS (wraps Cmd_AddCommand / Cvar_Get) —
    void        (*register_cmd)(const char *name, void (*fn)(void));
    void        (*register_cvar)(const char *name, const char *def, int flags);
    float       (*cvar_float)(const char *name);
    int         (*cvar_int)(const char *name);
    const char* (*cvar_string)(const char *name);

    // — FILESYSTEM (wraps FS_ReadFile / FS_WriteFile) —
    int  (*read_file)(const char *path, void **buf);
    void (*write_file)(const char *path, const void *buf, int len);
    void (*free_file)(void *buf);

    // — TIME —
    unsigned long long (*get_time_ms)(void);

    // — SOUND —
    void (*play_sound)(const char *path, const vec3_t pos, float vol);

    // — ENTITY SPAWNING —
    int  (*spawn_entity)(const char *model, const vec3_t pos, const vec3_t angles);
    void (*remove_entity)(int ent_id);

    // — NETWORK —
    void (*send_message)(int player_id, const void *data, int len);
    int  (*get_players)(player_info_t *out, int max);

} engine_adapter_t;

void Q3IDE_SetAdapter(const engine_adapter_t *adapter);
extern const engine_adapter_t *g_adapter;
```

---

## Object Model

### Root

```
Q3IDE                        — root singleton, owns everything
├── Scene                    — all objects that live in 3D space
│   ├── Window[]             — macOS app captures on 3D surfaces
│   │   └── Ornament[]       — UI chrome attached to this window
│   ├── Portal[]             — walk-through zone connectors        [STUB]
│   ├── Widget[]             — persistent mini HUD displays        [STUB]
│   ├── Ornament[]           — free viewport-pinned HUD elements   [STUB]
│   ├── UMLNode[]            — 3D architecture diagram nodes       [STUB]
│   ├── RuntimeGeometry[]    — AI-spawned props, walls, desks      [STUB]
│   └── Laser[]              — pointer/debug laser beams
├── Space[]                  — 8 workflow zones                    [STUB]
│   ├── wall_cache[]         — CachedWall_t + WallSlot_t
│   └── views[]              — SpaceWindowView per window
├── Agent[]                  — AI sessions                         [STUB]
└── Audio[]                  — spatialized sources                 [STUB]
```

---

### SpatialObject — base of everything in Scene

```c
typedef enum {
    STYPE_WINDOW = 0, STYPE_PORTAL, STYPE_WIDGET,
    STYPE_ORNAMENT, STYPE_UMLNODE, STYPE_GEOMETRY, STYPE_LASER,
} SpatialType_t;

typedef struct SpatialObject_s {
    int             id;
    SpatialType_t   type;
    int             space_id;
    vec3_t          pos;
    vec3_t          normal;
    float           w, h;
    int             render_layer;
    qboolean        visible;
    qboolean        active;
    char            error[128];
    void (*update)(struct SpatialObject_s *self);
    void (*render)(struct SpatialObject_s *self, const void *refdef);
    void (*destroy)(struct SpatialObject_s *self);
} SpatialObject_t;
```

**Rule:** First field of every subtype is `SpatialObject_t base;`. Always.

---

### Window_t — extends SpatialObject

```c
typedef enum { WMODE_NORMAL=0, WMODE_THEATER, WMODE_BILLBOARD, WMODE_FOCUS } WindowMode_t;
typedef enum { WCAPTURE_COMPOSITE=0, WCAPTURE_DEDICATED, WCAPTURE_ENGINE } WindowCapture_t;
typedef enum { WPLACEMENT_AUTO=0, WPLACEMENT_TRAINED, WPLACEMENT_LOCKED, WPLACEMENT_DRAGGING } WindowPlacement_t;
typedef enum { WLAYOUT_WALL=0, WLAYOUT_FLOATING, WLAYOUT_OVERVIEW, WLAYOUT_FOCUS3 } WindowLayout_t;
typedef enum { WVISIBILITY_PRIVATE=0, WVISIBILITY_TEAM, WVISIBILITY_PUBLIC } WindowVisibility_t;

typedef struct {
    SpatialObject_t     base;           // ALWAYS first

    unsigned int        native_id;
    char                label[128];
    char                app_name[64];

    WindowMode_t        mode;
    WindowCapture_t     capture;
    WindowPlacement_t   placement;
    WindowLayout_t      layout;
    WindowVisibility_t  visibility;

    qboolean    streaming;
    qboolean    idle_apple;
    qboolean    idle_ours;
    qboolean    paused;

    // Texture slot — maps to "*q3ide_N" image and "q3ide/windowN" shader
    int         scratch_slot;           // N → R_CreateImage("*q3ide_N", ...)
    int         tex_w, tex_h;
    float       uv_x0, uv_x1, uv_y0, uv_y1;
    qhandle_t   shader;                 // from re.RegisterShader("q3ide/windowN")
    int         res_tier;               // 0-7 — adaptive resolution (8 tiers)

    int         hover_active;
    float       hover_t;
    unsigned long long hit_time_ms;
    vec3_t      hit_pos;

    unsigned int window_ids[8];
    int          window_count, window_cur;
    qboolean     window_user_sel;

    int          capture_handle;
    qboolean     owns_stream, stream_active, ever_failed;
    unsigned long long last_throttle_ms, last_frame_ms, last_upload_ms, frames;

    int          perf_idx;
    int          ornament_ids[8];
    int          ornament_count;
} Window_t;
```

---

### SpaceWindowView, Space, Stubs

```c
typedef struct {
    int window_id; int space_id;
    vec3_t pos; vec3_t normal; float w, h;
    WindowLayout_t layout;
} SpaceWindowView_t;

typedef struct { SpatialObject_t base; int dest_space_id; vec3_t spawn_pos; } Portal_t;    // [STUB]
typedef struct { SpatialObject_t base; int parent_window_id; } Ornament_t;                 // [STUB]
typedef struct { SpatialObject_t base; } Widget_t;                                         // [STUB]
typedef struct { SpatialObject_t base; } UMLNode_t;                                        // [STUB]
typedef struct { SpatialObject_t base; } RuntimeGeometry_t;                                // [STUB]
typedef struct { int id; int window_id; } Agent_t;                                         // [STUB]
typedef struct { int id; int object_id; } Audio_t;                                         // [STUB]
typedef struct {
    int id; char name[32]; qboolean active;
    int window_ids[Q3IDE_MAX_WINDOWS]; int window_count;
    SpaceWindowView_t views[Q3IDE_MAX_WINDOWS];
    CachedWall_t wall_cache[Q3IDE_MAX_CACHED_WALLS];
    int wall_cache_count; qboolean wall_cache_valid;
} Space_t;
```

---

### Feature Registration

New feature = new `.c` file + 1 row in `spatial/core/features.c`. Nothing else.

```c
typedef struct {
    const char *name;
    int         render_layer;
    void (*init)(void);
    void (*shutdown)(void);
    void (*frame)(void);
    void (*render)(const void *refdef);
    void (*key)(int key, qboolean down);
} q3ide_feature_t;
```

Render dispatch — single path, called once per monitor by the engine adapter:
```c
// spatial/core/render_dispatch.c
void Q3IDE_RenderFrame(const void *refdef) {
    for (int i = 0; i < g_feature_count; i++)
        if (g_features[i].render)
            g_features[i].render(refdef);
}
```

Multi-monitor: viewport loop lives in `engine/quake3e/q3ide_render.c` only.
`spatial/` never knows how many monitors exist.

---

## Stream Freeze During Placement

When the placement queue drains, `Q3IDE_WM_PauseStreams()` / `Q3IDE_WM_ResumeStreams()`
freeze the last captured frame on the GPU.

- `R_UploadSubImage()` is simply not called for paused windows
- Last uploaded texture stays on GPU at zero cost — `qglTexSubImage2D` never fires
- SCK stream stops delivering frames during pause
- FPS restores to 100% instantly on resume
- Replaces the old "2fps throttle during placement" approach entirely

---

## Current → New File Mapping

```
CURRENT (quake3e/code/q3ide/)        NEW LOCATION
──────────────────────────────────────────────────────────────────
q3ide_params.h                    → engine/quake3e/q3ide_params.h
q3ide_engine.c                    → engine/quake3e/q3ide_adapter.c
q3ide_engine_hooks.h              → engine/quake3e/q3ide_hooks.h
q3ide_engine_hooks_grapple.c      → spatial/nav/grapple.c
q3ide_engine_hooks_input.c        → spatial/nav/shoot_to_place.c
q3ide_render.c                    → engine/quake3e/q3ide_render.c (viewport loop)
                                  + spatial/core/render_dispatch.c (feature dispatch)
q3ide_frame.c                     → spatial/core/frame.c
q3ide_win_mngr.c                  → spatial/window/manager.c
q3ide_win_mngr.h                  → spatial/window/manager.h
q3ide_win_mngr_internal.h         → spatial/window/entity.h
q3ide_scene.c                     → spatial/core/scene.c
q3ide_interaction.c/h             → spatial/window/interaction.c/h
q3ide_interaction_frame.c         → spatial/window/interaction.c (merged)
q3ide_geometry.c                  → spatial/window/render.c
q3ide_geometry_clamp.c            → spatial/window/placement.c
q3ide_spawn.c                     → spatial/window/placement.c (merged)
q3ide_view_modes.c/h              → spatial/window/view_modes.c/h
q3ide_aas.c/h                     → spatial/space/aas.c/h
q3ide_aas_face.c                  → spatial/space/aas_face.c
q3ide_aas_query.c                 → spatial/space/aas_query.c
q3ide_aas_walls.c                 → spatial/space/wall_scanner.c
q3ide_aas_format.h                → spatial/space/aas_format.h
q3ide_distance.c                  → spatial/space/player_state.c
q3ide_poll.c                      → spatial/window/capture_poll.c
q3ide_attach_filter.c             → spatial/window/attach_filter.c
q3ide_commands.c                  → spatial/core/commands.c
q3ide_commands_attach.c           → spatial/core/commands.c (merged)
q3ide_commands_desktop.c          → spatial/core/commands.c (merged)
q3ide_commands_query.c            → spatial/core/commands.c (merged)
q3ide_console.c                   → spatial/core/commands.c (merged)
q3ide_dylib.c                     → engine/quake3e/q3ide_dylib.c
q3ide_effects.c                   → spatial/fx/effects.c
q3ide_entity.c                    → spatial/window/interaction.c (merged)
q3ide_laser.c                     → spatial/fx/laser.c
q3ide_log.c/h                     → spatial/core/log.c/h
q3ide_overlay.c                   → spatial/ui/hud.c
q3ide_overlay_kbcache.c           → spatial/ui/kb_cache.c
q3ide_overlay_keyboard.c          → spatial/ui/kb_overlay.c
q3ide_overlay_keys.h              → spatial/ui/kb_overlay.h
q3ide_overlay_winlist.c           → spatial/ui/winlist.c
q3ide_portal.c                    → spatial/space/portal.c [STUB]
q3ide_rope.c                      → spatial/fx/rope.c
q3ide_teleport.c                  → spatial/space/teleport.c
q3ide_design.h                    → spatial/ui/theme.h
```

---

## New Directory Structure

```
q3ide/
│
├── engine/                              ENGINE ADAPTER LAYER
│   ├── adapter.h                        Abstract interface — only surface spatial/ uses
│   └── quake3e/
│       ├── q3ide_adapter.c              Wraps refexport_t re.* and CM_* for spatial/
│       ├── q3ide_adapter.h
│       ├── q3ide_hooks.h                CL_Init / CL_Frame / CL_ShutdownAll declarations
│       ├── q3ide_hooks.c                Wiring: Q3IDE_Init, UpdateCaptures, Shutdown
│       ├── q3ide_render.c               Multi-monitor viewport loop only
│       ├── q3ide_dylib.c                dlopen/dlsym for capture dylib
│       └── q3ide_params.h              THE HOLY BOOK — all constants (SURF_*, MASK_*, etc.)
│
├── spatial/                             Q3IDE BRAIN — engine-agnostic
│   │
│   ├── core/
│   │   ├── scene.h/.c                   SpatialObject_t, scene graph, ID allocator
│   │   ├── features.h/.c                Feature registration + frame/render/key loops
│   │   ├── render_dispatch.h/.c         Single render dispatch (kills 2x render bug)
│   │   ├── frame.h/.c                   Per-frame tick coordinator
│   │   ├── commands.h/.c                Console command handlers (q3ide_list etc.)
│   │   └── log.h/.c                     Levelled logger
│   │
│   ├── window/
│   │   ├── entity.h                     Window_t + all enums
│   │   ├── manager.h/.c                 Win_Create/Destroy/FindById — accessor API
│   │   ├── render.h/.c                  scratch_slot → shader → add_quad pipeline
│   │   ├── interaction.h/.c             Hover, focus, pointer mode, dwell
│   │   ├── view_modes.h/.c              Overview (O) + Focus3 (I)
│   │   ├── placement.h/.c               Area transition + leapfrog coordinator
│   │   ├── capture_poll.h/.c            Ring buffer drain, upload_texture calls
│   │   ├── attach_filter.h/.c           App allow/block lists, pending queue
│   │   ├── visibility.h/.c              Dot product + BSP trace per window
│   │   ├── adaptive_res.h/.c            8 resolution tiers
│   │   └── perf_metrics.h/.c            Per-window stats
│   │
│   ├── space/
│   │   ├── space.h/.c                   Space_t — 8 zones, window refs, wall cache [STUB]
│   │   ├── aas.h/.c                     AAS wrapper — area detection
│   │   ├── aas_face.c                   AAS face/portal geometry helpers
│   │   ├── aas_query.h/.c               GetAreaWalls, wall geometry
│   │   ├── aas_format.h                 AAS binary format (read-only)
│   │   ├── wall_scanner.h/.c            trace() → surface filter → quad corners
│   │   ├── wall_cache.h/.c              CachedWall_t, WallSlot_t, trained positions
│   │   ├── placement_queue.h/.c         FPS-gated drain, stream freeze
│   │   ├── player_state.h/.c            Player pos, distance helpers, area tracking
│   │   ├── portal.h/.c                  Portal_t [STUB]
│   │   └── teleport.h/.c               Teleport logic
│   │
│   ├── ui/
│   │   ├── theme.h                      Design tokens — VisionOS colors, spacing, typography
│   │   ├── primitives.h/.c              Panel/Label/Button/List — AI uses ONLY these
│   │   ├── hud.h/.c                     Billboard text, HUD messages
│   │   ├── winlist.h/.c                 Window list panel
│   │   ├── kb_overlay.h/.c              Left-monitor keybinding overlay
│   │   ├── kb_cache.h/.c               Glyph cache
│   │   ├── ornament.h/.c               Ornament_t [STUB]
│   │   ├── widget.h/.c                 Widget_t [STUB]
│   │   ├── vibrancy.h/.c               Dynamic text contrast [STUB]
│   │   └── context_menu.h/.c           Right-click menus [STUB]
│   │
│   ├── fx/
│   │   ├── laser.h/.c                   Laser pointer beam
│   │   ├── rope.h/.c                    Grapple rope geometry
│   │   └── effects.h/.c                Blood splat, hit effects
│   │
│   ├── nav/
│   │   ├── grapple.h/.c                 Grapple hook — movement + window nav
│   │   ├── shoot_to_place.h/.c          Shoot-to-move
│   │   └── bookmark.h/.c               Saved positions [STUB]
│   │
│   ├── input/                           [STUB]
│   │   ├── hotkeys.h/.c
│   │   ├── kb_display.h/.c
│   │   └── magic_mouse.h/.c
│   │
│   ├── mode/                            [STUB]
│   │   ├── theater.h/.c
│   │   └── office.h/.c
│   │
│   ├── agent/                           [STUB]
│   │   ├── agent.h/.c
│   │   ├── diff.h/.c
│   │   └── dashboard.h/.c
│   │
│   ├── audio/                           [STUB]
│   │   ├── spatial.h/.c
│   │   ├── ducking.h/.c
│   │   └── notifications.h/.c
│   │
│   ├── uml/                             [STUB]
│   │   ├── navigator.h/.c
│   │   ├── node.h/.c
│   │   ├── pipe.h/.c
│   │   └── mini.h/.c
│   │
│   ├── project/                         [STUB]
│   │   ├── scanner.h/.c
│   │   ├── browser.h/.c
│   │   └── quickopen.h/.c
│   │
│   ├── multiplayer/                     [STUB]
│   │   └── sync.h/.c
│   │
│   ├── quakeos/                         [STUB]
│   │   ├── font.h/.c
│   │   ├── syntax.h/.c
│   │   ├── editor.h/.c
│   │   ├── markdown.h/.c
│   │   ├── image_viewer.h/.c
│   │   └── focus_mode.h/.c
│   │
│   ├── bot/                             [STUB]
│   │   ├── bot.h/.c
│   │   └── api_bridge.h/.c
│   │
│   └── ai_geometry/                     [STUB]
│       ├── generator.h/.c
│       ├── props.h/.c
│       └── structural.h/.c
│
├── capture/                             CAPTURE LAYER (Rust, unchanged)
│   └── src/
│       ├── lib.rs
│       ├── backend.rs
│       ├── screencapturekit.rs
│       ├── ringbuf.rs
│       └── window.rs
│
├── daemon/                              UML pre-processor (separate Rust process)
│   └── src/
│       ├── main.rs
│       ├── watcher.rs
│       ├── cache.rs
│       └── parsers/
│
├── q3ide_capture.h                      C-ABI header (cbindgen)
└── q3ide_main.c                         Entry point
```

---

## UI Design System

AI must only use these primitives. No inline colors, sizes, or raw quad geometry. Ever.

```c
// spatial/ui/primitives.h
void UI_Panel(vec3_t pos, float w, float h, UIStyle_t s);
void UI_Label(vec3_t pos, const char *text, UITextStyle_t s);
void UI_Button(vec3_t pos, const char *label, UIButtonStyle_t s, void (*cb)(void));
void UI_List(vec3_t pos, const char **items, int n, UIStyle_t s);
```

All styles come from `spatial/ui/theme.h`. No hardcoded values anywhere.

---

## Rendering Standards

- `nomipmaps` + `nopicmip` on all Q3IDE shaders — screen content must never blur
- Shaders in `baseq3/shaders/q3ide/` — no inline shader strings in C
- All geometry through adapter `add_quad()` — no direct GL calls from `spatial/`
- Normal vectors always computed explicitly, never assumed
- `cull disable` in shaders for double-sided quads
- Pixel format: `GL_BGRA` + `GL_UNSIGNED_INT_8_8_8_8_REV` — matches SCK output, zero swizzle

---

## Debug Visualization

`/q3ide_debug` — overlays on all active scene objects:
- Wireframe bounding box + normal arrow
- Type label + object ID + active flags
- Red outline if `error[0] != '\0'`

---

## Terminology

| Term | Definition |
|---|---|
| SpatialObject_t | Base struct for every 3D scene object. First field of every subtype. |
| Window_t | A captured macOS app window projected onto a 3D wall surface as a quad. |
| scratch_slot | Index N. Maps to image `*q3ide_N` (in hash table) and shader `q3ide/windowN`. |
| R_CreateImage | `tr_image.c` function that allocates an `image_t` and uploads initial pixel data. |
| R_UploadSubImage | `tr_image.c` function that calls `qglTexSubImage2D` to update an existing image per-frame. |
| Upload32 | Internal `tr_image.c` function. `subImage=qtrue` path calls `qglTexSubImage2D`. |
| polyVert_t | Q3 renderer vertex: `{vec3_t xyz, float st[2], byte modulate[4]}`. Used by AddPolyToScene. |
| refexport_t | Renderer function table. `re.AddPolyToScene`, `re.RegisterShader`, etc. Filled by `GetRefAPI`. |
| refimport_t | Engine services passed to renderer at init. `ri.Cmd_AddCommand`, `ri.FS_ReadFile`, etc. |
| RE_UploadCinematic | Q3's existing per-frame texture update for ROQ video. Q3IDE uses the same mechanism. |
| CG_DRAW_ACTIVE_FRAME | cgame VM message sent each frame. Q3IDE injects quads before RenderScene processes them. |
| Engine Adapter | `engine/adapter.h`. The only boundary between `spatial/` and any engine. |
| Feature Registration | New feature = new .c file + 1 row in features.c. Nothing else touched. |
| RenderDispatch | Single render entry in `spatial/core/`. Called once per monitor by adapter. |
| SpaceWindowView | Per-space override of Window position/size/layout. Same Window, different Space. |
| Stream Freeze | `PauseStreams()`/`ResumeStreams()` — hold last GPU frame at zero cost during placement. |
| MASK_SOLID | Q3 brushmask `= CONTENTS_SOLID`. Used by wall scanner to trace against world geometry. |
| surfaceFlags | Q3 BSP per-surface flags. Wall scanner rejects `SURF_SKY`, `SURF_NODRAW`, `SURF_HINT`, `SURF_SKIP`. |
| IMGFLAG_NOSCALE | Prevents `Upload32` from forcing power-of-2 dimensions. Required for exact capture sizes. |
| IMGFLAG_NOLIGHTSCALE | Prevents gamma/overbright shift on capture textures. Screen pixels must not be color-shifted. |
| Three-Layer Architecture | Capture → Brain → Engine Adapter. Swap `engine/` without touching `spatial/`. |
| lint.sh | Hard build failure on: 200+ line files in `spatial/`, magic numbers, direct engine calls. |

---

## Lint Rules (lint.sh — hard build failures)

- Any `.c` file in `spatial/` exceeding 400 lines
- Any magic number in `spatial/` not from `q3ide_params.h`
- Any direct call to `trap_*`, `re.*`, `CG_*`, `CM_*`, `qgl*` inside `spatial/`
- `-Wswitch` — unhandled enum case anywhere