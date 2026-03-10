# Apple ScreenCaptureKit — Lessons Learned

Hard-won knowledge from building a 30-window compositor inside Quake 3.

---

## Frame Delivery Model

SCK is **content-driven**, not timer-driven.

`minimumFrameInterval` is a **ceiling** (maximum rate), not a guaranteed rate.

| Window state | Frames delivered | `replayd` cost |
|---|---|---|
| Active (typing, scrolling, video) | up to N fps (your cap) | proportional to fps |
| Idle (nothing changed) | **0 frames** | near zero |

> Apple confirmed: *"If you make a 10 second recording of a screen where nothing ever changes, you will only ever get 1 frame."*

So with 30 windows and a 25fps cap:
- 29 idle windows + 1 active = **25 frames/sec total**, not 750.
- Setting a cap costs you nothing on idle streams.

**Set `minimumFrameInterval` always.** Default is 60fps. Without it, an active stream at 120Hz refresh rate tries to push 120 frames/sec through `replayd`.

```rust
// In SCStreamConfiguration:
.with_fps(25)
// Internally: CMTime { value: 1, timescale: 25 }
// Meaning: minimum 1/25 second between frames = max 25fps
```

---

## SCFrameStatus

Every frame callback includes a status. Handle all three:

| Status | Meaning | Action |
|---|---|---|
| `Complete` | New pixels, content changed | Upload texture |
| `Started` | First frame from new stream (content may not have changed yet) | **Accept** — idle apps only ever send this + Idle |
| `Idle` | Content unchanged, no new IOSurface | Skip upload, keep last frame |

**Critical bug we hit:** Dropping `Started` frames caused all idle apps (Sourcetree, Docker, Toggl Track) to show black forever. They send `Started` once at stream open, then `Idle` until content changes. Never reject `Started`.

---

## Concurrent Stream Limit

Apple does **not document** a maximum number of concurrent `SCStream` instances.

In practice: **~9–11 streams** before `replayd` silently stops delivering frames. `start_capture()` returns success but callbacks never fire. No error, no log.

**The limit is per-process, not per-display.**

### Workaround: Display Compositing

Instead of 1 stream per window, use **1 stream per physical display**:

```swift
// BAD: 30 windows = 30 streams → hits limit
SCContentFilter(window: windowA)
SCContentFilter(window: windowB)
// ...

// GOOD: 30 windows across 3 displays = 3 streams
SCContentFilter(display: monitor1, including: [winA, winB, winC, ...])
SCContentFilter(display: monitor2, including: [winD, winE, ...])
SCContentFilter(display: monitor3, including: [winF, winG, ...])
```

Each stream delivers a full **display-resolution frame** (e.g. 1920×1080). Each window appears at its **actual screen CGRect** position. Non-window areas are transparent/black. Crop per-window frames using `SCWindow.frame` (CGRect).

**Zero resolution loss** — each window occupies its exact screen pixels. No scaling.

**Catches:**
- Overlapping windows: occluded pixels are lost (same as on-screen reality)
- Window moves: `updateContentFilter` must be called async, crop coordinates go stale until then

This technique is called **display compositing**.

---

## Silent Stream Death

SCK can silently evict streams without any callback or error. Symptoms:
- `start_capture()` returns 0 (success)
- `SCFrameStatus::Started` fires once
- Then nothing forever

**Detection:** track `stream_active` per window. If no frame arrived within N seconds of `start_capture()`, the stream is dead.

**Recovery:** call `start_capture()` again. Periodic watchdog (every 5s) that retries dead streams is enough.

---

## `replayd` — the daemon behind SCK

All SCK streams route through the `replayd` system daemon. Known behaviors:
- Resource pool is fixed size (IOSurface slots, CMSampleBuffer pool)
- Silently drops streams when pool is exhausted
- Restarting `replayd` (or the user logging out/in) resets the pool
- Idle streams hold their slot even with zero frames being delivered — this is why compositing matters

---

## Implementation Reference (this project)

| File | Role |
|---|---|
| `capture/src/screencapturekit.rs` | Rust C-ABI wrapper — `start_capture`, `get_frame`, etc. |
| `capture/screencapturekit-rs/` | Vendored + patched Rust crate (fixed frame routing bug) |
| `quake3e/code/q3ide/q3ide_win_mngr.c` | Stream lifecycle, `RestartEvictedStreams` watchdog |
| `quake3e/code/q3ide/q3ide_params.h` | `Q3IDE_CAPTURE_FPS`, `Q3IDE_MAX_TUNNEL_FPS` |

### Frame routing bug (fixed in vendored crate)

Original `screencapturekit-rs` v1.5.3: `sample_handler()` received the stream pointer but ignored it, broadcasting every frame to **all registered handlers** via a global registry. With N streams, every handler got every stream's frames → frame mixing.

Fix: `STREAM_HANDLER_MAP: Mutex<HashMap<usize, Vec<usize>>>` maps stream pointer → handler IDs. `sample_handler` routes only to the correct stream's handlers.

---

## WWDC Sessions

- [Meet ScreenCaptureKit (WWDC22)](https://developer.apple.com/videos/play/wwdc2022/10156/)
- [Take ScreenCaptureKit to the next level (WWDC22)](https://developer.apple.com/videos/play/wwdc2022/10155/)
- [What's new in ScreenCaptureKit (WWDC23)](https://developer.apple.com/videos/play/wwdc2023/10136/)

## External References

- [SCContentFilter `init(display:including:)` — Apple Docs](https://developer.apple.com/documentation/screencapturekit/sccontentfilter/init(display:including:))
- [SCStreamConfiguration.minimumFrameInterval — Apple Docs](https://developer.apple.com/documentation/screencapturekit/scstreamconfiguration/minimumframeinterval)
- [Recording to disk with ScreenCaptureKit — Nonstrict](https://nonstrict.eu/blog/2023/recording-to-disk-with-screencapturekit/)
- [ScreenCaptureKit: What does SCFrameStatus idle mean — Apple Forums](https://developer.apple.com/forums/thread/718356)
