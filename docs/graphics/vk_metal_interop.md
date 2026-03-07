# VK_EXT_metal_objects -- Vulkan/Metal Interop and IOSurface

> **Sources:**
> - [VK_EXT_metal_objects - Vulkan Documentation Project](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_metal_objects.html)
> - [VK_EXT_metal_objects(3) - Vulkan Man Pages](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_metal_objects.html)
> - [VK_EXT_metal_objects proposal - Vulkan-Docs GitHub](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_EXT_metal_objects.adoc)
> - [vulkan_metal.h - Vulkan-Headers GitHub](https://github.com/KhronosGroup/Vulkan-Headers/blob/main/include/vulkan/vulkan_metal.h)
> - [VkExportMetalIOSurfaceInfoEXT(3) - Vulkan Registry](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkExportMetalIOSurfaceInfoEXT.html)
> - [MoltenVK Runtime User Guide](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md)

## Overview

`VK_EXT_metal_objects` is a Vulkan extension that provides the ability to import and export
underlying Metal objects associated with specific Vulkan objects. This extension is only
available on implementations layered on top of Metal (i.e., MoltenVK on Apple platforms).

For Q3IDE, this extension enables the **zero-copy path**: importing IOSurfaces from
ScreenCaptureKit directly as Vulkan images, eliminating all data copies.

## Supported Metal Object Types

The extension supports importing and exporting these Metal object types:

| Metal Object | Vulkan Object | Import Struct | Export Struct |
|-------------|---------------|--------------|--------------|
| MTLDevice | VkDevice | VkImportMetalDeviceInfoEXT | VkExportMetalDeviceInfoEXT |
| MTLCommandQueue | VkQueue | VkImportMetalCommandQueueInfoEXT | VkExportMetalCommandQueueInfoEXT |
| MTLBuffer | VkDeviceMemory | VkImportMetalBufferInfoEXT | VkExportMetalBufferInfoEXT |
| MTLTexture | VkImage | VkImportMetalTextureInfoEXT | VkExportMetalTextureInfoEXT |
| IOSurfaceRef | VkImage | VkImportMetalIOSurfaceInfoEXT | VkExportMetalIOSurfaceInfoEXT |
| MTLSharedEvent | VkSemaphore/VkEvent | VkImportMetalSharedEventInfoEXT | VkExportMetalSharedEventInfoEXT |

## IOSurface Import -- The Zero-Copy Path

### VkImportMetalIOSurfaceInfoEXT

```c
typedef struct VkImportMetalIOSurfaceInfoEXT {
    VkStructureType    sType;       // VK_STRUCTURE_TYPE_IMPORT_METAL_IO_SURFACE_INFO_EXT
    const void*        pNext;
    IOSurfaceRef       ioSurface;   // The IOSurface to import (or NULL)
} VkImportMetalIOSurfaceInfoEXT;
```

- If `ioSurface` is not `NULL_HANDLE`, the IOSurface will be used to underlie the VkImage.
- If `ioSurface` is `NULL_HANDLE`, the implementation creates a new IOSurface to underlie
  the VkImage.

### Creating a VkImage from an IOSurface

```c
// IOSurface received from ScreenCaptureKit
IOSurfaceRef capturedSurface = /* from SCStreamOutput callback */;

// Prepare import info
VkImportMetalIOSurfaceInfoEXT importInfo = {
    .sType = VK_STRUCTURE_TYPE_IMPORT_METAL_IO_SURFACE_INFO_EXT,
    .pNext = NULL,
    .ioSurface = capturedSurface
};

// Prepare export hint (required to enable Metal object export)
VkExportMetalObjectCreateInfoEXT exportCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECT_CREATE_INFO_EXT,
    .pNext = &importInfo,
    .exportObjectType = VK_EXPORT_METAL_OBJECT_TYPE_METAL_IOSURFACE_BIT_EXT
};

// Create VkImage backed by the IOSurface
VkImageCreateInfo imageCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = &exportCreateInfo,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_B8G8R8A8_UNORM,
    .extent = {
        IOSurfaceGetWidth(capturedSurface),
        IOSurfaceGetHeight(capturedSurface),
        1
    },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
};

VkImage capturedImage;
vkCreateImage(device, &imageCreateInfo, NULL, &capturedImage);
```

### IOSurface Export

To retrieve the IOSurface underlying an existing VkImage:

```c
VkExportMetalIOSurfaceInfoEXT exportInfo = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_IO_SURFACE_INFO_EXT,
    .pNext = NULL,
    .image = existingVkImage,
    .ioSurface = NULL  // Will be filled by the call
};

VkExportMetalObjectsInfoEXT exportObjectsInfo = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
    .pNext = &exportInfo
};

vkExportMetalObjectsEXT(device, &exportObjectsInfo);
// exportInfo.ioSurface now contains the IOSurfaceRef
```

## MTLTexture Import

For cases where you have a Metal texture (perhaps from CoreVideo or CoreImage):

```c
VkImportMetalTextureInfoEXT textureImport = {
    .sType = VK_STRUCTURE_TYPE_IMPORT_METAL_TEXTURE_INFO_EXT,
    .pNext = NULL,
    .plane = VK_IMAGE_ASPECT_COLOR_BIT,
    .mtlTexture = existingMTLTexture
};

VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = &textureImport,
    // ... rest of image creation
};
```

## Synchronization via MTLSharedEvent

For synchronizing between Vulkan and Metal operations (e.g., when ScreenCaptureKit finishes
writing to an IOSurface):

```c
// Import an MTLSharedEvent as a VkSemaphore
VkImportMetalSharedEventInfoEXT eventImport = {
    .sType = VK_STRUCTURE_TYPE_IMPORT_METAL_SHARED_EVENT_INFO_EXT,
    .pNext = NULL,
    .mtlSharedEvent = metalSharedEvent
};

VkSemaphoreTypeCreateInfo timelineInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
    .pNext = &eventImport,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    .initialValue = 0
};

VkSemaphoreCreateInfo semInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &timelineInfo
};

VkSemaphore syncSemaphore;
vkCreateSemaphore(device, &semInfo, NULL, &syncSemaphore);
```

This allows Vulkan to wait on Metal events and vice versa, enabling proper synchronization
between ScreenCaptureKit's capture pipeline and Quake3e's render pipeline.

## Per-Frame IOSurface Update Pattern

ScreenCaptureKit delivers a new IOSurface for each captured frame. The recommended pattern:

### Option A: Recreate VkImage Each Frame

```c
// In the capture callback:
void onNewFrame(IOSurfaceRef newSurface) {
    // Destroy previous image
    if (currentImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, currentImage, NULL);
    }

    // Create new image from new IOSurface
    currentImage = createImageFromIOSurface(newSurface);
    currentImageView = createImageView(currentImage);

    // Update descriptor set to point to new image view
    updateDescriptorSet(currentImageView);
}
```

**Drawback:** Frequent VkImage creation/destruction may have overhead.

### Option B: Reuse IOSurface with Stable Dimensions

If the captured window size does not change, ScreenCaptureKit may reuse the same IOSurface
(updating its contents in place). In this case:

```c
// Create VkImage once from the first IOSurface
VkImage capturedImage = createImageFromIOSurface(firstSurface);

// Per-frame: the IOSurface contents update automatically
// Just ensure proper synchronization before rendering
// (use fences/semaphores to wait for capture to finish writing)
```

### Option C: Double-Buffered IOSurface Images

```c
VkImage capturedImages[2];
int currentIndex = 0;

void onNewFrame(IOSurfaceRef newSurface) {
    int writeIndex = (currentIndex + 1) % 2;

    // Destroy and recreate only the write-side image
    vkDestroyImage(device, capturedImages[writeIndex], NULL);
    capturedImages[writeIndex] = createImageFromIOSurface(newSurface);

    // Swap after GPU finishes with current frame
    currentIndex = writeIndex;
}
```

## Platform Availability

- **MoltenVK:** Fully supported. This is the primary use case.
- **Native Vulkan (Linux/Windows):** Not available. This extension only exists on Metal-based
  implementations.
- **visionOS:** Supported via MoltenVK.

## Required Instance/Device Extensions

```c
const char* requiredExtensions[] = {
    VK_EXT_METAL_OBJECTS_EXTENSION_NAME,
    VK_EXT_METAL_SURFACE_EXTENSION_NAME,  // For creating Metal-backed surfaces
};
```

Check availability:
```c
// During physical device selection
VkPhysicalDeviceFeatures2 features = { ... };
// Check if VK_EXT_metal_objects is in the list of available device extensions
```

## Relevance to Q3IDE

This extension is the **critical enabler** for Q3IDE's zero-copy capture-to-render pipeline on
the Vulkan path:

1. **ScreenCaptureKit** delivers captured window content as IOSurfaces.
2. The Rust capture dylib passes the IOSurfaceRef through the C-ABI.
3. The engine adapter uses `VK_EXT_metal_objects` to create a VkImage backed by the IOSurface.
4. Quake3e renders the captured window content with **zero data copies**.

### Integration with Quake3e's Vulkan Renderer

Quake3e's Vulkan renderer manages its own texture objects. The engine adapter needs to:

1. Create a VkImage from the IOSurface using `VkImportMetalIOSurfaceInfoEXT`.
2. Create a VkImageView for sampling.
3. Update the texture descriptor to point to the new image view.
4. Ensure synchronization: the captured IOSurface must not be written to while the GPU is
   reading from it.

### Comparison with OpenGL Path

| Aspect | OpenGL (CGLTexImageIOSurface2D) | Vulkan (VK_EXT_metal_objects) |
|--------|-------------------------------|------------------------------|
| Zero-copy | Yes | Yes |
| Live binding | Automatic | Manual (recreate or sync) |
| Texture target | GL_TEXTURE_RECTANGLE_ARB only | Any VkImage format |
| Coordinate system | Pixel coordinates | Standard UV (normalized) |
| API status on macOS | Deprecated | Active (via MoltenVK) |
| Synchronization | Implicit | Explicit (fences/semaphores) |

The Vulkan path via `VK_EXT_metal_objects` is preferred for new development because:
- It avoids OpenGL's deprecated status on macOS.
- It uses standard normalized texture coordinates.
- It supports any image format, not just `GL_TEXTURE_RECTANGLE_ARB`.
- MoltenVK is actively maintained and supports Vulkan 1.4.
