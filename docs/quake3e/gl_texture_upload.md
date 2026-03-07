# Quake3e OpenGL Texture Upload - Key Functions and Structures

> **Source:** https://raw.githubusercontent.com/ec-/Quake3e/main/code/renderer/tr_image.c
> **Fetched:** 2026-03-07
> **Purpose:** Detailed analysis of OpenGL texture creation, upload, and dynamic update mechanisms in the Quake3e renderer. Critical for Q3IDE's runtime texture injection.

---

## Overview

The OpenGL renderer's `tr_image.c` handles all texture management: loading images from disk,
uploading pixel data to GPU via OpenGL, managing mipmaps, and supporting dynamic texture updates.
This is the primary integration point for Q3IDE's screen capture texture streaming.

## Key Data Structures

### `image_t` (defined in `tr_local.h`)

The central texture handle structure. Key fields:
- `char imgName[MAX_QPATH]` - Texture name/path
- `int width, height` - Texture dimensions
- `GLuint texnum` - OpenGL texture object name (GPU handle)
- `int internalFormat` - GL internal format (GL_RGBA8, GL_RGB, etc.)
- `imgFlags_t flags` - Flags controlling wrap, mipmap, picmip behavior
- `int TMU` - Texture Mapping Unit assignment
- `int wrapClampMode` - GL_REPEAT, GL_CLAMP_TO_EDGE, etc.
- `int frameUsed` - Last frame this texture was referenced (for cleanup)

### `imgFlags_t` (flags enum)

```c
typedef enum {
    IMGFLAG_NONE           = 0x0000,
    IMGFLAG_MIPMAP         = 0x0001,  // Generate mipmaps
    IMGFLAG_PICMIP         = 0x0002,  // Apply r_picmip reduction
    IMGFLAG_CLAMPTOEDGE    = 0x0004,  // GL_CLAMP_TO_EDGE wrap mode
    IMGFLAG_CLAMPTOBORDER  = 0x0008,  // GL_CLAMP_TO_BORDER wrap mode
    IMGFLAG_NOSCALE        = 0x0010,  // Don't force power-of-2
    IMGFLAG_NOLIGHTSCALE   = 0x0020,  // Skip light scaling
    IMGFLAG_LIGHTMAP       = 0x0040,  // This is a lightmap texture
    // ... more flags
} imgFlags_t;
```

## Core Functions

### `R_CreateImage` - Primary texture creation

```c
image_t *R_CreateImage( const char *name, const char *name2, byte *pic,
                        int width, int height, imgFlags_t flags )
```

**What it does:**
1. Allocates a new `image_t` slot from the global image table
2. Generates a GL texture name via `qglGenTextures()`
3. Stores metadata (name, dimensions, flags)
4. Calls `Upload32()` to process and upload pixel data to GPU
5. Configures wrap mode (repeat/clamp/border)
6. Configures filtering (linear, mipmap, anisotropic)
7. Adds image to hash table for lookup
8. Returns the `image_t` handle

**Parameters:**
- `name` - Texture path/identifier (e.g., "textures/base_wall/concrete1")
- `name2` - Alternate name (for lookup purposes)
- `pic` - Raw RGBA pixel data (4 bytes per pixel)
- `width, height` - Source image dimensions
- `flags` - Combination of `imgFlags_t` values

**Filtering configuration:**
- Mipmapped textures: configurable min/mag filters + anisotropy
- Non-mipmapped: `GL_LINEAR` for both filters
- Special handling for 3DFX Voodoo hardware

### `Upload32` - Main texture upload routine

```c
static void Upload32( byte *data, int x, int y, int width, int height,
                      image_t *image, qboolean subImage )
```

**What it does:**
1. Resamples to power-of-2 dimensions if needed (unless `IMGFLAG_NOSCALE`)
2. Applies picmip reduction (divides dimensions by 2^picmip)
3. Clamps to `glConfig.maxTextureSize`
4. Applies light scaling via `R_LightScaleTexture()`
5. Determines internal format via `RawImage_GetInternalFormat()`
6. Uploads base level via `LoadTexture()`
7. If mipmapped: generates mipmap chain, uploads each level
8. Optionally applies color blending per mip level

**The `subImage` parameter:**
- `qfalse` = Creating new texture (uses `glTexImage2D`)
- `qtrue` = Updating existing texture (uses `glTexSubImage2D`)

### `R_UploadSubImage` - Dynamic texture updates (CRITICAL for Q3IDE)

```c
void R_UploadSubImage( byte *data, int x, int y, int width, int height,
                       image_t *image )
```

**What it does:**
- Calls `Upload32()` with `subImage = qtrue`
- This triggers `glTexSubImage2D()` instead of `glTexImage2D()`
- Allows updating texture contents without recreating the GPU resource
- Preserves existing texture state, filtering, and wrap mode

**This is the primary mechanism Q3IDE should use for streaming screen captures to GPU textures.**

### `LoadTexture` - Low-level GPU upload

```c
static void LoadTexture( int miplevel, int x, int y, int width, int height,
                         const byte *data, qboolean subImage, image_t *image )
```

**What it does:**
- If `subImage == qfalse`: calls `qglTexImage2D()` to allocate and upload
- If `subImage == qtrue`: calls `qglTexSubImage2D()` to update in-place
- Handles format conversion based on `image->internalFormat`

### `R_FindImageFile` - Image lookup and loading

```c
image_t *R_FindImageFile( const char *name, imgFlags_t flags )
```

**What it does:**
1. Hash-table lookup for previously loaded images
2. If found, returns existing `image_t`
3. If not found, calls `R_LoadImage()` to load from disk
4. `R_LoadImage()` tries multiple formats: PNG, TGA, JPG, PCX, BMP
5. Calls `R_CreateImage()` with loaded pixel data
6. Returns `NULL` if image cannot be found/loaded

### `RawImage_GetInternalFormat` - Format selection

Determines the OpenGL internal format based on:
- Whether the image has alpha channel
- `r_texturebits` cvar (16-bit or 32-bit)
- Whether texture compression is available (S3TC/DXT1)
- Lightmap images always use `GL_RGB`

### `GL_TextureMode` - Filter mode changes

Updates filtering for all loaded textures when `gl_textureMode` changes.
Iterates through all images and updates min/mag filter + anisotropy.

## Texture Creation Pipeline (Full Flow)

```
1. R_FindImageFile("textures/some_texture", flags)
   |
   +-- Hash lookup (return if cached)
   |
   +-- R_LoadImage() -- tries .png, .tga, .jpg, .pcx, .bmp
   |
   +-- R_CreateImage(name, name2, pixels, w, h, flags)
       |
       +-- Allocate image_t slot
       +-- qglGenTextures() -- get GL texture name
       +-- Upload32(pixels, 0, 0, w, h, image, qfalse)
       |   |
       |   +-- ResampleTexture() -- power-of-2 conversion
       |   +-- R_LightScaleTexture() -- gamma/overbright
       |   +-- RawImage_GetInternalFormat() -- format selection
       |   +-- LoadTexture(0, ..., qfalse) -- base mip
       |   |   +-- qglTexImage2D() -- GPU upload
       |   +-- R_MipMap2() / R_MipMap() -- generate mips
       |   +-- LoadTexture(n, ..., qfalse) -- each mip level
       |
       +-- Set wrap mode (GL_REPEAT, GL_CLAMP_TO_EDGE, etc.)
       +-- Set filter mode (GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, etc.)
       +-- Set anisotropy
       +-- Add to hash table
       +-- Return image_t*
```

## Dynamic Texture Update Pipeline (For Q3IDE)

```
1. Initial: R_CreateImage("q3ide_capture_0", NULL, blank_pixels, 1920, 1080,
                           IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOSCALE | IMGFLAG_NOLIGHTSCALE)
   -- Creates GPU texture with initial blank content

2. Per-frame: R_UploadSubImage(capture_pixels, 0, 0, 1920, 1080, capture_image)
   |
   +-- Upload32(pixels, 0, 0, 1920, 1080, image, qtrue)
       |
       +-- LoadTexture(0, 0, 0, 1920, 1080, pixels, qtrue, image)
           |
           +-- qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1920, 1080,
                                GL_RGBA, GL_UNSIGNED_BYTE, pixels)
```

## Q3IDE Integration Strategy

1. **Create capture textures** at initialization using `R_CreateImage()` with:
   - `IMGFLAG_CLAMPTOEDGE` - No tiling
   - `IMGFLAG_NOSCALE` - Preserve exact dimensions
   - `IMGFLAG_NOLIGHTSCALE` - Don't modify colors
   - No `IMGFLAG_MIPMAP` - Skip mipmap generation for speed
   - No `IMGFLAG_PICMIP` - Don't reduce resolution

2. **Stream frames** each render frame using `R_UploadSubImage()` with RGBA pixel data from the Rust capture dylib's ring buffer.

3. **Create a shader** that references the capture texture, then use `AddPolyToScene()` with that shader to draw quads on wall surfaces.

4. **Performance note:** `glTexSubImage2D` is relatively fast for streaming updates, especially without mipmap regeneration. For 1080p RGBA at 60fps, expect ~8MB/frame upload bandwidth.
