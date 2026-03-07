# screencapturekit-rs - Examples Directory Listing

> **Source:** https://github.com/svtlabs/screencapturekit-rs/tree/main/screencapturekit/examples
> **Fetched:** 2026-03-07

## Available Examples

The screencapturekit-rs repository includes a comprehensive set of examples demonstrating
various capture scenarios. Examples are numbered sequentially and some require specific
feature flags.

### Basic Examples (no feature flags needed)

| Example | File | Description |
|---------|------|-------------|
| 01 | `01_basic_capture.rs` | Simplest screen capture - captures primary display |
| 02 | `02_window_capture.rs` | Capture a specific window by title |
| 03 | `03_audio_capture.rs` | Audio + video capture simultaneously |
| 04 | `04_pixel_access.rs` | Read raw pixel data from frames |
| 06 | `06_iosurface.rs` | Zero-copy GPU buffers via IOSurface |
| 07 | `07_list_content.rs` | List all available shareable content (displays, windows, apps) |
| 09 | `09_closure_handlers.rs` | Closure-based handlers and delegates |
| 12 | `12_stream_updates.rs` | Dynamic configuration and filter updates |
| 14 | `14_app_capture.rs` | Application-based capture filtering |
| 15 | `15_memory_leak_check.rs` | Memory leak detection using `leaks` tool |
| 17 | `17_metal_textures.rs` | Metal texture creation from IOSurface |

### Feature-Gated Examples

| Example | File | Feature Flag | Description |
|---------|------|-------------|-------------|
| 05 | `05_screenshot.rs` | `macos_14_0` | Single screenshot capture |
| 08 | `08_async.rs` | `async` | Async/await API with runtime-agnostic support |
| 10 | `10_recording_output.rs` | `macos_15_0` | Direct-to-file video recording |
| 11 | `11_content_picker.rs` | `macos_14_0` | System UI for content selection |
| 13 | `13_advanced_config.rs` | `macos_15_0` | HDR, presets, microphone capture |

### Advanced/Integration Examples

| Example | File | Description |
|---------|------|-------------|
| 16 | `16_full_metal_app/` | Full Metal GUI application (macOS 14.0+, directory with multiple files) |
| 18 | `18_wgpu_integration.rs` | wgpu GPU rendering integration |
| 19 | `19_ffmpeg_encoding.rs` | FFmpeg video encoding pipeline |
| 20 | `20_egui_viewer.rs` | egui immediate mode GUI viewer |
| 21 | `21_bevy_streaming.rs` | Bevy game engine streaming integration |

## Running Examples

### Basic examples (no feature flags):

```bash
# Simplest capture
cargo run --example 01_basic_capture

# Window capture
cargo run --example 02_window_capture

# Audio capture
cargo run --example 03_audio_capture

# Pixel access
cargo run --example 04_pixel_access

# IOSurface zero-copy
cargo run --example 06_iosurface

# List available content
cargo run --example 07_list_content

# Closure handlers
cargo run --example 09_closure_handlers

# Stream updates
cargo run --example 12_stream_updates

# App-based capture
cargo run --example 14_app_capture

# Metal textures
cargo run --example 17_metal_textures
```

### Feature-gated examples:

```bash
# Screenshot (macOS 14.0+)
cargo run --example 05_screenshot --features macos_14_0

# Async capture
cargo run --example 08_async --features async

# Recording output (macOS 15.0+)
cargo run --example 10_recording_output --features macos_15_0

# Content picker (macOS 14.0+)
cargo run --example 11_content_picker --features macos_14_0

# Advanced config (macOS 15.0+)
cargo run --example 13_advanced_config --features macos_15_0
```

### Integration examples:

```bash
# wgpu integration
cargo run --example 18_wgpu_integration

# FFmpeg encoding
cargo run --example 19_ffmpeg_encoding

# egui viewer
cargo run --example 20_egui_viewer

# Bevy streaming
cargo run --example 21_bevy_streaming
```

## Key Examples for Q3IDE

The most relevant examples for the Q3IDE project (streaming window textures into a game engine) are:

1. **`02_window_capture.rs`** - Shows how to capture a specific window by title
2. **`06_iosurface.rs`** - Zero-copy GPU texture access (critical for real-time performance)
3. **`17_metal_textures.rs`** - Metal texture creation from IOSurface (GPU pipeline)
4. **`04_pixel_access.rs`** - Reading raw pixel data (fallback path)
5. **`07_list_content.rs`** - Enumerating available windows (for `/q3ide_list` command)
6. **`16_full_metal_app/`** - Complete Metal rendering application reference
