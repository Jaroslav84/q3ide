# Quake3e Common Renderer Code - Directory Structure

> **Source:** https://github.com/ec-/Quake3e/tree/main/code/renderercommon
> **Fetched:** 2026-03-07
> **Purpose:** File listing for the shared renderer code (`code/renderercommon/`), used by all renderer backends (OpenGL, OpenGL2, Vulkan).

---

## Directory: `code/renderercommon/`

### Subdirectories
| Directory | Purpose |
|---|---|
| `vulkan/` | Vulkan SDK headers (vulkan.h, etc.) bundled for compilation |

### Source Files

| File | Purpose |
|---|---|
| `tr_font.c` | TrueType font rendering (FreeType-based glyph rasterization) |
| `tr_image_bmp.c` | BMP image format loader |
| `tr_image_jpg.c` | JPEG image format loader |
| `tr_image_pcx.c` | PCX image format loader |
| `tr_image_png.c` | PNG image format loader |
| `tr_image_tga.c` | TGA image format loader |
| `tr_noise.c` | Perlin noise generation (used for shader effects) |

### Header Files

| File | Purpose |
|---|---|
| `tr_public.h` | **KEY FILE** - Public renderer API (`refexport_t` and `refimport_t` interfaces) |
| `tr_types.h` | **KEY FILE** - Renderer type definitions (`refEntity_t`, `refdef_t`, `glconfig_t`, `polyVert_t`) |

## Key Interfaces in `tr_public.h`

### `refexport_t` - What the renderer provides to the engine

This is the function pointer table that the engine uses to call into whichever renderer is loaded.
Key functions relevant to Q3IDE:

```c
typedef struct {
    void    (*Shutdown)( int destroyWindow );
    void    (*BeginRegistration)( glconfig_t *config );
    qhandle_t (*RegisterModel)( const char *name );
    qhandle_t (*RegisterSkin)( const char *name );
    qhandle_t (*RegisterShader)( const char *name );
    qhandle_t (*RegisterShaderNoMip)( const char *name );
    void    (*LoadWorld)( const char *name );
    void    (*SetWorldVisData)( const byte *vis );
    void    (*EndRegistration)( void );
    void    (*ClearScene)( void );
    void    (*AddRefEntityToScene)( const refEntity_t *re );
    void    (*AddPolyToScene)( qhandle_t hShader, int numVerts, const polyVert_t *verts );
    void    (*AddLightToScene)( const vec3_t org, float radius, float r, float g, float b );
    void    (*RenderScene)( const refdef_t *fd );
    void    (*SetColor)( const float *rgba );
    void    (*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
    void    (*BeginFrame)( stereoFrame_t stereoFrame );
    void    (*EndFrame)( int *




, int *
rontEndMsec );
    // ... more functions
} refexport_t;
```

### `refimport_t` - What the engine provides to the renderer

Function pointer table for the renderer to call back into the engine:
- `Cmd_AddCommand` / `Cmd_RemoveCommand` - Console command registration
- `Cmd_Argc` / `Cmd_Argv` - Command argument parsing
- `Cvar_Get` / `Cvar_Set` - Cvar access
- `FS_ReadFile` / `FS_FreeFile` - File system access
- `Printf` / `Error` - Logging
- `Hunk_Alloc` - Memory allocation
- Various system services

## Also Relevant: `code/renderer2/`

The OpenGL2 renderer (ioquake3-derived, not actively maintained) lives in `code/renderer2/` and contains approximately 30+ source files including:

### Subdirectories
- `glsl/` - GLSL shader source files

### Key Files
- `tr_init.c` - Renderer initialization
- `tr_image.c` / `tr_image_dds.c` - Texture management (with DDS support)
- `tr_fbo.c` - Framebuffer object management
- `tr_glsl.c` - GLSL shader compilation
- `tr_postprocess.c` - Post-processing effects
- `tr_dsa.c` - Direct State Access extensions
- All standard `tr_*.c` files mirroring the GL1 renderer structure

## Q3IDE Integration Notes

- **`tr_public.h`** is the most important file for Q3IDE's engine adapter layer. It defines the exact API boundary between engine and renderer.
- **`tr_types.h`** defines `polyVert_t` which is needed for `AddPolyToScene()` - the primary mechanism for drawing custom geometry (quads on walls).
- The `refexport_t` / `refimport_t` pattern is exactly the "swappable trait" that Q3IDE's architecture is designed around.
- Image format loaders in this directory handle disk-to-memory loading; the actual GPU upload happens in each renderer's own `tr_image.c`.
