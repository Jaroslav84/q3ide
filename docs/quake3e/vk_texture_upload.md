# Quake3e Vulkan Texture Upload - Key Functions and Structures

> **Source:** https://raw.githubusercontent.com/ec-/Quake3e/main/code/renderervk/tr_image.c
> **Fetched:** 2026-03-07
> **Purpose:** Detailed analysis of Vulkan texture creation, upload, and management in the Quake3e Vulkan renderer. Critical for Q3IDE's runtime texture injection via the Vulkan backend.

---

## Overview

The Vulkan renderer's `tr_image.c` handles texture management using Vulkan API calls instead
of OpenGL. The high-level structure mirrors the OpenGL renderer, but the upload path is
fundamentally different: all mipmap data is pre-staged into a single contiguous buffer, then
uploaded via a single Vulkan command.

## Key Data Structures

### `Image_Upload_Data`

```c
typedef struct {
    byte *buffer;          // Contiguous buffer with all mip levels
    int buffer_size;       // Total size of the buffer in bytes
    int mip_levels;        // Number of mipmap levels generated
    int base_level_width;  // Width of the base (level 0) mip
    int base_level_height; // Height of the base (level 0) mip
} Image_Upload_Data;
```

This structure is unique to the Vulkan path. It packages all mipmap data into one allocation
for efficient Vulkan staging buffer upload.

### `image_t` (Vulkan variant)

The `image_t` structure in the Vulkan renderer includes Vulkan-specific fields:
- `VkFormat internalFormat` - Vulkan format (e.g., `VK_FORMAT_R8G8B8A8_UNORM`)
- `VkSamplerAddressMode wrapClampMode` - Vulkan wrap mode
- Vulkan image handle, memory, image view, sampler (managed by `vk.c`)

## Core Functions

### `upload_vk_image` - Primary Vulkan texture upload

```c
static void upload_vk_image( image_t *image, byte *pic )
```

**What it does:**
1. Calls `generate_image_upload_data()` to prepare all mipmap levels
2. Determines Vulkan format based on `r_texturebits` and alpha channel presence
3. Calls `vk_create_image()` to create Vulkan image, memory, and image view
4. Calls `vk_upload_image_data()` to transfer pixel data via staging buffer
5. Frees the upload data buffer

**Format selection logic:**
```c
if ( r_texturebits->integer > 16 || r_texturebits->integer == 0 ||
     ( image->flags & IMGFLAG_LIGHTMAP ) ) {
    image->internalFormat = VK_FORMAT_R8G8B8A8_UNORM;  // 32-bit RGBA
} else {
    qboolean has_alpha = RawImage_HasAlpha( upload_data.buffer, w * h );
    image->internalFormat = has_alpha ?
        VK_FORMAT_B4G4R4A4_UNORM_PACK16 :  // 16-bit with alpha
        VK_FORMAT_A1R5G5B5_UNORM_PACK16;   // 16-bit no alpha
}
```

### `generate_image_upload_data` - Mipmap chain generation

```c
static Image_Upload_Data generate_image_upload_data( byte *data, int width, int height,
                                                      image_t *image )
```

**What it does:**
1. Resamples to power-of-2 dimensions if needed
2. Applies picmip reduction
3. Clamps to maximum texture size
4. Applies light scaling (`R_LightScaleTexture()`)
   - Uses `s_gammatable_linear` when FBO is active (Vulkan-specific)
5. Allocates a contiguous buffer for all mip levels
6. Copies base level into buffer
7. If mipmapped: generates each mip level via `R_MipMap()` and appends to buffer
8. Returns the `Image_Upload_Data` structure

### `R_CreateImage` - Vulkan image creation entry point

```c
image_t *R_CreateImage( const char *name, const char *name2, byte *pic,
                        int width, int height, imgFlags_t flags )
```

**What it does (Vulkan-specific parts):**
1. Allocates `image_t` slot (same as OpenGL)
2. Sets Vulkan wrap mode:
   ```c
   if ( flags & IMGFLAG_CLAMPTOBORDER )
       image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
   else if ( flags & IMGFLAG_CLAMPTOEDGE )
       image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   else
       image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   ```
3. Calls `upload_vk_image()` to create Vulkan resources and upload
4. Adds to hash table
5. Returns `image_t*`

### Vulkan Backend Functions (in `vk.c`)

These are called from `tr_image.c` but defined in `vk.c`:

#### `vk_create_image`
Creates the Vulkan image, allocates device memory, creates image view.
- `VkImageCreateInfo` with dimensions, format, mip levels, usage flags
- `vkAllocateMemory` with device-local memory
- `vkCreateImageView` for shader sampling

#### `vk_upload_image_data`
Transfers pixel data from CPU to GPU:
- Creates a staging buffer in host-visible memory
- Copies pixel data to staging buffer
- Records pipeline barrier (UNDEFINED -> TRANSFER_DST_OPTIMAL)
- Records `vkCmdCopyBufferToImage` for each mip level
- Records pipeline barrier (TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL)
- Submits and waits for completion
- Frees staging buffer

#### `vk_update_descriptor_set`
Updates the Vulkan descriptor set for a texture:
- Creates/updates sampler with current filter settings
- Writes descriptor set with image view and sampler

### `GL_TextureMode` - Filter mode changes (Vulkan variant)

```c
void GL_TextureMode( const char *string )
```

**What it does for Vulkan:**
1. Calls `vk_wait_idle()` to synchronize GPU
2. Calls `vk_destroy_samplers()` to rebuild all samplers
3. Iterates all mipmapped textures
4. Calls `vk_update_descriptor_set( img, qtrue )` for each

## Key Differences from OpenGL Path

| Aspect | OpenGL | Vulkan |
|---|---|---|
| Upload granularity | Per-mip-level `glTexImage2D` calls | Single staged buffer with all mips |
| Texture creation | `glGenTextures` + immediate upload | `vkCreateImage` + staging + copy |
| Dynamic updates | `glTexSubImage2D` (fast, in-place) | Requires staging buffer + command submission |
| Format selection | GL_RGBA8, GL_RGB5, etc. | VK_FORMAT_R8G8B8A8_UNORM, etc. |
| Gamma handling | Standard gamma table | FBO-aware linear gamma table |
| Synchronization | Implicit (GL driver handles) | Explicit barriers and fences |

## Vulkan Texture Update Pipeline (Full Flow)

```
1. R_CreateImage("texture_name", NULL, pixels, w, h, flags)
   |
   +-- Allocate image_t slot
   +-- Set Vulkan wrap mode
   +-- upload_vk_image(image, pixels)
       |
       +-- generate_image_upload_data(pixels, w, h, image)
       |   |
       |   +-- ResampleTexture() -- power-of-2 if needed
       |   +-- R_LightScaleTexture() -- gamma/overbright
       |   +-- Allocate contiguous buffer
       |   +-- Copy base level + generate mip chain
       |   +-- Return Image_Upload_Data
       |
       +-- Determine VkFormat
       +-- vk_create_image(w, h, format, mip_levels, ...)
       |   +-- vkCreateImage()
       |   +-- vkAllocateMemory()
       |   +-- vkBindImageMemory()
       |   +-- vkCreateImageView()
       |
       +-- vk_upload_image_data(image, upload_data)
       |   +-- Create staging buffer
       |   +-- memcpy pixel data to staging
       |   +-- Pipeline barrier: UNDEFINED -> TRANSFER_DST
       |   +-- vkCmdCopyBufferToImage (per mip level)
       |   +-- Pipeline barrier: TRANSFER_DST -> SHADER_READ_ONLY
       |   +-- Submit command buffer
       |   +-- Wait for completion
       |   +-- Free staging buffer
       |
       +-- Free upload data buffer
   |
   +-- Add to hash table
   +-- Return image_t*
```

## Q3IDE Integration Strategy (Vulkan Path)

### Challenge

Unlike OpenGL's simple `glTexSubImage2D()` for dynamic updates, Vulkan requires:
1. A staging buffer allocation
2. Memory copy to staging buffer
3. Pipeline barriers for layout transitions
4. A command buffer submission
5. GPU synchronization

This is more expensive per-frame than OpenGL's path.

### Recommended Approach

1. **Create capture textures** at initialization using `R_CreateImage()` with appropriate flags.

2. **For per-frame updates,** either:
   - **Option A:** Add a new `vk_update_image_data()` function that reuses a persistent staging buffer (avoids per-frame allocation) and uses the existing command buffer flow.
   - **Option B:** Use Vulkan's `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` for the image memory directly (if the GPU supports it), enabling direct CPU writes without staging. This is simpler but may be slower on discrete GPUs.
   - **Option C:** Double-buffer the staging: while frame N is being rendered, frame N+1's data is being uploaded to the alternate staging buffer.

3. **Format:** Use `VK_FORMAT_B8G8R8A8_UNORM` to match macOS ScreenCaptureKit's native BGRA output format, avoiding per-pixel swizzling.

4. **Synchronization:** The capture texture update must happen before the frame's draw commands that reference it. Best integration point is during `BeginFrame()` or early in the render command processing.

5. **Skip mipmaps** for capture textures (single mip level) to minimize upload cost.

### Performance Considerations

- Vulkan staging buffer upload for 1080p RGBA: ~8MB per frame
- At 60fps: ~480MB/s sustained transfer bandwidth
- Most modern GPUs handle this easily via PCIe bandwidth
- Using persistent mapped staging buffers avoids `vkMapMemory`/`vkUnmapMemory` overhead
- Consider using `VK_KHR_external_memory` for zero-copy sharing with ScreenCaptureKit's IOSurface (advanced optimization)
