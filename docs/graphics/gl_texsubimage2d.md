# glTexSubImage2D -- Dynamic Texture Update Per Frame

> **Sources:**
> - [glTexSubImage2D - OpenGL 4 Reference Pages](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexSubImage2D.xhtml)
> - [Updating textures per frame - Khronos Forums](https://community.khronos.org/t/updating-textures-per-frame/75020)
> - [glTexSubImage2D - GLTEXSUBIMAGE2D(3G) manual page](https://www.x.org/releases/X11R6.8.1/doc/glTexSubImage2D.3.html)
> - [glTexSubImage2D function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/opengl/gltexsubimage2d)

## Overview

`glTexSubImage2D` is used to specify a contiguous sub-image of an existing two-dimensional
texture image. Unlike `glTexImage2D`, it does **not** allocate a new texture -- it only updates
portions of an already-created texture. This makes it the primary OpenGL function for dynamic,
per-frame texture updates.

## Function Signature

```c
void glTexSubImage2D(
    GLenum  target,
    GLint   level,
    GLint   xoffset,
    GLint   yoffset,
    GLsizei width,
    GLsizei height,
    GLenum  format,
    GLenum  type,
    const void *pixels
);
```

### Parameters

| Parameter | Description |
|-----------|-------------|
| `target` | Must be `GL_TEXTURE_2D`, `GL_TEXTURE_CUBE_MAP_POSITIVE_X`, etc. |
| `level` | Mipmap level-of-detail number. 0 is the base image level. |
| `xoffset` | Texel offset in the x direction within the texture array. |
| `yoffset` | Texel offset in the y direction within the texture array. |
| `width` | Width of the texture subimage. |
| `height` | Height of the texture subimage. |
| `format` | Format of the pixel data: `GL_RED`, `GL_RG`, `GL_RGB`, `GL_BGR`, `GL_RGBA`, `GL_BGRA`, etc. |
| `type` | Data type of the pixel data: `GL_UNSIGNED_BYTE`, `GL_UNSIGNED_INT_8_8_8_8_REV`, etc. |
| `pixels` | Pointer to the image data in memory, or offset into a bound `GL_PIXEL_UNPACK_BUFFER`. |

## Per-Frame Update Pattern

A common approach for per-frame texture updates:

```c
// Initial texture creation (once)
glGenTextures(1, &textureId);
glBindTexture(GL_TEXTURE_2D, textureId);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
             GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

// Per-frame update
glBindTexture(GL_TEXTURE_2D, textureId);
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixelData);
```

## Performance Considerations

### Format Matching is Critical

The single most important optimization for `glTexSubImage2D` performance is matching the pixel
format and type parameters with the GPU driver's preferred format. Using the wrong format forces
the driver to perform CPU-side format conversion on every upload.

- **Fast path:** `GL_BGRA` + `GL_UNSIGNED_INT_8_8_8_8_REV` -- matches NVIDIA's native pixel
  layout (which mirrors Microsoft GDI format). NVIDIA benchmarks show approximately **6x faster**
  uploads with `GL_BGRA` vs `GL_RGBA`.
- **Slow path:** `GL_RGB` + `GL_UNSIGNED_BYTE` -- 3-byte formats require padding and swizzling.
  Benchmarked upload performance increases of **over 25x** have been achieved on some hardware
  just from correcting these two parameters.

**Rule of thumb:** Pixel size should be an integer multiple of 32 bits. For all other cases, some
kind of data-padding occurs that slows down the transfer.

### GL_BGRA Availability

`GL_BGRA` and `GL_UNSIGNED_INT_8_8_8_8_REV` are available in OpenGL 1.2 or greater. The
`GL_EXT_BGRA` extension provides this on older implementations. This is widely supported across
all modern GPUs.

### Direct Upload Performance

Calling `glTexSubImage2D` directly (without PBOs) for a texture like 512x512 every frame can
reduce rendering performance drastically because:

1. The call is **synchronous** -- the CPU blocks until the GPU completes any pending operations
   using the texture.
2. The driver must internally copy the data from your application memory to driver-managed memory.
3. The GPU may need to finish rendering the current frame before the texture memory can be
   modified.

### Optimization with PBOs

For significantly better performance, combine `glTexSubImage2D` with Pixel Buffer Objects (PBOs):

1. Map the PBO (`glMapBuffer`)
2. Write pixel data into the mapped buffer
3. Unmap the PBO (`glUnmapBuffer`)
4. Call `glTexSubImage2D` -- with a PBO bound, the `pixels` parameter becomes an offset into
   the PBO rather than a CPU pointer
5. The upload becomes an **asynchronous DMA transfer**, so `glTexSubImage2D` returns immediately

See `gl_pbo_streaming.md` for the complete PBO streaming pattern.

## Error Conditions

- `GL_INVALID_ENUM` if target, format, or type is not an accepted value.
- `GL_INVALID_VALUE` if level < 0 or if xoffset, yoffset, width, or height cause writing
  outside the texture boundaries.
- `GL_INVALID_OPERATION` if the texture array has not been defined by a previous
  `glTexImage2D` or `glTexStorage2D` operation.
- `GL_INVALID_OPERATION` if type is `GL_UNSIGNED_INT_8_8_8_8_REV` and format is neither
  `GL_RGBA` nor `GL_BGRA`.

## Relevance to Q3IDE

For the Q3IDE project, `glTexSubImage2D` is the primary mechanism the Quake3e engine uses for
uploading captured window frames as textures. The capture pipeline (ScreenCaptureKit -> ring
buffer -> C-ABI) delivers frames that must be uploaded to the GPU every frame. Key considerations:

- ScreenCaptureKit on macOS delivers frames in BGRA format, which aligns with the GPU fast path.
- PBO-based double buffering should be used to avoid stalling the render pipeline.
- The texture should be pre-allocated with `glTexImage2D` once, then updated with
  `glTexSubImage2D` each frame.
- For zero-copy on macOS, consider `CGLTexImageIOSurface2D` instead (see
  `iosurface_zero_copy.md`).
