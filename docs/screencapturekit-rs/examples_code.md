# screencapturekit-rs - Example Code Reconstructions

> **Source:** https://github.com/svtlabs/screencapturekit-rs/tree/main/screencapturekit/examples
> **Supplemental sources:** README.md, docs.rs, web search results
> **Fetched:** 2026-03-07
>
> **Note:** Individual example files could not be fetched directly due to GitHub rate limits.
> These examples are reconstructed from README documentation, search results, and API docs.
> They represent the documented API patterns but may not be byte-for-byte identical to the
> actual example files in the repository.

## Example 01: Basic Capture

Demonstrates the simplest possible screen capture setup.

```rust
use screencapturekit::prelude::*;
use std::thread;
use std::time::Duration;

struct Handler;

impl SCStreamOutputTrait for Handler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, of_type: SCStreamOutputType) {
        println!("Received frame at {:?}", sample.presentation_timestamp());
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Get available content
    let content = SCShareableContent::get()?;
    let display = &content.displays()[0];

    // Configure filter: capture entire display, exclude no windows
    let filter = SCContentFilter::create()
        .with_display(display)
        .with_excluding_windows(&[])
        .build();

    // Configure stream: 1920x1080, BGRA pixel format
    let config = SCStreamConfiguration::new()
        .with_width(1920)
        .with_height(1080)
        .with_pixel_format(PixelFormat::BGRA);

    // Create and start stream
    let mut stream = SCStream::new(&filter, &config);
    stream.add_output_handler(Handler, SCStreamOutputType::Screen);
    stream.start_capture()?;

    // Let it run for 5 seconds
    thread::sleep(Duration::from_secs(5));

    stream.stop_capture()?;
    Ok(())
}
```

## Example 02: Window Capture

Demonstrates capturing a specific window by title.

```rust
use screencapturekit::prelude::*;
use std::thread;
use std::time::Duration;

struct WindowHandler;

impl SCStreamOutputTrait for WindowHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, of_type: SCStreamOutputType) {
        // Access frame data
        let timestamp = sample.presentation_timestamp();
        println!("Window frame at {:?}", timestamp);
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = SCShareableContent::get()?;
    let windows = content.windows();

    // Find a specific window by title
    let target_window = windows
        .iter()
        .find(|w| w.title().as_deref() == Some("Terminal"))
        .ok_or("Target window not found")?;

    println!("Capturing window: {:?} ({}x{})",
        target_window.title(),
        target_window.frame().size.width,
        target_window.frame().size.height
    );

    // Create filter for single window
    let filter = SCContentFilter::builder()
        .window(target_window)
        .build();

    let config = SCStreamConfiguration::new()
        .with_width(target_window.frame().size.width as u32)
        .with_height(target_window.frame().size.height as u32)
        .with_pixel_format(PixelFormat::BGRA);

    let mut stream = SCStream::new(&filter, &config);
    stream.add_output_handler(WindowHandler, SCStreamOutputType::Screen);
    stream.start_capture()?;

    thread::sleep(Duration::from_secs(10));
    stream.stop_capture()?;

    Ok(())
}
```

## Example 04: Pixel Access

Demonstrates reading raw pixel data from captured frames.

```rust
use screencapturekit::prelude::*;

struct PixelHandler;

impl SCStreamOutputTrait for PixelHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, of_type: SCStreamOutputType) {
        // Get pixel buffer from sample
        if let Some(pixel_buffer) = sample.pixel_buffer() {
            // Lock the pixel buffer for CPU access
            let lock = pixel_buffer.lock_base_address();

            let width = pixel_buffer.width();
            let height = pixel_buffer.height();
            let bytes_per_row = pixel_buffer.bytes_per_row();
            let data = pixel_buffer.base_address();

            println!("Frame: {}x{}, {} bytes/row", width, height, bytes_per_row);

            // Access raw pixel data (BGRA format)
            // data is a pointer to the raw pixel bytes
            // Each pixel is 4 bytes: B, G, R, A

            // Lock is automatically released when `lock` is dropped
        }
    }
}
```

## Example 06: IOSurface (Zero-Copy GPU Access)

Demonstrates zero-copy GPU texture access via IOSurface.

```rust
use screencapturekit::prelude::*;

struct IOSurfaceHandler;

impl SCStreamOutputTrait for IOSurfaceHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, of_type: SCStreamOutputType) {
        // Get IOSurface from sample buffer - zero-copy GPU access
        if let Some(surface) = sample.io_surface() {
            let width = surface.width();
            let height = surface.height();
            let pixel_format = surface.pixel_format();
            let seed = surface.seed(); // Change counter

            println!("IOSurface: {}x{}, format: {:?}, seed: {}",
                width, height, pixel_format, seed);

            // The IOSurface can be used directly with Metal or OpenGL
            // for zero-copy texture upload
        }
    }
}
```

## Example 07: List Content

Demonstrates enumerating all available capture targets.

```rust
use screencapturekit::prelude::*;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = SCShareableContent::get()?;

    // List all displays
    println!("=== Displays ===");
    for display in content.displays() {
        println!("  Display {}: {}x{} at ({}, {})",
            display.display_id(),
            display.width(),
            display.height(),
            display.frame().origin.x,
            display.frame().origin.y
        );
    }

    // List all windows
    println!("\n=== Windows ===");
    for window in content.windows() {
        println!("  Window: {:?} (owner: {:?}, layer: {}, {}x{})",
            window.title(),
            window.owning_application().map(|a| a.application_name()),
            window.window_layer(),
            window.frame().size.width,
            window.frame().size.height
        );
    }

    // List all running applications
    println!("\n=== Applications ===");
    for app in content.applications() {
        println!("  App: {:?} (bundle: {:?}, pid: {})",
            app.application_name(),
            app.bundle_identifier(),
            app.process_id()
        );
    }

    Ok(())
}
```

## Example 08: Async Capture

Demonstrates the async/await API (requires `async` feature).

```rust
use screencapturekit::prelude::*;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Async content discovery
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

    // Create async stream with 30 frame buffer capacity
    let mut stream = AsyncSCStream::new(
        &filter,
        &config,
        30,  // buffer capacity
        SCStreamOutputType::Screen,
    );

    // Iterate frames asynchronously
    let mut frame_count = 0;
    while let Some(frame) = stream.next().await {
        println!("Async frame {}: {:?}", frame_count, frame.presentation_timestamp());
        frame_count += 1;
        if frame_count >= 100 {
            break;
        }
    }

    Ok(())
}
```

## Example 14: App Capture

Demonstrates application-based capture filtering.

```rust
use screencapturekit::prelude::*;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = SCShareableContent::get()?;

    // Find a specific application
    let target_app = content.applications()
        .iter()
        .find(|app| app.bundle_identifier().as_deref() == Some("com.apple.Terminal"))
        .ok_or("Application not found")?;

    let display = &content.displays()[0];

    // Get all windows belonging to the target app
    let app_windows: Vec<&SCWindow> = content.windows()
        .iter()
        .filter(|w| {
            w.owning_application()
                .map(|a| a.bundle_identifier() == target_app.bundle_identifier())
                .unwrap_or(false)
        })
        .collect();

    println!("Found {} windows for {:?}", app_windows.len(), target_app.application_name());

    // Create filter to capture only this app's windows on the display
    let filter = SCContentFilter::create()
        .with_display(display)
        .with_excluding_windows(&[])  // or exclude specific windows
        .build();

    let config = SCStreamConfiguration::new()
        .with_width(1920)
        .with_height(1080)
        .with_pixel_format(PixelFormat::BGRA);

    // ... start stream as usual

    Ok(())
}
```

## Display Capture with Window Exclusion

```rust
use screencapturekit::prelude::*;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = SCShareableContent::get()?;
    let display = &content.displays()[0];
    let windows = content.windows();

    // Exclude your own application's windows
    let my_windows: Vec<&SCWindow> = windows
        .iter()
        .filter(|w| w.owning_application()
            .map(|app| app.bundle_identifier().as_deref() == Some("com.myapp"))
            .unwrap_or(false))
        .collect();

    let filter = SCContentFilter::builder()
        .display(display)
        .exclude_windows(&my_windows)
        .build();

    let config = SCStreamConfiguration::new()
        .with_width(display.width())
        .with_height(display.height())
        .with_pixel_format(PixelFormat::BGRA);

    // ... start stream

    Ok(())
}
```
