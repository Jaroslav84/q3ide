# screencapturekit-rs - Project Overview and API

> **Source:** https://github.com/svtlabs/screencapturekit-rs
> **Fetched:** 2026-03-07

## Project Summary

screencapturekit-rs is a Rust crate providing safe, idiomatic bindings to Apple's ScreenCaptureKit framework. The library enables developers to capture screen content, windows, and applications on macOS 12.3 and later with high performance and minimal overhead.

- **Repository:** https://github.com/svtlabs/screencapturekit-rs (also known as doom-fish/screencapturekit-rs)
- **Stars:** 192
- **Forks:** 41
- **Commits:** 543
- **License:** Apache-2.0 and MIT dual licensing
- **Current Version:** 1.5.x (as of March 2026)

## Key Features

- **Screen & Window Capture**: Capture displays, windows, or specific applications
- **Audio Capture**: System audio and microphone input recording
- **Real-time Processing**: High-performance frame callbacks with custom dispatch queues
- **Builder Pattern API**: Clean, type-safe configuration
- **Async Support**: Runtime-agnostic async API compatible with Tokio, async-std, and smol
- **IOSurface Access**: Zero-copy GPU texture access for Metal/OpenGL
- **Memory Safe**: Proper reference counting with leak-free design
- **Zero Dependencies**: No runtime dependencies required

## Installation

Add to `Cargo.toml`:

```toml
[dependencies]
screencapturekit = "1"
```

For async support:
```toml
[dependencies]
screencapturekit = { version = "1", features = ["async"] }
```

For latest macOS features:
```toml
[dependencies]
screencapturekit = { version = "1", features = ["macos_26_0"] }
```

## Quick Start Example - Synchronous Capture

```rust
use screencapturekit::prelude::*;

struct Handler;

impl SCStreamOutputTrait for Handler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, _type: SCStreamOutputType) {
        println!("Received frame at {:?}", sample.presentation_timestamp());
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Get available displays
    let content = SCShareableContent::get()?;
    let display = &content.displays()[0];

    // Configure capture
    let filter = SCContentFilter::create()
        .with_display(display)
        .with_excluding_windows(&[])
        .build();

    let config = SCStreamConfiguration::new()
        .with_width(1920)
        .with_height(1080)
        .with_pixel_format(PixelFormat::BGRA);

    // Start streaming
    let mut stream = SCStream::new(&filter, &config);
    stream.add_output_handler(Handler, SCStreamOutputType::Screen);
    stream.start_capture()?;

    // Capture runs in background...
    std::thread::sleep(std::time::Duration::from_secs(5));
    stream.stop_capture()?;

    Ok(())
}
```

## Async Capture Example

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

    let mut stream = AsyncSCStream::new(&filter, &config, 30, SCStreamOutputType::Screen);

    while let Some(frame) = stream.next().await {
        println!("Frame: {:?}", frame.presentation_timestamp());
    }

    Ok(())
}
```

## Window Capture Example

```rust
use screencapturekit::prelude::*;

let content = SCShareableContent::get()?;
let windows = content.windows();
let window = windows
    .iter()
    .find(|w| w.title().as_deref() == Some("Terminal"))
    .ok_or("Window not found")?;

let filter = SCContentFilter::builder()
    .window(window)
    .build();

let config = SCStreamConfiguration::new()
    .with_width(window.frame().size.width as u32)
    .with_height(window.frame().size.height as u32)
    .with_pixel_format(PixelFormat::BGRA);
```

## Architecture & Design Patterns

### Builder Pattern
All types use consistent `::new()` initialization with chainable `with_*()` methods for configuration.

### Stream Configuration
The `SCStreamConfiguration` builder accepts dimensions, pixel formats, audio settings, and quality parameters before stream creation.

### Content Filtering
The `SCContentFilter` builder enables targeting specific displays, windows, or applications with exclusion lists.

## Notable Projects Using This Library

- **AFFiNE** (50k+ GitHub stars)
- **Vibe** (5k+ stars) - Local transcription
- **Lycoris** - Speech recognition and note-taking

## Platform Requirements

- macOS 12.3+ (Monterey)
- Screen Recording Permission (System Preferences -> Privacy & Security)
- Hardened Runtime for notarized apps

## Resources

- **Crates.io**: https://crates.io/crates/screencapturekit
- **Documentation**: https://docs.rs/screencapturekit
- **GitHub**: https://github.com/svtlabs/screencapturekit-rs
