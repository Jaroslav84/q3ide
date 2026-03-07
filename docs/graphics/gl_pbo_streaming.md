# OpenGL PBO (Pixel Buffer Object) Texture Streaming

> **Sources:**
> - [OpenGL Pixel Buffer Object (PBO) - Song Ho Ahn](https://www.songho.ca/opengl/gl_pbo.html)
> - [Pixel Buffer Object - OpenGL Wiki](https://www.khronos.org/opengl/wiki/Pixel_Buffer_Object)
> - [ARB_pixel_buffer_object - Khronos Registry](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_pixel_buffer_object.txt)
> - [Fast Texture Transfers using PBOs - NVIDIA](https://download.nvidia.com/developer/Papers/2005/Fast_Texture_Transfers/Fast_Texture_Transfers.pdf)
> - [PBO for streaming textures - Khronos Forums](https://community.khronos.org/t/pbo-for-streaming-textures/55043)
> - [Fast texture uploads using PBOs - GitHub Gist (roxlu)](https://gist.github.com/roxlu/4663550)
> - [opengl-texture-streaming - GitHub (elsampsa)](https://github.com/elsampsa/opengl-texture-streaming)
> - [Using PBOs - RIP Tutorial](https://riptutorial.com/opengl/example/28872/using-pbos)

## Overview

Pixel Buffer Objects (PBOs) are OpenGL buffer objects used for asynchronous pixel data transfers
between the CPU and GPU. They are the standard mechanism for high-performance texture streaming
in OpenGL, enabling DMA (Direct Memory Access) transfers that do not block the CPU.

## How PBOs Work

Without PBOs:
```
CPU Memory --[synchronous copy]--> Driver Memory --[DMA]--> GPU Texture
             (CPU blocks here)
```

With PBOs:
```
CPU Memory --[memcpy]--> PBO (driver-managed) --[async DMA]--> GPU Texture
             (fast)       (CPU returns immediately)
```

The main advantage: the DMA transfer from PBO to GPU texture happens **asynchronously**. After
calling `glTexSubImage2D` with a bound PBO, the function returns immediately while the GPU
performs the actual data transfer in the background.

## Binding Targets

PBOs use two binding targets:

| Target | Direction | Use Case |
|--------|-----------|----------|
| `GL_PIXEL_UNPACK_BUFFER` | CPU -> GPU | Texture uploads (streaming) |
| `GL_PIXEL_PACK_BUFFER` | GPU -> CPU | Readback (`glReadPixels`) |

For texture streaming, use `GL_PIXEL_UNPACK_BUFFER`.

## Usage Hints

| Hint | Purpose |
|------|---------|
| `GL_STREAM_DRAW` | Data written once by CPU, used few times by GPU (texture streaming) |
| `GL_STREAM_READ` | Data written once by GPU, read few times by CPU (async readback) |
| `GL_DYNAMIC_DRAW` | Data written repeatedly by CPU, used many times by GPU |

For per-frame texture streaming, use `GL_STREAM_DRAW`.

## Single PBO Streaming

Basic pattern with one PBO:

```c
GLuint pbo;
GLsizei dataSize = width * height * 4;  // BGRA, 4 bytes per pixel

// Initialization
glGenBuffers(1, &pbo);
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, NULL, GL_STREAM_DRAW);

// Per-frame update
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);

// Orphan the buffer (allocate new storage, avoid GPU stall)
glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, NULL, GL_STREAM_DRAW);

// Map the buffer for writing
void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
if (ptr) {
    memcpy(ptr, frameData, dataSize);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
}

// Upload from PBO to texture (asynchronous with PBO bound)
glBindTexture(GL_TEXTURE_2D, textureId);
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                (void*)0);  // Offset into PBO, not a CPU pointer

glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
```

**Note:** When a PBO is bound to `GL_PIXEL_UNPACK_BUFFER`, the last parameter of
`glTexSubImage2D` is interpreted as a byte offset into the PBO, not a CPU pointer. Using `0`
means "start at the beginning of the PBO".

## Double PBO Streaming (Recommended)

The double-PBO pattern achieves full pipelining: while the GPU transfers data from one PBO to
the texture, the CPU writes new frame data into the other PBO.

```c
GLuint pbo[2];
int index = 0;
GLsizei dataSize = width * height * 4;

// Initialization
glGenBuffers(2, pbo);
for (int i = 0; i < 2; i++) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[i]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, NULL, GL_STREAM_DRAW);
}
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

// Per-frame update
void streamFrame(const void* newFrameData) {
    // Swap indices
    index = (index + 1) % 2;
    int nextIndex = (index + 1) % 2;

    // Step 1: Upload from current PBO to texture (async DMA)
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[index]);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                    (void*)0);

    // Step 2: Write new data into the next PBO
    // (This happens in parallel with the DMA from Step 1)
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[nextIndex]);

    // Orphan to avoid stall
    glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, NULL, GL_STREAM_DRAW);

    // Map and write
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr) {
        memcpy(ptr, newFrameData, dataSize);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}
```

### Timeline

```
Frame N:
  GPU: DMA from PBO[0] -> Texture
  CPU: Write new data -> PBO[1]       (parallel!)

Frame N+1:
  GPU: DMA from PBO[1] -> Texture
  CPU: Write new data -> PBO[0]       (parallel!)
```

The frame displayed is always one frame behind the latest capture. This one-frame latency is
typically acceptable for screen capture display.

## Buffer Orphaning Explained

The `glBufferData(target, size, NULL, hint)` call before `glMapBuffer` is critical:

1. Tells the driver: "I don't need the old buffer contents."
2. The driver internally allocates new memory for the map operation.
3. The old memory remains valid until the GPU finishes its DMA transfer.
4. Without orphaning, `glMapBuffer` would block until the GPU finishes -- defeating the purpose
   of async transfer.

### Alternative: glMapBufferRange with Invalidation

```c
void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, dataSize,
    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
```

`GL_MAP_INVALIDATE_BUFFER_BIT` achieves the same effect as orphaning: the driver may allocate
new memory, avoiding synchronization.

## Persistent Mapping (OpenGL 4.4+)

For the highest performance with modern OpenGL:

```c
// One-time setup
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
glBufferStorage(GL_PIXEL_UNPACK_BUFFER, dataSize * NUM_BUFFERS, NULL,
                flags | GL_DYNAMIC_STORAGE_BIT);
void* persistentPtr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                       dataSize * NUM_BUFFERS, flags);

// Per-frame: just write directly, no map/unmap needed
size_t offset = frameIndex * dataSize;
memcpy((char*)persistentPtr + offset, newFrameData, dataSize);

// Sync with fence before reusing a region
glClientWaitSync(fences[frameIndex], GL_SYNC_FLUSH_COMMANDS_BIT, timeout);
glDeleteSync(fences[frameIndex]);

// Upload
glTexSubImage2D(..., (void*)(intptr_t)offset);

// Place fence after draw that uses this texture
fences[frameIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
```

This eliminates all map/unmap overhead. The buffer stays mapped for the entire application
lifetime.

## Performance Comparison

Benchmarks from NVIDIA and community testing:

| Method | 1920x1080 BGRA Upload Time | Notes |
|--------|---------------------------|-------|
| Direct `glTexSubImage2D` | ~16-20 ms | CPU blocks |
| Single PBO | ~5-8 ms | Async, some stalls |
| Double PBO | ~1-3 ms | Fully pipelined |
| Persistent mapping | ~0.5-2 ms | No map/unmap overhead |
| Wrong format (GL_RGB) | ~40+ ms | Format conversion penalty |

*Times are approximate and hardware-dependent.*

## Common Pitfalls

1. **Forgetting to unbind the PBO.** If `GL_PIXEL_UNPACK_BUFFER` is still bound, subsequent
   `glTexImage2D`/`glTexSubImage2D` calls will interpret the pointer as a PBO offset, causing
   crashes or corruption.

2. **Using GL_RGB format.** 3-byte pixels require padding and are dramatically slower. Always
   use 4-byte formats (BGRA or RGBA).

3. **Not orphaning before mapping.** Without orphaning (or invalidation), `glMapBuffer` blocks
   until the GPU finishes, negating async benefits.

4. **Reading from a write-only mapped buffer.** If you map with `GL_WRITE_ONLY` and then read,
   behavior is undefined and often very slow (the driver may place the buffer in write-combined
   memory).

5. **Mixing PBO targets.** Binding the same buffer to both `GL_PIXEL_UNPACK_BUFFER` and
   `GL_PIXEL_PACK_BUFFER` simultaneously causes undefined behavior on some drivers.

## Relevance to Q3IDE

For Q3IDE's OpenGL renderer path in Quake3e:

1. **If zero-copy IOSurface is not feasible** (due to `GL_TEXTURE_RECTANGLE_ARB` limitations),
   double-PBO streaming is the recommended fallback.
2. The Rust capture dylib's `crossbeam` ring buffer naturally maps to the PBO double-buffer
   pattern: one slot being written by the capture thread, another being read for GPU upload.
3. Use `GL_BGRA` + `GL_UNSIGNED_INT_8_8_8_8_REV` to match ScreenCaptureKit's native format.
4. On modern macOS with Apple Silicon, PBO performance is excellent due to unified memory.
5. Quake3e's renderer likely needs minimal modification: just bind a PBO before calling its
   existing texture upload path.
