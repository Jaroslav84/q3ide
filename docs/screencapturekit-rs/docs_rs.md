# screencapturekit-rs - API Documentation (docs.rs)

> **Source:** https://docs.rs/screencapturekit/latest/screencapturekit/
> **Fetched:** 2026-03-07

## Crate Overview

Safe, idiomatic Rust bindings for Apple's ScreenCaptureKit framework, enabling screen and audio capture on macOS 12.3+.

## Core Features

- **Screen & Window Capture**: Displays, windows, or specific applications
- **Audio Capture**: System audio and microphone input (macOS 13.0+)
- **Real-time Processing**: High-performance frame callbacks with custom dispatch queues
- **Builder Pattern API**: Type-safe configuration
- **Async Support**: Runtime-agnostic async/await
- **Zero-copy GPU Access**: IOSurface for Metal/OpenGL
- **Memory Safe**: Proper reference counting, leak-free design
- **Zero Dependencies**: No runtime dependencies

---

## Module Reference

### `stream` - Stream Configuration and Management

Contains the core types for creating and managing capture streams.

#### Key Types:

**`SCStream`** - Main capture stream
- `SCStream::new(filter: &SCContentFilter, config: &SCStreamConfiguration) -> Self`
- `add_output_handler(handler: impl SCStreamOutputTrait, output_type: SCStreamOutputType)`
- `start_capture() -> Result<()>`
- `stop_capture() -> Result<()>`
- `update_configuration(config: &SCStreamConfiguration) -> Result<()>`
- `update_content_filter(filter: &SCContentFilter) -> Result<()>`

**`SCStreamConfiguration`** - Builder for capture parameters
- `SCStreamConfiguration::new() -> Self`
- `.with_width(width: u32) -> Self`
- `.with_height(height: u32) -> Self`
- `.with_pixel_format(format: PixelFormat) -> Self`
- `.with_minimum_frame_interval(interval: CMTime) -> Self`
- `.with_queue_depth(depth: u32) -> Self`
- `.with_shows_cursor(show: bool) -> Self`
- `.with_captures_audio(capture: bool) -> Self` (macOS 13.0+)
- `.with_sample_rate(rate: f64) -> Self` (macOS 13.0+)
- `.with_channel_count(count: u32) -> Self` (macOS 13.0+)

**`SCContentFilter`** - Target content selection
- `SCContentFilter::create() -> SCContentFilterBuilder`
  - `.with_display(display: &SCDisplay) -> Self`
  - `.with_excluding_windows(windows: &[&SCWindow]) -> Self`
  - `.with_including_windows(windows: &[&SCWindow]) -> Self`
  - `.build() -> SCContentFilter`
- `SCContentFilter::builder() -> SCContentFilterBuilder`
  - `.window(window: &SCWindow) -> Self`
  - `.display(display: &SCDisplay) -> Self`
  - `.exclude_windows(windows: &[&SCWindow]) -> Self`
  - `.build() -> SCContentFilter`

**`SCStreamOutputType`** - Output type enum
```rust
pub enum SCStreamOutputType {
    Screen,
    Audio,       // macOS 13.0+
    Microphone,  // macOS 15.0+
}
```

**`SCStreamOutputTrait`** - Frame callback trait
```rust
pub trait SCStreamOutputTrait {
    fn did_output_sample_buffer(
        &self,
        sample: CMSampleBuffer,
        output_type: SCStreamOutputType,
    );
}
```

**`SCFrameStatus`** - Frame status
```rust
pub enum SCFrameStatus {
    Complete,
    Idle,
    Blank,
    Suspended,
    Started,
    Stopped,
}
```

**`PixelFormat`** - Pixel format options
```rust
pub enum PixelFormat {
    BGRA,          // 32-bit BGRA (general purpose, 4 bytes/pixel)
    l10r,          // 10-bit RGB (HDR content)
    YCbCr_420v,    // YCbCr 4:2:0 video range (video encoding)
    YCbCr_420f,    // YCbCr 4:2:0 full range (video encoding)
}
```

---

### `shareable_content` - Display, Window, and Application Enumeration

**`SCShareableContent`** - Query available capture targets
- `SCShareableContent::get() -> Result<SCShareableContent>`
- `.displays() -> &[SCDisplay]`
- `.windows() -> &[SCWindow]`
- `.applications() -> &[SCRunningApplication]`

**`SCDisplay`** - Display information
- `.display_id() -> u32`
- `.width() -> u32`
- `.height() -> u32`
- `.frame() -> CGRect`

**`SCWindow`** - Window information
- `.window_id() -> u32`
- `.title() -> Option<String>`
- `.frame() -> CGRect`
- `.window_layer() -> i32`
- `.owning_application() -> Option<SCRunningApplication>`
- `.is_on_screen() -> bool`
- `.is_active() -> bool` (macOS 14.2+)

**`SCRunningApplication`** - Application information
- `.application_name() -> Option<String>`
- `.bundle_identifier() -> Option<String>`
- `.process_id() -> i32`

---

### `cm` - Core Media Types

**`CMSampleBuffer`** - Frame data container
- `.presentation_timestamp() -> CMTime`
- `.duration() -> CMTime`
- `.format_description() -> Option<CMFormatDescription>`
- `.pixel_buffer() -> Option<CVPixelBuffer>`
- `.io_surface() -> Option<IOSurface>`
- `.frame_status() -> SCFrameStatus`
- `.audio_buffer_list() -> Option<AudioBufferList>`

**`CMTime`** - High-precision timestamp
- `.value -> i64`
- `.timescale -> i32`
- `.seconds() -> f64`

**`CMSampleTimingInfo`** - Sample timing metadata

**`CMFormatDescription`** - Format metadata
- `.media_type() -> FourCharCode`
- `.dimensions() -> CGSize`

**`IOSurface`** - GPU-backed pixel buffer
- `.width() -> usize`
- `.height() -> usize`
- `.bytes_per_row() -> usize`
- `.pixel_format() -> FourCharCode`
- `.seed() -> u32` (change counter for detecting updates)
- `.base_address() -> *mut u8`
- `.lock() -> IOSurfaceLock`
- `.unlock()`

---

### `cv` - Core Video Types

**`CVPixelBuffer`** - Pixel buffer with lock guards
- `.width() -> usize`
- `.height() -> usize`
- `.bytes_per_row() -> usize`
- `.pixel_format_type() -> FourCharCode`
- `.base_address() -> *const u8`
- `.lock_base_address() -> CVPixelBufferLockGuard` (RAII lock)
- `.io_surface() -> Option<IOSurface>`

**`CVPixelBufferPool`** - Pixel buffer pool for reuse

---

### `cg` - Core Graphics Types

```rust
pub struct CGRect {
    pub origin: CGPoint,
    pub size: CGSize,
}

pub struct CGPoint {
    pub x: f64,
    pub y: f64,
}

pub struct CGSize {
    pub width: f64,
    pub height: f64,
}
```

**`CGImage`** - Core Graphics image (used for screenshots)

---

### `metal` - Metal GPU Integration

**`MetalDevice`** - GPU device wrapper
- `MetalDevice::system_default() -> Option<MetalDevice>`
- `.create_texture(descriptor: &MetalTextureDescriptor) -> MetalTexture`
- `.create_command_queue() -> MetalCommandQueue`
- `.create_buffer(length: usize) -> MetalBuffer`

**`MetalTexture`** - Metal texture with automatic retain/release
- `.width() -> usize`
- `.height() -> usize`
- `.pixel_format() -> MetalPixelFormat`

**`MetalBuffer`** - Vertex/uniform buffer

**`MetalCommandQueue`** - Command submission queue
- `.create_command_buffer() -> MetalCommandBuffer`

**`MetalCommandBuffer`** - Command buffer
- `.commit()`
- `.wait_until_completed()`

**`MetalLayer`** - CAMetalLayer for window rendering

**`MetalRenderPipelineState`** - Compiled render pipeline

**`CapturedTextures<T>`** - Multi-plane texture container (Y + CbCr for YCbCr formats)

**`Uniforms`** - Shader uniform structure

---

### `dispatch_queue` - Custom Dispatch Queues

For controlling which thread receives frame callbacks.

```rust
pub struct DispatchQueue { ... }

impl DispatchQueue {
    pub fn new(label: &str, qos: QualityOfService) -> Self;
}

pub enum QualityOfService {
    Background,
    Utility,
    Default,
    UserInitiated,
    UserInteractive,
}
```

---

### `error` - Error Types

```rust
pub enum SCError {
    // Stream errors
    StreamStartFailed(String),
    StreamStopFailed(String),
    StreamConfigurationFailed(String),

    // Content errors
    ContentNotFound(String),
    PermissionDenied(String),

    // System errors
    SystemError(i32),
}

pub type SCResult<T> = Result<T, SCError>;
```

---

### `screenshot_manager` - Single-Frame Capture (macOS 14.0+)

**`SCScreenshotManager`**
- `.capture_image(filter: &SCContentFilter, config: &SCStreamConfiguration) -> Result<CGImage>`
- `.capture_sample_buffer(filter: &SCContentFilter, config: &SCStreamConfiguration) -> Result<CMSampleBuffer>`

---

### `recording_output` - File Recording (macOS 15.0+)

**`SCRecordingOutput`** - Direct video recording to file
- Configure codec, output URL, file type
- Attach to `SCStream` as output

---

### `content_sharing_picker` - System Content Picker UI (macOS 14.0+)

**`SCContentSharingPicker`** - System UI for user-driven content selection
- `.present()`
- Callbacks: selected content, cancellation, errors

---

### `async_api` - Async Wrappers (feature = "async")

All async APIs are executor-agnostic (Tokio, async-std, smol, etc.).

**`AsyncSCShareableContent`**
- `AsyncSCShareableContent::get() -> Result<SCShareableContent>` (async)

**`AsyncSCStream`**
- `AsyncSCStream::new(filter, config, buffer_capacity, output_type) -> Self`
- `.next() -> Option<CMSampleBuffer>` (async)

**`AsyncSCScreenshotManager`** (macOS 14.0+)
- `.capture_image(...) -> Result<CGImage>` (async)

**`AsyncSCContentSharingPicker`** (macOS 14.0+)
- `.present() -> Result<SCShareableContent>` (async)

---

## Performance Characteristics

On Apple Silicon:
- **1080p:** 30-60 FPS, 30-100ms first frame latency
- **4K:** 15-30 FPS, 50-150ms first frame latency

## Platform Requirements

- macOS 12.3+ (Monterey)
- Screen Recording Permission (System Preferences -> Privacy & Security)
- Hardened Runtime for notarized apps
