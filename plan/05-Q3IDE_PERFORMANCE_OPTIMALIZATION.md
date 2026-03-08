# Q3IDE — Rendering Optimization Ideas

Ideas and techniques for future performance improvements. These are NOT yet in IDE_VISION.md and are NOT part of any batch. This is a brainstorm doc — pick from it when performance becomes a bottleneck.

---

## Texture Upload Optimizations

### Partial Texture Updates (Region-Based)

Instead of uploading the entire Window texture every dirty frame, detect which rectangular region changed and upload only that region via `glTexSubImage2D` with offset parameters.

A terminal scrolling one line only changes ~5% of the pixel area. A cursor blink changes a single character cell. Uploading a 50x20 pixel region instead of 2560x1440 is a ~99.97% bandwidth reduction for a cursor blink.

**How:** Frame differencing — compare new frame to previous frame, find the bounding box of changed pixels. SCK doesn't provide this natively, so it would need a CPU-side or GPU compute comparison pass. Trade: small CPU cost for massive upload savings.

### Texture Atlas / Texture Array

Instead of one GL texture per Window, pack multiple Windows into a single large texture atlas (e.g., 8192x8192). Or use a GL texture array (`GL_TEXTURE_2D_ARRAY`) where each layer is a Window.

**Benefit:** Fewer texture binds per frame. The renderer can draw all Windows in one draw call with a single texture bind, selecting the layer via shader uniform. Reduces driver overhead significantly with 10+ Windows.

**Trade:** More complex UV management. Atlas fragmentation when Windows are different sizes. Array requires all layers to be the same resolution (or pad smaller ones).

### Mipmap Generation for Distance Viewing

Generate mipmaps for Window textures. When a Window is far away, the GPU samples a smaller mipmap level automatically. Reduces texture bandwidth for distant Windows without any CPU-side resolution scaling.

**How:** Call `glGenerateMipmap` after each texture upload. Cheap on modern GPUs. Only regenerate when the texture is dirty.

**Bonus:** Distant Windows look naturally blurred/smoothed instead of aliased, which is visually better.

### Texture Compression (BCn/DXT)

Compress Window textures into GPU-native formats (BC1/BC3/BC7) before upload. Reduces VRAM usage by 4-8x.

- 2560x1440 BGRA = 14.7 MB
- 2560x1440 BC3 = ~3.7 MB
- 8 Windows: 118 MB → ~30 MB

**Trade:** Compression takes CPU time. BC7 is high quality but slow to encode. BC1 is fast but lossy. For code/text, quality matters — BC7 or uncompressed may be the only acceptable options. Could use BC1 for distant/thumbnailed Windows where quality doesn't matter.

### Reduced Color Depth for Terminals

Terminals are mostly 2-8 colors (background, foreground, syntax highlighting). Storing them as full 32-bit BGRA is wasteful. A palette-indexed 8-bit texture with a 256-color palette would be 4x smaller.

**Trade:** Only works for terminal Windows, not browsers or video. Need to detect Window type. Adds shader complexity for palette lookup.

---

## Capture Pipeline Optimizations

### SCK Frame Rate Capping Per Stream

Set `minimumFrameInterval` per `SCStream` based on Window priority:

- Focused Window: `CMTime(1, 60)` (60fps)
- Visible nearby: `CMTime(1, 30)` (30fps)
- Visible far: `CMTime(1, 15)` (15fps)
- Background: `CMTime(1, 5)` (5fps)

This throttles SCK itself — it never even generates frames faster than needed. Reduces SCK's GPU/CPU overhead at the source, not just at upload time.

**Dynamic adjustment:** When the player moves, recalculate priorities and call `stream.updateConfiguration()` with new frame intervals. SCK supports on-the-fly config changes without restarting the stream.

### CVPixelBuffer Pool Reuse

Instead of allocating new pixel buffers for every frame, maintain a pool of pre-allocated `CVPixelBuffer` objects matching each Window's resolution. Reuse them round-robin.

Reduces memory allocation pressure and avoids GC pauses in the Objective-C runtime.

### SCK Pixel Format Optimization

Capture in `kCVPixelFormatType_32BGRA` (current) is simple but not the most efficient. Options:

- `kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange` (YUV 4:2:0): 50% less data than BGRA. Requires YUV→RGB conversion shader. Good for video content (YouTube), bad for text (chroma subsampling blurs text).
- `kCVPixelFormatType_OneComponent8` (grayscale): for terminals that are monochrome. 75% less data.

**Hybrid approach:** Detect Window content type. Terminal = grayscale or low-color. Browser = BGRA. Video = YUV.

### Batch SCK Content Refresh

Instead of calling `SCShareableContent.excludingDesktopWindows()` every time the window list might change, cache the result and only refresh on:

- Timer (every 5 seconds)
- macOS notification that window list changed (via `NSWorkspace` notifications)
- Manual refresh command

Reduces SCK's enumeration overhead.

---

## GPU-Side Optimizations

### Glass Material Blur Caching

The Glass Material shader requires a multi-pass blur of the scene behind the Window. This is expensive if done every frame for every Floating Window.

**Cache the blur:** Only re-render the blur pass when:
- The Window moves
- The camera moves significantly (threshold: >5° rotation or >1m translation)
- The scene behind the Window changes (difficult to detect — use a timer, e.g., re-blur every 200ms)

Between updates, reuse the cached blur texture. Reduces the Glass Material cost from "N blur passes per frame" to "N blur passes per second."

### Shader LOD for Windows

Different shader complexity based on distance:

| Distance | Shader |
|----------|--------|
| Close | Full Glass Material + specular + blur + Ornament rendering |
| Medium | Simplified glass (pre-baked blur, no specular) |
| Far | Flat textured quad, no glass effect |
| Thumbnail | Solid color tinted by dominant Window color |

### Occlusion Culling for Windows

If a Window is behind a BSP wall (fully occluded), skip its texture upload entirely. The Q3 engine already does PVS (Potentially Visible Set) culling for world geometry — hook into this to also cull Windows.

**How:** Each Window has a world position. Check if it's in the current PVS leaf set. If not, skip upload. This is almost free since the PVS is already computed.

### Portal Render-to-Texture Caching

Portal previews are expensive — they render an entire Space to a texture. Cache the Portal texture and only refresh it:

- When the player looks at the Portal (dwell-triggered refresh)
- At a low fixed interval (every 500ms) when not looked at
- Immediately when a notification fires in the destination Space

Between refreshes, display the cached frame. Portals that nobody looks at cost almost nothing.

### Async Compute for Frame Differencing

Use GPU compute shaders to compare the current frame with the previous frame and output a dirty-region bounding box. Faster than CPU-side comparison, and the data is already on the GPU if using IOSurface.

```
Compute shader:
  Input: current frame texture, previous frame texture
  Output: bounding box (min_x, min_y, max_x, max_y) of changed pixels
  If bounding box area < threshold → mark as clean (skip upload)
  If bounding box area < 50% → upload only the region
  If bounding box area > 50% → upload full frame
```

---

## Memory Optimizations

### Texture Streaming Priority Queue

Maintain a priority queue of Window textures sorted by:

1. Distance to player (closest = highest priority)
2. Dirty flag (dirty > clean)
3. Last update time (stale > recent)
4. Window status (Active > Idle)

Each frame, pop the top N items from the queue and upload only those. N is determined by the adaptive budget. This ensures the most important Windows always get updated first.

### VRAM Budget Enforcement

Track total VRAM used by Window textures. Set a soft limit (e.g., 1GB). When approaching the limit:

1. Reduce resolution of lowest-priority Windows
2. Drop mipmaps for distant Windows
3. Compress textures of idle Windows to BCn
4. As last resort, unload textures of Windows in other Spaces entirely (rebuild from capture on return)

### Thumbnail Cache for Home View / File Browser

Home View shows all Spaces as Portal thumbnails. Instead of rendering all 8 Spaces live, cache a static thumbnail per Space. Update the cache:

- When entering/leaving a Space
- When a notification fires in a Space
- On a slow timer (every 10 seconds)

Home View then just displays 8 small cached textures — almost free.

---

## Rendering Pipeline Optimizations

### Window Draw Batching

Sort Windows by texture atlas page (or texture array index) and draw them in a single batch. Minimize state changes (texture binds, shader swaps) between Window draws.

Ideal case: all Windows share one texture array, one shader, one draw call with instanced rendering. Each instance just varies UV coordinates and world-space transform.

### Deferred Ornament Rendering

Ornaments (text, icons, buttons) are small and numerous. Instead of rendering each Ornament individually:

1. Collect all visible Ornaments into a buffer
2. Sort by font atlas / icon atlas
3. Render all text in one batched draw call
4. Render all icons in one batched draw call

Reduces draw call count from O(Windows × Ornaments) to O(1).

### Temporal Reprojection for Portals

When a Portal preview is stale (cached frame is old), use temporal reprojection to warp the cached frame based on camera movement. The Portal content won't be pixel-perfect but will appear to update smoothly without actually re-rendering the destination Space.

Used in VR rendering extensively. Cheap and effective for the "glance at Portal in peripheral vision" use case.

---

## System-Level Optimizations

### Predictive Pre-Fetching

When the player moves toward a Space (velocity vector points at a Portal), start resuming capture for Windows in that Space before arrival. By the time the player steps through the Portal, Windows are already streaming.

**How:** Check player velocity direction each frame. If it intersects a Portal within ~2 seconds of travel time, start warming up that Space's Windows.

### Power/Thermal Awareness

On laptops or thermally constrained systems, monitor CPU/GPU temperature. When thermal throttling approaches:

1. Reduce global capture FPS cap
2. Increase dirty frame threshold (ignore small changes)
3. Reduce Portal preview rates
4. Show thermal warning on Performance Widget

### Multi-Queue Vulkan Uploads

On Vulkan, use a dedicated transfer queue (separate from the graphics queue) for texture uploads. This allows texture data to stream to the GPU while the graphics queue is rendering the scene — true parallelism.

Most discrete GPUs (including RX 580) support at least one dedicated transfer queue.

### IOSurface Texture Sharing Between Renderers

If both OpenGL and Vulkan need the same Window texture (e.g., for render debugging or renderer switching), share the IOSurface between both APIs without copying. macOS supports this natively through `CGLTexImageIOSurface2D` (GL) and `VK_EXT_metal_objects` (Vulkan via MoltenVK) pointing at the same underlying IOSurface.

---

## Content-Aware Optimizations

### Static Content Detection

Beyond dirty frame detection, classify Windows as "static" or "dynamic":

- **Static:** terminal with no output, editor with no typing, idle dashboard. Capture at 1fps or pause entirely.
- **Dynamic:** scrolling code, video playback, active terminal output. Capture at full priority.

**Detection:** Count dirty frames over a rolling window. If <2 dirty frames in the last second → classify as static.

### Content-Type Adaptive Quality

Detect what's in the Window and adjust quality accordingly:

| Content | Resolution Priority | Color Priority | FPS Priority |
|---------|-------------------|----------------|-------------|
| Code / text | High (must be readable) | Low (few colors) | Low (changes infrequently) |
| Video (YouTube) | Medium | High (full color) | High (smooth playback) |
| Dashboard / charts | Medium | Medium | Low |
| Terminal output (scrolling) | High | Low | High (during scroll) |
| Browser (web page) | High | High | Low (mostly static) |

This could be heuristic (based on app bundle ID) or learned (based on frame change patterns).

---

## Measurement & Profiling

### Per-Window Performance Metrics

Track and expose (via Performance Widget or console) per-Window:

- Capture FPS (actual frames received from SCK)
- Upload FPS (actual texture uploads to GPU)
- Dirty ratio (what % of frames were actually dirty)
- Bandwidth (MB/s for this Window's texture data)
- Latency (ms from SCK callback to texture visible in-game)
- VRAM (MB consumed by this Window's textures + mipmaps)
- Skip count (frames skipped due to budget)

### Frame Timing Histogram

Track frame times for both game rendering and texture uploads. Show as a histogram in the Performance Widget. Identify spikes — is the game hitching because of a texture upload? Or is the game itself slow on a particular frame?

### Budget Utilization Graph

Show a rolling graph of "GPU time available for Window updates" vs "GPU time actually used." If utilization is consistently >90%, the player is near the limit and should close Windows or move away.

---

*Pick from this list when a specific bottleneck emerges. Don't implement optimizations before measuring. Profile first, optimize the actual bottleneck, measure again.*