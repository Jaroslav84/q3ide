# screencapturekit-rs - lib.rs (Main API Entry Point)

> **Source:** https://github.com/svtlabs/screencapturekit-rs/blob/main/screencapturekit/src/lib.rs
> **Fetched:** 2026-03-07
>
> **Note:** The raw source file could not be fetched directly due to network restrictions.
> This document is reconstructed from docs.rs API documentation, GitHub search results,
> and crate documentation pages.

## Overview

The `lib.rs` file serves as the main entry point for the `screencapturekit` crate. It primarily
re-exports types from the internal `screencapturekit-sys` crate and organizes them into a
user-friendly module structure.

## Module Declarations

Based on the API documentation, `lib.rs` declares and re-exports the following modules:

```rust
// Core stream management
pub mod stream;

// Content discovery
pub mod shareable_content;

// Media types
pub mod cm;    // Core Media (CMSampleBuffer, CMTime, IOSurface)
pub mod cv;    // Core Video (CVPixelBuffer, CVPixelBufferPool)
pub mod cg;    // Core Graphics (CGRect, CGPoint, CGSize)

// GPU integration
pub mod metal;

// Supporting infrastructure
pub mod dispatch_queue;
pub mod error;
pub mod utils;

// Convenience re-exports
pub mod prelude;

// Feature-gated modules
#[cfg(feature = "macos_14_0")]
pub mod screenshot_manager;

#[cfg(feature = "macos_14_0")]
pub mod content_sharing_picker;

#[cfg(feature = "macos_15_0")]
pub mod recording_output;

#[cfg(feature = "async")]
pub mod async_api;
```

## Prelude Module

The prelude module provides convenient imports for common use:

```rust
pub mod prelude {
    // Re-exports all commonly used types
    pub use crate::stream::*;
    pub use crate::shareable_content::*;
    pub use crate::cm::*;
    pub use crate::cv::*;
    pub use crate::cg::*;
    pub use crate::error::*;
    pub use crate::dispatch_queue::*;

    #[cfg(feature = "async")]
    pub use crate::async_api::*;

    #[cfg(feature = "macos_14_0")]
    pub use crate::screenshot_manager::*;

    #[cfg(feature = "macos_14_0")]
    pub use crate::content_sharing_picker::*;

    #[cfg(feature = "macos_15_0")]
    pub use crate::recording_output::*;
}
```

## Key Re-exported Types

### From `stream` module:
- `SCStream` - Main capture stream
- `SCStreamConfiguration` - Capture configuration builder
- `SCContentFilter` - Target content filter builder
- `SCStreamOutputType` - Output type enum (Screen, Audio, Microphone)
- `SCStreamOutputTrait` - Trait for frame callbacks
- `SCFrameStatus` - Frame status enum
- `PixelFormat` - Pixel format enum (BGRA, l10r, YCbCr_420v, YCbCr_420f)

### From `shareable_content` module:
- `SCShareableContent` - Query available capture targets
- `SCDisplay` - Display information
- `SCWindow` - Window information
- `SCRunningApplication` - Application information

### From `cm` module:
- `CMSampleBuffer` - Frame data container
- `CMTime` - High-precision timestamp
- `CMSampleTimingInfo` - Sample timing
- `CMFormatDescription` - Format metadata
- `IOSurface` - GPU-backed pixel buffer

### From `cv` module:
- `CVPixelBuffer` - Core Video pixel buffer with lock guards
- `CVPixelBufferPool` - Pixel buffer pool

### From `cg` module:
- `CGRect` - Rectangle
- `CGPoint` - Point
- `CGSize` - Size
- `CGImage` - Core Graphics image

### From `metal` module:
- `MetalDevice` - GPU device wrapper
- `MetalTexture` - Metal texture with retain/release
- `MetalBuffer` - Vertex/uniform buffer
- `MetalCommandQueue` - Command queue
- `MetalCommandBuffer` - Command buffer
- `MetalLayer` - CAMetalLayer wrapper
- `MetalRenderPipelineState` - Compiled pipeline
- `CapturedTextures<T>` - Multi-plane texture container
- `Uniforms` - Shader uniform structure

### From `async_api` module (feature = "async"):
- `AsyncSCShareableContent` - Async content queries
- `AsyncSCStream` - Async stream with frame iteration
- `AsyncSCScreenshotManager` - Async screenshot capture
- `AsyncSCContentSharingPicker` - Async content picker UI

### From `error` module:
- `SCError` - Error type
- `SCResult<T>` - Result alias

## Trait: SCStreamOutputTrait

The primary trait that users implement to receive captured frames:

```rust
pub trait SCStreamOutputTrait {
    fn did_output_sample_buffer(
        &self,
        sample: CMSampleBuffer,
        output_type: SCStreamOutputType,
    );
}
```

## Quality of Service Levels (for dispatch queues)

```rust
pub enum QualityOfService {
    Background,
    Utility,
    Default,
    UserInitiated,
    UserInteractive,
}
```

## Pixel Format Enum

```rust
pub enum PixelFormat {
    BGRA,            // 32-bit BGRA (general purpose)
    l10r,            // 10-bit RGB (HDR content)
    YCbCr_420v,      // YCbCr 4:2:0 video range (video encoding)
    YCbCr_420f,      // YCbCr 4:2:0 full range (video encoding)
}
```

## FourCharCode

Used for pixel format and codec identification:

```rust
pub struct FourCharCode(pub u32);
```
