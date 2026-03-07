# Pixel Data Access from ScreenCaptureKit

> **Source:** Compiled from multiple sources via web search
> **Search Query:** "ScreenCaptureKit CMSampleBuffer get pixel data IOSurface BGRA"
> **Fetched:** 2026-03-07
> **Description:** How to extract raw pixel data (BGRA bytes, IOSurface, CVPixelBuffer) from ScreenCaptureKit's CMSampleBuffer output. Critical reference for the q3ide-capture Rust dylib that needs to copy frame data into a ring buffer for Quake3e texture upload.
>
> **Key Sources:**
> - [Apple Developer Forums: How do I convert a CVPixelBuffer to Data?](https://developer.apple.com/forums/thread/94379)
> - [Apple Developer Forums: Pixel data from CaptureOutput](https://developer.apple.com/forums/thread/27167)
> - [Apple Tech Q&A QA1781: Creating IOSurface-backed CVPixelBuffers](https://developer.apple.com/library/archive/qa/qa1781/_index.html)
> - [WWDC22 Session 10155: Take ScreenCaptureKit to the next level](https://developer.apple.com/videos/play/wwdc2022/10155/)
> - [screencapturekit-rs Rust crate](https://github.com/doom-fish/screencapturekit-rs)

---

## Overview

ScreenCaptureKit delivers captured frames as `CMSampleBuffer` objects through the `SCStreamOutput` protocol. Each video sample buffer wraps a `CVPixelBuffer` which is backed by an `IOSurface` -- a GPU-resident buffer that can be accessed for both CPU reads and GPU texture uploads.

This document covers the complete pipeline from `CMSampleBuffer` to raw pixel bytes.

---

## The Data Pipeline

```
SCStream -> CMSampleBuffer -> CVPixelBuffer -> IOSurface -> Raw Pixel Data (BGRA bytes)
                                                         -> Metal/OpenGL Texture (zero-copy)
```

---

## Step 1: Extract CVPixelBuffer from CMSampleBuffer

When your `SCStreamOutput` delegate receives a callback:

```swift
func stream(_ stream: SCStream,
            didOutputSampleBuffer sampleBuffer: CMSampleBuffer,
            of outputType: SCStreamOutputType) {

    guard outputType == .screen else { return }
    guard sampleBuffer.isValid else { return }

    // Extract the pixel buffer (image data)
    guard let pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
    // pixelBuffer is a CVPixelBuffer (aka CVImageBuffer)
}
```

**Objective-C equivalent:**
```objc
CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
```

---

## Step 2: Access Raw Pixel Data via CVPixelBuffer

### Lock the Base Address

Before accessing pixel data on the CPU, you must lock the pixel buffer:

```swift
// Lock for read-only access (use [] for read-write)
CVPixelBufferLockBaseAddress(pixelBuffer, .readOnly)

// Get buffer dimensions
let width = CVPixelBufferGetWidth(pixelBuffer)
let height = CVPixelBufferGetHeight(pixelBuffer)
let bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer)

// Get pointer to raw pixel data
guard let baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer) else {
    CVPixelBufferUnlockBaseAddress(pixelBuffer, .readOnly)
    return
}

// For BGRA format: each pixel is 4 bytes (Blue, Green, Red, Alpha)
let totalBytes = bytesPerRow * height

// Copy data if needed
let data = Data(bytes: baseAddress, count: totalBytes)

// IMPORTANT: Always unlock when done
CVPixelBufferUnlockBaseAddress(pixelBuffer, .readOnly)
```

**Objective-C equivalent:**
```objc
CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

size_t width = CVPixelBufferGetWidth(pixelBuffer);
size_t height = CVPixelBufferGetHeight(pixelBuffer);
size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);

// Copy into your buffer
memcpy(destBuffer, baseAddress, bytesPerRow * height);

CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
```

### Planar Buffers (YUV formats)

For YUV pixel formats (e.g., `kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange`), the buffer has multiple planes:

```swift
let planeCount = CVPixelBufferGetPlaneCount(pixelBuffer)

for plane in 0..<planeCount {
    let planeBaseAddress = CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, plane)
    let planeWidth = CVPixelBufferGetWidthOfPlane(pixelBuffer, plane)
    let planeHeight = CVPixelBufferGetHeightOfPlane(pixelBuffer, plane)
    let planeBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, plane)
    // Process plane data...
}
```

---

## Step 3: Access IOSurface Directly

### Get IOSurface from CVPixelBuffer

ScreenCaptureKit's pixel buffers are always IOSurface-backed:

```swift
// Get the backing IOSurface
guard let surfaceRef = CVPixelBufferGetIOSurface(pixelBuffer)?.takeUnretainedValue() else { return }
let surface = unsafeBitCast(surfaceRef, to: IOSurface.self)
```

**Objective-C equivalent:**
```objc
IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixelBuffer);
```

### Read Pixel Data from IOSurface

```swift
// Lock the IOSurface for CPU read access
surface.lock(options: .readOnly, seed: nil)

let baseAddress = surface.baseAddress
let bytesPerRow = surface.bytesPerRow
let width = surface.width
let height = surface.height
let pixelFormat = surface.pixelFormat  // e.g., kCVPixelFormatType_32BGRA

// Copy data
memcpy(destBuffer, baseAddress, bytesPerRow * height)

// Unlock
surface.unlock(options: .readOnly, seed: nil)
```

**Objective-C equivalent:**
```objc
IOSurfaceLock(surface, kIOSurfaceLockReadOnly, NULL);

void *baseAddress = IOSurfaceGetBaseAddress(surface);
size_t bytesPerRow = IOSurfaceGetBytesPerRow(surface);
size_t width = IOSurfaceGetWidth(surface);
size_t height = IOSurfaceGetHeight(surface);

memcpy(destBuffer, baseAddress, bytesPerRow * height);

IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, NULL);
```

---

## Step 4: GPU Texture Upload (Zero-Copy)

### Metal Texture from IOSurface

For GPU-accelerated rendering, create a Metal texture directly from the IOSurface without CPU copies:

```swift
let textureDescriptor = MTLTextureDescriptor.texture2DDescriptor(
    pixelFormat: .bgra8Unorm,
    width: surface.width,
    height: surface.height,
    mipmapped: false
)

let texture = device.makeTexture(
    descriptor: textureDescriptor,
    iosurface: surface,
    plane: 0
)
```

### OpenGL Texture from IOSurface

For OpenGL (relevant to Quake3e engine):

```objc
// Create an OpenGL texture from IOSurface
GLuint textureID;
glGenTextures(1, &textureID);
glBindTexture(GL_TEXTURE_RECTANGLE, textureID);

CGLTexImageIOSurface2D(
    cglContext,
    GL_TEXTURE_RECTANGLE,
    GL_RGBA,                    // internal format
    (GLsizei)IOSurfaceGetWidth(surface),
    (GLsizei)IOSurfaceGetHeight(surface),
    GL_BGRA,                    // format (matches BGRA pixel format)
    GL_UNSIGNED_INT_8_8_8_8_REV,
    surface,
    0                           // plane
);
```

---

## Pixel Format Configuration

### Setting BGRA Output

Configure the stream to output BGRA format (best for direct texture upload and CPU access):

```swift
let config = SCStreamConfiguration()
config.pixelFormat = kCVPixelFormatType_32BGRA  // 'BGRA' fourcc
```

### Common Pixel Formats

| Format Constant | FourCC | Description | Best For |
|----------------|--------|-------------|----------|
| `kCVPixelFormatType_32BGRA` | `BGRA` | 8-bit BGRA, 4 bytes/pixel | On-screen display, texture upload |
| `kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange` | `420v` | YUV 4:2:0 biplanar, video range | Video encoding, streaming |
| `kCVPixelFormatType_420YpCbCr8BiPlanarFullRange` | `420f` | YUV 4:2:0 biplanar, full range | Video encoding with full range |

### BGRA Memory Layout

For `kCVPixelFormatType_32BGRA`, each pixel occupies 4 bytes:

```
Byte 0: Blue   (0-255)
Byte 1: Green  (0-255)
Byte 2: Red    (0-255)
Byte 3: Alpha  (0-255)
```

**Note:** `bytesPerRow` may be larger than `width * 4` due to memory alignment padding. Always use `bytesPerRow` when calculating row offsets, never `width * 4`.

---

## Frame Metadata

### Checking Frame Validity

Always validate the frame before processing:

```swift
// Check sample buffer attachments for frame status
guard let attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer,
                                                                     createIfNecessary: false) as? [[SCStreamFrameInfo: Any]],
      let attachments = attachmentsArray.first,
      let statusRawValue = attachments[SCStreamFrameInfo.status] as? Int,
      let status = SCFrameStatus(rawValue: statusRawValue),
      status == .complete else {
    return  // Frame is not ready or is a
}
```

### Dirty Rects for Incremental Updates

For efficient updates (only copy changed regions):

```swift
if let dirtyRects = attachments[.dirtyRects] as? [CGRect] {
    for rect in dirtyRects {
        // Only copy the changed region from the IOSurface
        let startRow = Int(rect.origin.y)
        let endRow = Int(rect.origin.y + rect.height)
        let startCol = Int(rect.origin.x) * 4  // 4 bytes per pixel (BGRA)
        let copyWidth = Int(rect.width) * 4

        for row in startRow..<endRow {
            let srcOffset = row * bytesPerRow + startCol
            let dstOffset = row * dstBytesPerRow + startCol
            memcpy(dstBuffer + dstOffset, srcBuffer + srcOffset, copyWidth)
        }
    }
}
```

---

## Rust/FFI Considerations (for q3ide-capture)

### Accessing IOSurface from Rust

The `screencapturekit-rs` crate (https://github.com/doom-fish/screencapturekit-rs) provides Rust bindings for ScreenCaptureKit. Key considerations for the q3ide Rust dylib:

1. **CMSampleBuffer** is received in the stream output callback
2. Extract `CVPixelBuffer` via `CMSampleBufferGetImageBuffer()`
3. Lock, read, copy to ring buffer, unlock
4. The C functions (`CVPixelBufferLockBaseAddress`, `CVPixelBufferGetBaseAddress`, etc.) are available via the `core-video-sys` or `core-foundation` Rust crates

### C-ABI Function Signature

For the q3ide capture layer, the frame data can be exposed as:

```c
// C-ABI header (generated by cbindgen)
typedef struct {
    const uint8_t *data;     // Pointer to BGRA pixel data
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_row;
    uint64_t timestamp_ns;   // Presentation timestamp
} Q3IDEFrame;

// Returns the latest captured frame, or NULL if none available
const Q3IDEFrame* q3ide_get_frame(Q3IDEHandle handle);
```

### Performance Notes

- **Zero-copy GPU path:** If the Quake3e engine uses OpenGL, `CGLTexImageIOSurface2D` provides zero-copy texture upload from IOSurface
- **CPU copy path:** For the ring buffer approach, use `memcpy` from locked `CVPixelBuffer` base address
- **Alignment:** IOSurface rows are aligned (typically 64-byte or 256-byte), so `bytesPerRow >= width * 4`
- **Thread safety:** The `SCStreamOutput` callback runs on the queue specified in `addStreamOutput`. Lock appropriately when writing to the ring buffer.

---

## Complete Minimal Example

```swift
import ScreenCaptureKit
import CoreMedia
import CoreVideo

class CaptureHandler: NSObject, SCStreamOutput {
    var latestPixelData: Data?

    func stream(_ stream: SCStream,
                didOutputSampleBuffer sampleBuffer: CMSampleBuffer,
                of outputType: SCStreamOutputType) {

        guard outputType == .screen else { return }
        guard sampleBuffer.isValid else { return }

        // 1. Validate frame status
        guard let attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(
                sampleBuffer, createIfNecessary: false) as? [[SCStreamFrameInfo: Any]],
              let attachments = attachmentsArray.first,
              let statusRawValue = attachments[SCStreamFrameInfo.status] as? Int,
              let status = SCFrameStatus(rawValue: statusRawValue),
              status == .complete else { return }

        // 2. Get pixel buffer
        guard let pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }

        // 3. Lock and read pixel data
        CVPixelBufferLockBaseAddress(pixelBuffer, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(pixelBuffer, .readOnly) }

        guard let baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer) else { return }

        let width = CVPixelBufferGetWidth(pixelBuffer)
        let height = CVPixelBufferGetHeight(pixelBuffer)
        let bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer)

        // 4. Copy raw BGRA data
        let totalBytes = bytesPerRow * height
        latestPixelData = Data(bytes: baseAddress, count: totalBytes)

        // Frame is now available as raw BGRA bytes
        print("Captured frame: \(width)x\(height), \(totalBytes) bytes")
    }
}

// Setup
func startCapture() async throws {
    let content = try await SCShareableContent.excludingDesktopWindows(false,
                                                                       onScreenWindowsOnly: true)
    guard let window = content.windows.first else { return }

    let filter = SCContentFilter(desktopIndependentWindow: window)

    let config = SCStreamConfiguration()
    config.pixelFormat = kCVPixelFormatType_32BGRA
    config.width = Int(window.frame.width) * 2
    config.height = Int(window.frame.height) * 2
    config.minimumFrameInterval = CMTime(value: 1, timescale: 60)
    config.queueDepth = 5

    let handler = CaptureHandler()
    let stream = SCStream(filter: filter, configuration: config, delegate: nil)
    try stream.addStreamOutput(handler, type: .screen, sampleHandlerQueue: .main)
    try await stream.startCapture()
}
```

---

## References

- [ScreenCaptureKit Documentation](https://developer.apple.com/documentation/screencapturekit)
- [CVPixelBuffer Reference](https://developer.apple.com/documentation/corevideo/cvpixelbuffer-q2e)
- [IOSurface Reference](https://developer.apple.com/documentation/iosurface)
- [CMSampleBuffer Reference](https://developer.apple.com/documentation/coremedia/cmsamplebuffer-u71)
- [CGLTexImageIOSurface2D](https://developer.apple.com/documentation/opengl/1436137-cgltexImageiosurface2d)
- [screencapturekit-rs Rust crate](https://github.com/doom-fish/screencapturekit-rs)
- [Apple Developer Forums: CVPixelBuffer to Data](https://developer.apple.com/forums/thread/94379)
- [Apple Developer Forums: Pixel data from CaptureOutput](https://developer.apple.com/forums/thread/27167)
- [Apple Tech Q&A QA1781](https://developer.apple.com/library/archive/qa/qa1781/_index.html)
