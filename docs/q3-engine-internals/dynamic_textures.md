# Quake 3 Dynamic Texture Upload (Per-Frame)

> **Sources:**
> - [Quake III Arena Source Code - tr_image.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/renderer/tr_image.c)
> - [Quake III Arena Source Code - tr_backend.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/renderer/tr_backend.c)
> - [Fabien Sanglard - Quake 3 Renderer Review](https://fabiensanglard.net/quake3/renderer.php)
> - [Quake III Arena Source Code - tr_shader.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/renderer/tr_shader.c)
> - [Quake III Arena Source Code - tr_scene.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/renderer/tr_scene.c)

## Overview

Quake 3's renderer was designed primarily for static textures loaded at map start, but it includes a dynamic texture system used for cinematic playback (ROQ video). This system is directly relevant to Q3IDE, which needs to upload captured macOS window frames to GPU textures every frame.

## Existing Dynamic Texture Mechanisms

### 1. RE_StretchRaw -- Fullscreen Cinematic Rendering

```c
void RE_StretchRaw(int x, int y, int w, int h,
                   int cols, int rows,
                   const byte *data, int client, qboolean dirty);
```

This function is called by `CIN_DrawCinematic` after it decodes a ROQ video frame into pixel data:

1. **Input**: Raw 8-bit pixel data from the ROQ cinematic decoder
2. **Processing**: Converts the byte array into a 16-bit (RGB565) image
3. **Command**: Adds a "stretch raw" command to the backend render command list
4. **Backend**: When the backend processes this command, it uploads the image data to a dedicated scratch texture and renders it as a fullscreen quad

The function uses a dedicated **scratch texture** slot (one per cinematic client), meaning it does not interfere with the normal texture hash table.

### 2. RE_UploadCinematic -- In-World Cinematic Textures

```c
void RE_UploadCinematic(int w, int h, int cols, int rows,
                        const byte *data, int client, qboolean dirty);
```

Used for cinematic textures applied to world surfaces (video screens in maps):

1. Takes decoded cinematic frame data
2. Uploads directly to a GL texture via `glTexSubImage2D` or `glTexImage2D`
3. The texture is referenced by a shader stage, so it appears on the world surface

This is the closest existing analogue to what Q3IDE needs.

### 3. Shader Animated Textures (animMap)

```
textures/mymod/screen
{
    {
        animMap 10 tex_frame1.tga tex_frame2.tga tex_frame3.tga
    }
}
```

Pre-loaded texture sequences cycled at a fixed rate. Not suitable for Q3IDE since frames are not known in advance.

## OpenGL Texture Upload Functions

### glTexImage2D (Full Upload)

```c
qglTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
              scaledWidth, scaledHeight, 0,
              GL_RGBA, GL_UNSIGNED_BYTE, data);
```

- Creates or replaces an entire texture
- Must specify dimensions, format, and pixel data
- Slower than `glTexSubImage2D` for same-size updates

### glTexSubImage2D (Partial Update)

```c
qglTexSubImage2D(GL_TEXTURE_2D, 0,
                 0, 0,                    // x, y offset
                 width, height,           // dimensions
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 data);
```

- Updates a sub-region of an existing texture
- Faster than full upload when texture dimensions don't change
- **This is the preferred method for per-frame updates** in Q3IDE

## Q3IDE Dynamic Texture Strategy

Based on analysis of the Quake 3 texture system, here is the recommended approach for Q3IDE:

### Step 1: Create Capture Texture at Init

During Q3IDE initialization (after `CG_INIT` or engine init):

```c
// Create a dedicated texture for the captured window
image_t *q3ide_texture = R_CreateImage(
    "*q3ide_capture",           // Name (asterisk = internal/special)
    initial_data,               // Initial pixel data (can be blank)
    capture_width,              // Width (must be power of 2, or engine will resample)
    capture_height,             // Height (must be power of 2)
    qfalse,                     // No mipmaps (not needed for screen content)
    qfalse,                     // No picmip (don't reduce resolution)
    GL_CLAMP                    // Clamp to edge (no repeat)
);
```

### Step 2: Create Custom Shader

```c
// Register a shader that references the capture texture
shader_t *q3ide_shader = R_FindShader("q3ide/window", LIGHTMAP_NONE, qtrue);
```

Or define in a `.shader` file:

```
q3ide/window
{
    nomipmaps
    nopicmip
    {
        map *q3ide_capture
        rgbGen identity
        // No lightmap -- self-illuminated like a screen
    }
}
```

### Step 3: Per-Frame Texture Upload

Each frame, in the rendering path (before `RE_RenderScene`):

```c
void Q3IDE_UpdateTexture(const byte *frame_data, int width, int height) {
    // Bind the capture texture
    GL_Bind(q3ide_texture);

    // Upload new frame data (same dimensions, use SubImage for speed)
    qglTexSubImage2D(GL_TEXTURE_2D, 0,
                     0, 0,
                     width, height,
                     GL_RGBA, GL_UNSIGNED_BYTE,
                     frame_data);
}
```

### Step 4: Render as Wall Quad

Use `trap_R_AddPolyToScene` or direct polygon submission to render the textured quad on a wall:

```c
polyVert_t verts[4];
// Set positions from wall trace results
// Set texture coordinates: (0,0), (1,0), (1,1), (0,1)
// Set colors to white (identity)

trap_R_AddPolyToScene(q3ide_shaderHandle, 4, verts);
```

## Performance Considerations

### Texture Size

- Power-of-2 dimensions required (or engine resamples, wasting CPU)
- Recommended: 1024x1024 or 2048x2048 for good quality
- The Rust capture dylib should output frames already at the target resolution

### Upload Frequency

- Uploading a 1024x1024 RGBA texture = 4 MB per frame
- At 60 FPS = 240 MB/s of texture upload bandwidth
- Modern GPUs handle this easily via PCI-E
- Multiple windows = multiple uploads per frame (linearly scales)

### Format Considerations

| Format | Bytes/Pixel | 1024x1024 Size | Notes |
|--------|-------------|----------------|-------|
| GL_RGBA | 4 | 4 MB | Best compatibility, ScreenCaptureKit native |
| GL_BGRA | 4 | 4 MB | macOS native format, faster on some GPUs |
| GL_RGB | 3 | 3 MB | No alpha, but alignment issues |

### Optimization: PBO (Pixel Buffer Objects)

For higher performance, modern OpenGL allows asynchronous texture uploads via PBOs:

```c
// Create PBO
glGenBuffers(1, &pbo);
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STREAM_DRAW);

// Map PBO, write frame data
void *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
memcpy(ptr, frame_data, size);
glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

// Upload from PBO (asynchronous, returns immediately)
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
```

This is available in Quake3e's OpenGL context and would allow the GPU to DMA the texture data while the CPU continues with other work.

### Optimization: Double-Buffered Textures

Create two GL textures and alternate between them:
- Frame N: Upload to texture A, render with texture B
- Frame N+1: Upload to texture B, render with texture A

This avoids pipeline stalls where the GPU is trying to read a texture that the CPU is uploading to.

## Quake3e-Specific Considerations

Quake3e's renderer includes enhancements that affect dynamic texture handling:

- **VBO caching**: Static world surfaces are cached in VBOs; dynamic Q3IDE quads should not be cached
- **Vulkan renderer**: If using Quake3e's Vulkan backend, texture upload uses `vkCmdCopyBufferToImage` instead of OpenGL calls
- **Merged lightmaps**: Q3IDE textures are not lightmaps and won't be affected by atlas merging
- **Multi-threaded backend**: Texture uploads must be synchronized with the render thread

## Cinematic Texture Shader Example (Existing)

For reference, Quake 3 maps can display video on surfaces using:

```
textures/sfx/videoscreen
{
    qer_editorimage textures/sfx/videoscreen.tga
    {
        videoMap video/mycinematic.roq
        // Engine automatically handles per-frame upload
    }
}
```

The `videoMap` keyword triggers the engine to play a ROQ video and upload frames using `RE_UploadCinematic`. Q3IDE's system is analogous but sources frames from ScreenCaptureKit instead of a ROQ file.
