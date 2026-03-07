# screencapturekit-rs - Usage Examples and Patterns

> **Sources:**
> - Web search: "screencapturekit-rs window capture example rust"
> - Web search: "screencapturekit-rs SCContentFilter with_window with_display"
> - Web search: "screencapturekit AsyncSCStream tokio frame capture"
> - https://github.com/svtlabs/screencapturekit-rs
> - https://docs.rs/screencapturekit/latest/screencapturekit/
> - https://crates.io/crates/screencapturekit
> **Fetched:** 2026-03-07

## Pattern 1: Basic Display Capture (Synchronous)

The simplest capture pattern. Implements `SCStreamOutputTrait` for frame callbacks.

```rust
use screencapturekit::prelude::*;
use std::thread;
use std::time::Duration;

struct FrameHandler;

impl SCStreamOutputTrait for FrameHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, _type: SCStreamOutputType) {
        let ts = sample.presentation_timestamp();
        println!("Frame at {:.3}s", ts.seconds());
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = SCShareableContent::get()?;
    let display = &content.displays()[0];

    let filter = SCContentFilter::create()
        .with_display(display)
        .with_excluding_windows(&[])
        .build();

    let config = SCStreamConfiguration::new()
        .with_width(1920)
        .with_height(1080)
        .with_pixel_format(PixelFormat::BGRA);

    let mut stream = SCStream::new(&filter, &config);
    stream.add_output_handler(FrameHandler, SCStreamOutputType::Screen);
    stream.start_capture()?;

    thread::sleep(Duration::from_secs(5));
    stream.stop_capture()?;
    Ok(())
}
```

## Pattern 2: Window Capture by Title

Find and capture a specific window. Useful for targeting Terminal, IDE, or browser windows.

```rust
use screencapturekit::prelude::*;

fn capture_window_by_title(title: &str) -> Result<(), Box<dyn std::error::Error>> {
    let content = SCShareableContent::get()?;
    let windows = content.windows();

    // Find window by title
    let target = windows
        .iter()
        .find(|w| w.title().as_deref() == Some(title))
        .ok_or_else(|| format!("Window '{}' not found", title))?;

    println!("Found: {:?} ({}x{})",
        target.title(),
        target.frame().size.width,
        target.frame().size.height
    );

    // Filter for single window
    let filter = SCContentFilter::builder()
        .window(target)
        .build();

    let config = SCStreamConfiguration::new()
        .with_width(target.frame().size.width as u32)
        .with_height(target.frame().size.height as u32)
        .with_pixel_format(PixelFormat::BGRA);

    // ... create stream and start capture
    Ok(())
}
```

## Pattern 3: Enumerate Available Windows

List all capturable windows -- useful for implementing `/q3ide_list`.

```rust
use screencapturekit::prelude::*;

fn list_windows() -> Result<Vec<(u32, String, String, f64, f64)>, Box<dyn std::error::Error>> {
    let content = SCShareableContent::get()?;
    let mut result = Vec::new();

    for window in content.windows() {
        // Skip windows that aren't on screen
        if !window.is_on_screen() {
            continue;
        }

        let id = window.window_id();
        let title = window.title().unwrap_or_default();
        let owner = window.owning_application()
            .and_then(|a| a.application_name())
            .unwrap_or_default();
        let width = window.frame().size.width;
        let height = window.frame().size.height;

        result.push((id, title, owner, width, height));
    }

    Ok(result)
}
```

## Pattern 4: Async Frame Iteration (Tokio)

Runtime-agnostic async capture. Works with Tokio, async-std, smol.

```rust
use screencapturekit::prelude::*;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = AsyncSCShareableContent::get().await?;
    let display = &content.displays()[0];

    let filter = SCContentFilter::create()
        .with_display(display)
        .with_excluding_windows(&[])
        .build();

    let config = SCStreamConfiguration::new()
        .with_width(1920)
        .with_height(1080)
        .with_pixel_format(PixelFormat::BGRA);

    // Buffer capacity of 30 frames
    let mut stream = AsyncSCStream::new(
        &filter, &config, 30, SCStreamOutputType::Screen
    );

    while let Some(frame) = stream.next().await {
        let ts = frame.presentation_timestamp();
        println!("Async frame: {:.3}s", ts.seconds());
    }

    Ok(())
}
```

## Pattern 5: IOSurface Zero-Copy GPU Access

Access captured frames as IOSurface for direct GPU texture upload -- critical for real-time rendering.

```rust
use screencapturekit::prelude::*;

struct GPUHandler;

impl SCStreamOutputTrait for GPUHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, _type: SCStreamOutputType) {
        // Get IOSurface directly -- no CPU copy needed
        if let Some(surface) = sample.io_surface() {
            let width = surface.width();
            let height = surface.height();
            let seed = surface.seed(); // Use seed to detect actual changes

            // In a real engine integration:
            // 1. Create a Metal/OpenGL texture from this IOSurface
            // 2. The texture shares memory with the capture -- zero copy
            // 3. Use seed to skip re-upload when content hasn't changed

            println!("IOSurface: {}x{}, seed={}", width, height, seed);
        }
    }
}
```

## Pattern 6: Display Capture with Window Exclusion

Capture a display but exclude your own application's windows.

```rust
use screencapturekit::prelude::*;

fn capture_display_excluding_self(
    own_bundle_id: &str
) -> Result<(), Box<dyn std::error::Error>> {
    let content = SCShareableContent::get()?;
    let display = &content.displays()[0];

    // Find our own windows to exclude
    let own_windows: Vec<&SCWindow> = content.windows()
        .iter()
        .filter(|w| {
            w.owning_application()
                .and_then(|a| a.bundle_identifier())
                .as_deref() == Some(own_bundle_id)
        })
        .collect();

    let filter = SCContentFilter::builder()
        .display(display)
        .exclude_windows(&own_windows)
        .build();

    let config = SCStreamConfiguration::new()
        .with_width(display.width())
        .with_height(display.height())
        .with_pixel_format(PixelFormat::BGRA);

    // ... create stream and start
    Ok(())
}
```

## Pattern 7: Pixel Data Extraction

Read raw pixel bytes from captured frames.

```rust
use screencapturekit::prelude::*;

struct PixelReader;

impl SCStreamOutputTrait for PixelReader {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, _type: SCStreamOutputType) {
        if let Some(pixel_buffer) = sample.pixel_buffer() {
            // RAII lock -- auto-unlocks when guard is dropped
            let _lock = pixel_buffer.lock_base_address();

            let width = pixel_buffer.width();
            let height = pixel_buffer.height();
            let stride = pixel_buffer.bytes_per_row();
            let ptr = pixel_buffer.base_address();

            // BGRA pixel format: each pixel is 4 bytes
            // ptr[y * stride + x * 4 + 0] = Blue
            // ptr[y * stride + x * 4 + 1] = Green
            // ptr[y * stride + x * 4 + 2] = Red
            // ptr[y * stride + x * 4 + 3] = Alpha

            // Copy to your own buffer:
            let total_bytes = height * stride;
            let data = unsafe {
                std::slice::from_raw_parts(ptr, total_bytes)
            };

            // data now contains the raw BGRA pixel bytes
            println!("Read {}x{} frame ({} bytes)", width, height, total_bytes);
        }
    }
}
```

## Pattern 8: Metal Texture from IOSurface

Create a Metal texture directly from a captured IOSurface for GPU rendering.

```rust
use screencapturekit::prelude::*;

struct MetalHandler {
    device: MetalDevice,
}

impl SCStreamOutputTrait for MetalHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, _type: SCStreamOutputType) {
        if let Some(surface) = sample.io_surface() {
            // Create Metal texture from IOSurface -- shares GPU memory
            // No CPU-side copy needed
            let texture = self.device.create_texture_from_iosurface(&surface);

            // Use texture in your render pass
            // texture.width(), texture.height(), etc.
        }
    }
}
```

## Pattern 9: Dynamic Stream Reconfiguration

Update capture parameters while the stream is running.

```rust
use screencapturekit::prelude::*;

fn resize_capture(
    stream: &mut SCStream,
    new_width: u32,
    new_height: u32,
) -> Result<(), Box<dyn std::error::Error>> {
    let new_config = SCStreamConfiguration::new()
        .with_width(new_width)
        .with_height(new_height)
        .with_pixel_format(PixelFormat::BGRA);

    // Update without stopping the stream
    stream.update_configuration(&new_config)?;
    Ok(())
}

fn retarget_capture(
    stream: &mut SCStream,
    new_window: &SCWindow,
) -> Result<(), Box<dyn std::error::Error>> {
    let new_filter = SCContentFilter::builder()
        .window(new_window)
        .build();

    // Switch to a different window without stopping
    stream.update_content_filter(&new_filter)?;
    Ok(())
}
```

---

## Q3IDE-Relevant Integration Notes

For the Q3IDE project (streaming macOS windows as textures in a Quake 3 engine), the most relevant patterns are:

1. **Window enumeration** (Pattern 3) -- for implementing `/q3ide_list`
2. **Window capture by title** (Pattern 2) -- for implementing `/q3ide_attach <window>`
3. **IOSurface zero-copy** (Pattern 5) -- for real-time texture streaming to the engine
4. **Dynamic retargeting** (Pattern 9) -- for switching windows at runtime
5. **Pixel data extraction** (Pattern 7) -- fallback path if IOSurface is unavailable

### Recommended Cargo.toml for Q3IDE:

```toml
[dependencies]
screencapturekit = { version = "1", features = ["macos_14_0"] }
crossbeam = "0.8"  # Ring buffer for frame passing across FFI boundary
```

### Key Considerations:

- **Thread safety**: `SCStreamOutputTrait::did_output_sample_buffer` is called on a dispatch queue thread. Use `crossbeam` channels or ring buffers to pass frames to the engine thread.
- **IOSurface seed**: Use `IOSurface::seed()` to detect when content actually changes, avoiding unnecessary texture re-uploads.
- **Pixel format**: `PixelFormat::BGRA` is the most compatible with OpenGL texture upload (`GL_BGRA`).
- **Permission**: The Quake3e binary must have Screen Recording permission.
- **Performance**: On Apple Silicon, expect 30-60 FPS at 1080p with 30-100ms first frame latency.
