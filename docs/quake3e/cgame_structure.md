# Quake3e CGame Code - Directory Structure

> **Source:** https://github.com/ec-/Quake3e/tree/main/code/cgame
> **Fetched:** 2026-03-07
> **Purpose:** File listing for the client game module public interface (`code/cgame/`).

---

## Directory: `code/cgame/`

### Files

| File | Purpose |
|---|---|
| `cg_public.h` | **Public CGame API** - Defines the interface between the engine and the client game module |

## About the CGame Module

The `code/cgame/` directory in Quake3e contains only the **public header** that defines
the interface between the engine (client) and the cgame QVM/DLL. The actual cgame
implementation (all the `cg_*.c` files like `cg_main.c`, `cg_draw.c`, `cg_weapons.c`, etc.)
is **not part of the engine** -- it comes from the game mod (baseq3, CPMA, OSP, etc.).

This is by design: the engine provides a stable interface, and game mods implement the cgame
logic as a QVM (Quake Virtual Machine bytecode) or native DLL.

## Key Definitions in `cg_public.h`

### CGame System Calls (engine -> cgame)

```c
typedef enum {
    CG_INIT,                // Initialize cgame
    CG_SHUTDOWN,            // Shutdown cgame
    CG_CONSOLE_COMMAND,     // Handle console command
    CG_DRAW_ACTIVE_FRAME,   // Draw the game world (called each frame)
    CG_CROSSHAIR_PLAYER,    // Get player under crosshair
    CG_LAST_ATTACKER,       // Get last attacker
    CG_KEY_EVENT,           // Handle key event
    CG_MOUSE_EVENT,         // Handle mouse event
    CG_EVENT_HANDLING,      // UI event handling mode
    CG_MAX_CGAME_SYSCALL
} cgameExport_t;
```

### CGame Trap Calls (cgame -> engine)

The cgame module calls back into the engine via trap functions:

```c
typedef enum {
    CG_PRINT,               // Print to console
    CG_ERROR,               // Fatal error
    CG_MILLISECONDS,        // Get current time
    CG_CVAR_REGISTER,       // Register a cvar
    CG_CVAR_UPDATE,         // Update cvar value
    CG_CVAR_SET,            // Set cvar value
    CG_ARGC,                // Command argument count
    CG_ARGV,                // Get command argument
    CG_FS_FOPENFILE,        // Open file
    CG_FS_READ,             // Read file
    CG_FS_WRITE,            // Write file
    CG_FS_FCLOSEFILE,       // Close file
    CG_SENDCONSOLECOMMAND,  // Execute console command

    // Renderer traps (critical for Q3IDE):
    CG_R_REGISTERMODEL,     // Load 3D model
    CG_R_REGISTERSKIN,      // Load skin
    CG_R_REGISTERSHADER,    // Load shader/texture
    CG_R_CLEARSCENE,        // Clear scene for new frame
    CG_R_ADDREFENTITYTOSCENE, // Add entity to render
    CG_R_ADDPOLYTOSCENE,    // Add polygon to render (**KEY for Q3IDE**)
    CG_R_ADDLIGHTTOSCENE,   // Add dynamic light
    CG_R_RENDERSCENE,       // Render the composed scene
    CG_R_SETCOLOR,          // Set 2D drawing color
    CG_R_DRAWSTRETCHPIC,    // Draw 2D image

    // Additional traps...
    CG_CM_LOADMODEL,        // Load collision model
    CG_CM_POINTCONTENTS,    // Point collision test
    CG_CM_BOXTRACE,         // Box trace for collision

    // ... many more
} cgameImport_t;
```

## Q3IDE Integration with CGame

### Option A: Engine-Side Integration (Recommended for MVP)

Q3IDE operates entirely within the engine, bypassing cgame:
- Create textures via renderer API directly (`re.RegisterShader` or `R_CreateImage`)
- Draw quads via `re.AddPolyToScene()` from engine code
- No modification to cgame QVM needed
- Works with any game mod (baseq3, CPMA, etc.)

### Option B: CGame-Side Integration (Future)

For deeper integration, a modified cgame could:
- Use `CG_R_ADDPOLYTOSCENE` trap to draw capture quads
- Use `CG_R_REGISTERSHADER` to reference capture textures
- Handle placement logic in cgame code
- Requires custom cgame QVM/DLL (mod-specific)

### Why Engine-Side is Better for MVP

1. **Mod-independent:** Works with stock baseq3, CPMA, and all other mods
2. **Simpler:** No QVM compilation or mod distribution needed
3. **Direct access:** Engine code can call renderer functions directly, not through trap interface
4. **Performance:** No VM call overhead for per-frame texture updates

### The Key Trap for Q3IDE: `CG_R_ADDPOLYTOSCENE`

This trap (accessible as `re.AddPolyToScene()` from engine code) is how custom geometry
gets drawn in the game world:

```c
// From engine code:
re.AddPolyToScene( captureShader, 4, quadVerts );

// Where quadVerts is an array of 4 polyVert_t:
typedef struct {
    vec3_t xyz;       // 3D position
    float st[2];      // Texture coordinates
    byte modulate[4]; // Color modulation (RGBA)
} polyVert_t;
```

This is how Q3IDE will draw captured window content on wall surfaces in the game world.
