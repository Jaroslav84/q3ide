# CGLTexImageIOSurface2D -- Zero-Copy Texture on macOS (OpenGL)

> **Sources:**
> - [Convert IOSurface-backed texture - Apple Developer Forums](https://developer.apple.com/forums/thread/52008)
> - [Rendering macOS in Virtual Reality - Oskar Groth](https://oskargroth.com/blog/rendering-macos-in-vr)
> - [Best Practices for Working with Texture Data - Apple Developer](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/opengl_texturedata.html)
> - [CGLIOSurface.h - macOS SDK Headers](https://github.com/phracker/MacOSX-SDKs/blob/master/MacOSX10.8.sdk/System/Library/Frameworks/OpenGL.framework/Versions/A/Headers/CGLIOSurface.h)
> - [IOSurface compositing OpenGLContextView.m - GitHub (mstange)](https://github.com/mstange/macos-compositing/blob/master/iosurface-compositing/OpenGLContextView.m)
> - [IOSurface meeting notes - Chromium](https://www.chromium.org/developers/design-documents/iosurface-meeting-notes/)
> - [Cross-process Rendering - Russ Bishop](http://www.russbishop.net/cross-process-rendering)
> - [IOSurface - Apple Developer Documentation](https://developer.apple.com/documentation/iosurface)
> - [OpenGL on macOS - Litherum](https://litherum.blogspot.com/2016/08/opengl-on-macos.html)

## Overview

`CGLTexImageIOSurface2D` is the macOS-specific function that binds an OpenGL texture directly to
an IOSurface. This enables **true zero-copy** texture sharing: the GPU reads directly from the
same memory that the window server (or ScreenCaptureKit) writes to. No CPU-side copies, no DMA
transfers, no staging buffers.

This is the fastest possible path for displaying captured screen content as an OpenGL texture on
macOS.

## What is an IOSurface?

An IOSurface is a **kernel-managed chunk of texture memory** that can be:

- Paged on and off the GPU automatically
- **Shared across processes** without any data copies
- Used simultaneously by multiple OpenGL contexts, Metal devices, CoreImage, CoreVideo, and
  other GPU frameworks

Key property: If two different processes create textures backed by the same IOSurface using the
same GPU, there is **no copy back to host memory** just because of crossing process boundaries.

## How ScreenCaptureKit Delivers Frames

ScreenCaptureKit captures window and screen content and delivers frames as `CMSampleBuffer`
objects. Each frame contains:

- An **IOSurface** representing the captured pixel data
- Per-frame metadata (display time, content rect, etc.)

The IOSurface is already in GPU memory, backed by the window server's compositing output. This
is the key enabler for zero-copy rendering.

## CGLTexImageIOSurface2D Function

### Signature

```c
CGLError CGLTexImageIOSurface2D(
    CGLContextObj    ctx,
    GLenum           target,       // Must be GL_TEXTURE_RECTANGLE_ARB
    GLenum           internalFormat,
    GLsizei          width,
    GLsizei          height,
    GLenum           format,
    GLenum           type,
    IOSurfaceRef     ioSurface,
    GLuint           plane
);
```

### Parameters

| Parameter | Description |
|-----------|-------------|
| `ctx` | The CGL context |
| `target` | Must be `GL_TEXTURE_RECTANGLE_ARB` (only supported target) |
| `internalFormat` | Internal texture format (e.g., `GL_RGBA8`) |
| `width` | Width from `IOSurfaceGetWidth(ioSurface)` |
| `height` | Height from `IOSurfaceGetHeight(ioSurface)` |
| `format` | Pixel format (e.g., `GL_BGRA`) |
| `type` | Pixel type (e.g., `GL_UNSIGNED_INT_8_8_8_8_REV`) |
| `ioSurface` | The IOSurface to bind |
| `plane` | Plane index (0 for single-plane formats) |

### How It Works

The function is the rough equivalent of `glTexImage2D`, except the underlying source data comes
from an IOSurface rather than from an explicit pointer.

**Unlike `glTexImage2D`, the binding is "live":** if the contents of the IOSurface change, the
contents become visible to OpenGL **without making another call** to `CGLTexImageIOSurface2D`.

The underlying transfer mechanism combines:
- `GL_UNPACK_CLIENT_STORAGE_APPLE`
- `GL_STORAGE_HINT_CACHED_APPLE`

The transfer is done as a straight DMA to and from system memory and video memory with **no
format conversions of any kind**.

No matter how many different OpenGL contexts (in the same process or not) bind a texture to an
IOSurface, they all share the same system memory and GPU memory copies of the data.

## Usage Example

```c
#include <OpenGL/CGLIOSurface.h>
#include <IOSurface/IOSurface.h>

// One-time setup
GLuint texture;
glGenTextures(1, &texture);
glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);

// Set texture parameters
glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

// Bind IOSurface to texture (can be called once or per-frame with new surfaces)
CGLTexImageIOSurface2D(
    CGLGetCurrentContext(),
    GL_TEXTURE_RECTANGLE_ARB,
    GL_RGBA8,
    (GLsizei)IOSurfaceGetWidth(ioSurface),
    (GLsizei)IOSurfaceGetHeight(ioSurface),
    GL_BGRA,
    GL_UNSIGNED_INT_8_8_8_8_REV,
    ioSurface,
    0  // plane
);
```

### Rendering with GL_TEXTURE_RECTANGLE_ARB

Important: Rectangle textures use **pixel coordinates** (not normalized 0..1 UV coordinates):

```glsl
// Fragment shader
uniform sampler2DRect textureSampler;  // Note: sampler2DRect, not sampler2D
in vec2 texCoord;  // In pixel coordinates (0..width, 0..height)

void main() {
    fragColor = texture(textureSampler, texCoord);
}
```

If the engine uses normalized coordinates, you must scale them:

```glsl
vec2 pixelCoord = texCoord * vec2(textureWidth, textureHeight);
fragColor = texture(textureSampler, pixelCoord);
```

## Limitations

1. **Target restriction:** IOSurface-backed textures are limited to `GL_TEXTURE_RECTANGLE_ARB`.
   Regular `GL_TEXTURE_2D` is not supported.

2. **No mipmapping:** IOSurface-backed textures do not support mipmaps. Only a single LOD level
   is available.

3. **Coordinate system:** `GL_TEXTURE_RECTANGLE_ARB` uses pixel coordinates, not normalized
   [0, 1] texture coordinates. This requires shader modifications.

4. **macOS only:** This API is specific to macOS (and was available on iOS via different
   mechanisms). It is not available on Linux or Windows.

5. **OpenGL deprecation on macOS:** Apple deprecated OpenGL on macOS in favor of Metal as of
   macOS 10.14 (Mojave). The API still works but receives no updates. For long-term
   compatibility, consider the Vulkan/Metal interop path via `VK_EXT_metal_objects`.

## Cross-Process IOSurface Sharing

IOSurfaces can be shared between processes using Mach ports:

```c
// Process A: Create and share
IOSurfaceRef surface = IOSurfaceCreate(properties);
mach_port_t port = IOSurfaceCreateMachPort(surface);
// Send port to Process B via XPC, Mach IPC, etc.

// Process B: Receive and use
IOSurfaceRef sharedSurface = IOSurfaceLookupFromMachPort(port);
// Bind to OpenGL texture via CGLTexImageIOSurface2D
```

This is how the macOS window server shares window content with applications like ScreenCaptureKit.

## Using IOSurface as a Render Target

You can also render **into** an IOSurface by attaching the texture to a framebuffer:

```c
// Create FBO
GLuint fbo;
glGenFramebuffers(1, &fbo);
glBindFramebuffer(GL_FRAMEBUFFER, fbo);

// Bind IOSurface-backed texture to FBO
CGLTexImageIOSurface2D(ctx, GL_TEXTURE_RECTANGLE_ARB, ...);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_RECTANGLE_ARB, texture, 0);

// Render into the FBO -> renders into the IOSurface
```

## Relevance to Q3IDE

This is the **optimal path** for Q3IDE's OpenGL renderer on macOS:

1. **ScreenCaptureKit** delivers captured window frames as IOSurfaces.
2. The Rust capture dylib (`q3ide-capture`) receives IOSurface references via the
   ScreenCaptureKit callback.
3. The IOSurface reference is passed through the C-ABI to the engine adapter.
4. The engine adapter calls `CGLTexImageIOSurface2D` to bind the IOSurface directly to a
   Quake3e texture -- **zero copies, zero CPU involvement in the pixel transfer**.
5. The texture updates automatically as ScreenCaptureKit writes new frames to the IOSurface.

### Quake3e Integration Challenges

- Quake3e uses `GL_TEXTURE_2D` throughout. The `GL_TEXTURE_RECTANGLE_ARB` requirement means
  the engine adapter must handle this difference (possibly using a blit from rectangle to 2D
  texture, or modifying the relevant shaders).
- Rectangle textures use pixel coordinates. The engine's UV coordinate system may need
  adaptation.
- Despite these challenges, the zero-copy benefit (eliminating a full-resolution `memcpy` plus
  GPU upload per frame) is significant and worth the integration effort.
