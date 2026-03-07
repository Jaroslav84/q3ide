# OpenGL Dynamic Texture Streaming -- Video Upload Performance

> **Sources:**
> - [OpenGL Pixel Buffer Object (PBO) - Song Ho Ahn](https://www.songho.ca/opengl/gl_pbo.html)
> - [Buffer Object Streaming - OpenGL Wiki](https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming)
> - [Streaming textures with OpenGL, Qt and Gstreamer](https://www.edlangley.co.uk/projects/opengl-streaming-textures/)
> - [opengl-texture-streaming - GitHub](https://github.com/elsampsa/opengl-texture-streaming)
> - [Effective/Resourceful Video Streaming - Khronos Forums](https://community.khronos.org/t/effective-resourceful-video-streaming/109434)
> - [Fastest way to stream video to Texture - GameDev.net](https://www.gamedev.net/forums/topic/641283-fastest-way-to-stream-video-to-texture/5049834/)
> - [Fast Texture Transfers - NVIDIA](https://download.nvidia.com/developer/Papers/2005/Fast_Texture_Transfers/Fast_Texture_Transfers.pdf)
> - [Best Practices for Working with Texture Data - Apple](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/opengl_texturedata.html)

## Overview

Dynamic texture streaming is the process of continuously uploading new image data to GPU textures
at high frame rates, such as when displaying live video or screen captures within an OpenGL
application. This document covers the techniques and performance considerations for achieving
efficient texture streaming in OpenGL.

## Performance Hierarchy (Fastest to Slowest)

1. **Zero-copy IOSurface** (macOS only) -- `CGLTexImageIOSurface2D`, no data transfer at all
2. **PBO double/triple buffering** -- asynchronous DMA, CPU never waits
3. **PBO single buffer** -- asynchronous DMA, but potential stalls
4. **Direct glTexSubImage2D** -- synchronous, CPU blocks during transfer

## Key Technique: Pixel Buffer Objects (PBOs)

The main advantage of PBOs is fast pixel data transfer to and from the GPU through **DMA (Direct
Memory Access)** without involving CPU cycles. With PBOs bound, `glTexSubImage2D` initiates an
asynchronous DMA transfer and returns immediately.

### Double-Buffered PBO Streaming

The highest-performance portable approach uses two PBOs in a ping-pong pattern:

```c
GLuint pbo[2];
int writeIndex = 0;

// Initialization
glGenBuffers(2, pbo);
for (int i = 0; i < 2; i++) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[i]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, NULL, GL_STREAM_DRAW);
}

// Per-frame update
void updateTexture(const void* newFrameData) {
    int readIndex = writeIndex;
    writeIndex = (writeIndex + 1) % 2;

    // Upload from read PBO to texture (async DMA)
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[readIndex]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);

    // Write new data into write PBO
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[writeIndex]);

    // Orphan the buffer to avoid GPU sync stall
    glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, NULL, GL_STREAM_DRAW);

    // Map and write
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(ptr, newFrameData, dataSize);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}
```

The key insight: while `glTexSubImage2D` performs the DMA transfer from PBO[readIndex] to the
texture, the CPU is simultaneously writing new frame data into PBO[writeIndex]. The two
operations happen in parallel.

### Buffer Orphaning

Before mapping a PBO for writing, call `glBufferData` with a NULL pointer to "orphan" the old
buffer. This tells the driver:

- "I don't need the old data anymore"
- The driver can allocate new memory for the map, avoiding a GPU sync stall
- The old memory is freed when the GPU finishes using it

## Format Optimization

### Preferred Formats

| Format | Type | Speed | Notes |
|--------|------|-------|-------|
| `GL_BGRA` | `GL_UNSIGNED_INT_8_8_8_8_REV` | Fastest | Native GPU layout, no swizzle |
| `GL_BGRA` | `GL_UNSIGNED_BYTE` | Fast | Minor swizzle on some GPUs |
| `GL_RGBA` | `GL_UNSIGNED_BYTE` | Medium | Component swizzle required |
| `GL_RGB` | `GL_UNSIGNED_BYTE` | Slow | 3-byte, requires padding to 4-byte |

### Critical Rules

1. **Pixel size must be a multiple of 32 bits.** 3-byte formats (RGB) require internal padding,
   causing major slowdowns.
2. **Match the driver's native format.** On NVIDIA, this is BGRA. On macOS, ScreenCaptureKit
   delivers BGRA natively, which is ideal.
3. **Benchmarked improvements of 25x+** have been seen just from correcting format/type
   parameters on some hardware.

## Video Streaming Pipeline

For streaming video (or screen capture) to textures, a typical architecture:

```
Source (camera/decoder/ScreenCaptureKit)
    |
    v
Frame Queue (ring buffer, lock-free)
    |
    v
Main Render Thread:
    1. Dequeue latest frame
    2. Map PBO (write index)
    3. Copy frame data into PBO
    4. Unmap PBO
    5. Bind PBO (read index), call glTexSubImage2D
    6. Render scene using texture
    7. Swap PBO indices
```

### GStreamer Integration Example

Videos can be played by a GStreamer pipeline using FakeSink as the video sink element. FakeSink
has a callback into the application every time a newly decoded video frame arrives. That buffer
is placed in a queue, handled in the main render thread, which uploads the video buffer into an
OpenGL texture.

## Buffer Object Streaming Techniques (OpenGL Wiki)

The OpenGL Wiki documents several streaming approaches:

### 1. Unsynchronized Mapping

```c
glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size,
    GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
```

This disables implicit synchronization, requiring the application to manage sync manually via
fences. Highest performance, but most complex.

### 2. Persistent Mapping (OpenGL 4.4+)

```c
glBufferStorage(GL_PIXEL_UNPACK_BUFFER, size, NULL,
    GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size,
    GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
// ptr remains valid for the lifetime of the buffer
```

The buffer stays mapped permanently, eliminating map/unmap overhead. Requires manual
synchronization with `glFenceSync` / `glClientWaitSync`.

### 3. Orphaning (Most Compatible)

```c
glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STREAM_DRAW);
// Then map and write
```

The simplest approach. Works on all OpenGL versions that support PBOs. The driver allocates a
new buffer internally, avoiding stalls.

## Performance Benchmarks

From NVIDIA's "Fast Texture Transfers" technical brief and community benchmarks:

| Method | Relative Speed | Notes |
|--------|---------------|-------|
| Direct `glTexSubImage2D` | 1x (baseline) | CPU blocks |
| Single PBO | 2-3x | Async, but potential stalls |
| Double PBO (ping-pong) | 3-5x | Fully pipelined |
| PBO + correct format | 5-25x | Format matching is critical |
| IOSurface (macOS) | Zero-copy | No transfer at all |

## Relevance to Q3IDE

For Q3IDE's screen capture texture streaming pipeline:

1. ScreenCaptureKit delivers frames as IOSurfaces in BGRA format.
2. The **ideal path** on macOS is zero-copy via `CGLTexImageIOSurface2D` (see
   `iosurface_zero_copy.md`).
3. If zero-copy is not feasible (e.g., engine limitations), the fallback should use
   double-buffered PBOs with `GL_BGRA` / `GL_UNSIGNED_INT_8_8_8_8_REV`.
4. The Rust capture dylib's ring buffer aligns with the double-PBO pattern: one slot being
   written by capture, another being read for GPU upload.
