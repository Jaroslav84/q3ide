# screencapturekit-rs - Source File Structure

> **Source:** https://github.com/svtlabs/screencapturekit-rs/tree/main/screencapturekit/src
> **Fetched:** 2026-03-07

## Repository Structure

The screencapturekit-rs project is a Cargo workspace with multiple crates:

```
screencapturekit-rs/
├── Cargo.toml                      # Workspace root
├── README.md
├── screencapturekit/               # Main public crate
│   ├── Cargo.toml
│   ├── src/
│   │   └── lib.rs                  # Re-exports from sys crate
│   └── examples/                   # Example programs (see examples_listing.md)
└── screencapturekit-sys/           # Low-level FFI/sys crate (internal)
    ├── Cargo.toml
    └── src/
        └── ...                     # Objective-C bridging, unsafe FFI
```

## Source Modules (screencapturekit/src/)

The main crate's `lib.rs` re-exports from the underlying `screencapturekit-sys` crate. The actual implementation modules are:

### Core Modules

| Module | Path | Purpose |
|--------|------|---------|
| `stream/` | `stream/` | Stream configuration and management |
| `stream/configuration/` | `stream/configuration/` | `SCStreamConfiguration` - capture parameters |
| `stream/content_filter/` | `stream/content_filter/` | `SCContentFilter` - target selection |
| `stream/sc_stream/` | `stream/sc_stream/` | `SCStream` - main capture stream |
| `shareable_content/` | `shareable_content/` | `SCShareableContent`, `SCDisplay`, `SCWindow`, `SCRunningApplication` |
| `error/` | `error/` | Error types and result aliases |

### Media Modules

| Module | Path | Purpose |
|--------|------|---------|
| `cm/` | `cm/` | Core Media types (`CMSampleBuffer`, `CMTime`, `IOSurface`) |
| `cv/` | `cv/` | Core Video types (`CVPixelBuffer`, `CVPixelBufferPool`) |
| `cg/` | `cg/` | Core Graphics types (`CGRect`, `CGPoint`, `CGSize`) |
| `metal/` | `metal/` | Metal GPU integration (textures, shaders, render pipeline) |

### Supporting Modules

| Module | Path | Purpose |
|--------|------|---------|
| `dispatch_queue/` | `dispatch_queue/` | Custom dispatch queues for callbacks |
| `screenshot_manager/` | `screenshot_manager/` | `SCScreenshotManager` (macOS 14.0+) |
| `content_sharing_picker/` | `content_sharing_picker/` | `SCContentSharingPicker` UI (macOS 14.0+) |
| `recording_output/` | `recording_output/` | `SCRecordingOutput` file recording (macOS 15.0+) |
| `async_api/` | `async_api/` | Async wrappers (requires `async` feature) |
| `utils/` | `utils/` | FFI strings, FourCharCode utilities |
| `prelude/` | `prelude/` | Convenience re-exports for common use |

## Module Dependency Graph

```
prelude (re-exports) ──► stream
                     ──► shareable_content
                     ──► cm
                     ──► cv
                     ──► cg
                     ──► error

stream ──► stream/configuration
       ──► stream/content_filter
       ──► stream/sc_stream

sc_stream ──► cm (CMSampleBuffer)
          ──► dispatch_queue
          ──► error

shareable_content ──► cg (CGRect, CGSize)

metal ──► cm (IOSurface)
      ──► cv (CVPixelBuffer)

async_api ──► stream
          ──► shareable_content
          ──► screenshot_manager
          ──► content_sharing_picker
```

## Key Type Locations

| Type | Module |
|------|--------|
| `SCStream` | `stream::sc_stream` |
| `SCStreamConfiguration` | `stream::configuration` |
| `SCContentFilter` | `stream::content_filter` |
| `SCShareableContent` | `shareable_content` |
| `SCDisplay` | `shareable_content` |
| `SCWindow` | `shareable_content` |
| `SCRunningApplication` | `shareable_content` |
| `CMSampleBuffer` | `cm` |
| `CMTime` | `cm` |
| `IOSurface` | `cm` |
| `CVPixelBuffer` | `cv` |
| `SCStreamOutputType` | `stream::sc_stream` |
| `SCStreamOutputTrait` | `stream::sc_stream` |
| `PixelFormat` | `stream::configuration` |
| `AsyncSCStream` | `async_api` |
| `AsyncSCShareableContent` | `async_api` |
| `SCScreenshotManager` | `screenshot_manager` |
| `SCRecordingOutput` | `recording_output` |
| `MetalTexture` | `metal` |
| `MetalDevice` | `metal` |
