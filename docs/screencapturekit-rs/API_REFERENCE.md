# screencapturekit-rs v1.5.3 — Verified API Reference

This documents the **actual working API** as confirmed through compile-testing against the real crate. Many online docs/examples are outdated or wrong.

## Module Structure

| Module | Types |
|--------|-------|
| `prelude::*` | Re-exports most common types |
| `cm` | CMSampleBuffer, CMTime, IOSurface, IOSurfaceLockOptions, IOSurfaceLockGuard, SCFrameStatus |
| `cv` | CVPixelBuffer |
| `stream` | SCStream, SCContentFilter, SCStreamConfiguration, SCStreamOutputType, SCStreamOutputTrait |
| `shareable_content` | SCShareableContent, SCWindow, SCRunningApplication |
| `cg` | CGRect, CGSize, CGPoint |
| `metal` | MetalDevice, MetalTexture |

## Imports That Work

```rust
use screencapturekit::prelude::*;           // most types
use screencapturekit::SCFrameStatus;        // NOT in prelude
use screencapturekit::cm::IOSurfaceLockOptions; // NOT in prelude
```

## CMSampleBuffer

```rust
sample.frame_status()           // -> Option<SCFrameStatus>
sample.image_buffer()           // -> Option<CVPixelBuffer>
sample.presentation_timestamp() // -> CMTime
```

**Gotchas:**
- `frame_status()` returns `Option<SCFrameStatus>`, not `SCFrameStatus`
- There is NO `pixel_buffer()` method — use `image_buffer()`
- There is NO `io_surface()` on CMSampleBuffer — go through CVPixelBuffer first

## CVPixelBuffer

```rust
pixel_buffer.width()            // -> usize
pixel_buffer.height()           // -> usize
pixel_buffer.bytes_per_row()    // -> usize
pixel_buffer.io_surface()       // -> Option<IOSurface>
pixel_buffer.lock_read_only()   // -> lock guard (for the pixel buffer itself)
```

**Gotchas:**
- There is NO `base_address()` on CVPixelBuffer
- There is NO `lock_base_address()` on CVPixelBuffer
- To get pixel data, go through IOSurface lock guard

## IOSurface

```rust
surface.width()                 // -> usize
surface.height()                // -> usize
surface.bytes_per_row()         // -> usize
surface.pixel_format()          // -> u32
surface.is_in_use()             // -> bool
surface.lock(options)           // -> Result<IOSurfaceLockGuard>
```

**Gotchas:**
- There is NO `base_address()` on IOSurface directly
- You MUST lock first, then access data on the guard

## IOSurfaceLockOptions

This is a **bitflags struct**, not an enum. Construct with:

```rust
IOSurfaceLockOptions::from_bits(1)  // read-only (kIOSurfaceLockReadOnly = 1)
IOSurfaceLockOptions::from_bits(0)  // read-write
```

There is NO `ReadOnly` variant or `read_only()` constructor.

## IOSurfaceLockGuard (the key type for pixel access)

```rust
guard.as_slice()                // -> &[u8]  (the pixel data!)
guard.as_slice_mut()            // -> Option<&mut [u8]>
guard.as_ptr()                  // -> *const u8
guard.base_address()            // -> raw pointer
guard.width()                   // -> usize
guard.height()                  // -> usize
guard.bytes_per_row()           // -> usize
guard.row(index)                // -> &[u8] (specific row)
guard.alloc_size()              // -> usize
guard.is_read_only()            // -> bool
```

## CMTime

```rust
ts.as_seconds()                 // -> Option<f64>  (NOT f64 directly)
```

**Gotchas:**
- Returns `Option<f64>`, not `f64`
- There is NO `seconds()` method — use `as_seconds()`

## SCShareableContent

```rust
SCShareableContent::get()       // -> Result<SCShareableContent>
content.windows()               // -> Vec<SCWindow>
```

## SCWindow

```rust
w.window_id()                   // -> u32
w.title()                       // -> Option<String>
w.is_on_screen()                // -> bool
w.frame()                       // -> CGRect
w.owning_application()          // -> Option<SCRunningApplication>
```

## SCRunningApplication

```rust
app.application_name()          // -> String  (NOT Option<String>)
```

## CGRect / CGSize

```rust
rect.size()                     // -> CGSize  (method, not field)
size.width                      // -> f64     (field, not method)
size.height                     // -> f64     (field, not method)
```

**Gotchas:**
- `size()` is a METHOD with parentheses
- `width`/`height` on CGSize are FIELDS without parentheses

## SCContentFilter

```rust
SCContentFilter::create()
    .with_window(&sc_window)
    .build()
```

There is NO `builder()` method — use `create()`.

## SCStreamConfiguration

```rust
SCStreamConfiguration::new()
    .with_width(w)
    .with_height(h)
    .with_pixel_format(PixelFormat::BGRA)
    .with_shows_cursor(false)
    .with_queue_depth(3)
```

## SCStream

```rust
let mut stream = SCStream::new(&filter, &config);
stream.add_output_handler(handler, SCStreamOutputType::Screen);
stream.start_capture()          // -> Result<()>
stream.stop_capture()           // -> Result<()>
```

## SCStreamOutputTrait

```rust
impl SCStreamOutputTrait for MyHandler {
    fn did_output_sample_buffer(
        &self,
        sample: CMSampleBuffer,
        _output_type: SCStreamOutputType,
    ) {
        // handle frame here
    }
}
```

## Complete Working Pattern (pixel data extraction)

```rust
use screencapturekit::prelude::*;
use screencapturekit::SCFrameStatus;
use screencapturekit::cm::IOSurfaceLockOptions;

fn did_output_sample_buffer(&self, sample: CMSampleBuffer, _: SCStreamOutputType) {
    // 1. Check frame is complete
    if !matches!(sample.frame_status(), Some(SCFrameStatus::Complete)) {
        return;
    }

    // 2. Get pixel buffer
    let Some(pixel_buffer) = sample.image_buffer() else { return; };
    let width = pixel_buffer.width();
    let height = pixel_buffer.height();
    let stride = pixel_buffer.bytes_per_row();

    // 3. Lock IOSurface and copy pixels
    let Some(surface) = pixel_buffer.io_surface() else { return; };
    let Ok(guard) = surface.lock(IOSurfaceLockOptions::from_bits(1)) else { return; };
    let pixels: Vec<u8> = guard.as_slice().to_vec();
    // guard drops here, unlocking

    // 4. Get timestamp
    let timestamp_ns = sample.presentation_timestamp()
        .as_seconds()
        .map(|s| (s * 1_000_000_000.0) as u64)
        .unwrap_or(0);
}
```
