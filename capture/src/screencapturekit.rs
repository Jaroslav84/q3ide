/// macOS ScreenCaptureKit backend implementation.
///
/// Per-window capture using SCContentFilter(desktopIndependentWindow:).
/// Each window gets its own SCStream with a filter targeting that specific
/// SCWindow by windowID. No frame mixing — each stream delivers frames
/// for its one window only.
///
/// Desktop capture: Composites all displays vertically into a single frame.

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};

use screencapturekit::prelude::*;
use screencapturekit::SCFrameStatus;

use crate::backend::{CaptureBackend, CaptureError, DisplayInfo, FrameData, Result, WindowInfo};
use crate::ringbuf::FrameRingBuffer;

/// Special window ID for desktop capture (all displays composited).
pub const DESKTOP_CAPTURE_ID: u32 = 0xFFFF_FFFF;

// ─── Shared frame extraction ────────────────────────────────────────────────

/// Raw pixel data extracted from a CMSampleBuffer.
struct RawPixels {
    pixels: Vec<u8>,
    width: u32,
    height: u32,
    stride: u32,
    timestamp_ns: u64,
}

/// Extract BGRA pixel data from a CMSampleBuffer.
/// Returns None for non-screen, non-complete, or empty frames.
fn extract_pixels(sample: &CMSampleBuffer, output_type: SCStreamOutputType) -> Option<RawPixels> {
    if output_type != SCStreamOutputType::Screen {
        return None;
    }

    match sample.frame_status() {
        Some(SCFrameStatus::Complete) | None => {}
        Some(_) => return None,
    }

    let pixel_buffer = sample.image_buffer()?;
    let width = pixel_buffer.width();
    let height = pixel_buffer.height();

    if width == 0 || height == 0 {
        return None;
    }

    let guard = pixel_buffer.lock_read_only().ok()?;
    let pixels = guard.as_slice().to_vec();
    drop(guard);

    let timestamp_ns = sample
        .presentation_timestamp()
        .as_seconds()
        .map(|s| (s * 1_000_000_000.0) as u64)
        .unwrap_or(0);

    Some(RawPixels {
        pixels,
        width: width as u32,
        height: height as u32,
        stride: pixel_buffer.bytes_per_row() as u32,
        timestamp_ns,
    })
}

/// Log frame at intervals: first 3 frames, then every 300th.
fn should_log_frame(count: u64) -> bool {
    count < 3 || count % 300 == 0
}

// ─── Per-window capture ─────────────────────────────────────────────────────

/// Per-window capture session — one SCStream per window.
struct WindowCaptureSession {
    stream: SCStream,
    latest_frame: Arc<Mutex<Option<FrameData>>>,
}

/// Frame handler for a single window's SCStream.
struct WindowFrameHandler {
    latest_frame: Arc<Mutex<Option<FrameData>>>,
    window_id: u32,
    frame_count: AtomicU64,
}

impl SCStreamOutputTrait for WindowFrameHandler {
    fn did_output_sample_buffer(
        &self,
        sample: CMSampleBuffer,
        output_type: SCStreamOutputType,
    ) {
        let Some(raw) = extract_pixels(&sample, output_type) else {
            return;
        };

        let n = self.frame_count.fetch_add(1, Ordering::Relaxed);
        if should_log_frame(n) {
            log::info!(
                "capture: wid={} frame={} {}x{}", self.window_id, n, raw.width, raw.height
            );
        }

        let frame = FrameData {
            pixels: raw.pixels,
            width: raw.width,
            height: raw.height,
            stride: raw.stride,
            timestamp_ns: raw.timestamp_ns,
            source_wid: self.window_id,
        };

        if let Ok(mut slot) = self.latest_frame.lock() {
            *slot = Some(frame);
        }
    }
}

// ─── Desktop composite capture ──────────────────────────────────────────────

#[allow(dead_code)]
struct DesktopCaptureSession {
    ring_buffer: FrameRingBuffer,
    streams: Vec<SCStream>,
    display_frames: Arc<Mutex<Vec<Option<DisplayFrame>>>>,
    layout: CompositeLayout,
}

#[derive(Clone)]
struct DisplayFrame {
    pixels: Vec<u8>,
    width: u32,
    height: u32,
}

/// Layout for compositing multiple displays into one frame.
#[derive(Clone)]
struct CompositeLayout {
    total_width: u32,
    total_height: u32,
    entries: Vec<CompositeEntry>,
}

#[derive(Clone)]
struct CompositeEntry {
    scaled_width: u32,
    scaled_height: u32,
    y_offset: u32,
}

impl CompositeLayout {
    fn from_displays(displays: &[SCDisplay]) -> Self {
        let total_width = displays.iter().map(|d| d.width()).max().unwrap_or(1920);
        let mut entries = Vec::with_capacity(displays.len());
        let mut y = 0u32;

        for d in displays {
            let aspect = d.height() as f32 / d.width() as f32;
            let h = (total_width as f32 * aspect) as u32;
            entries.push(CompositeEntry {
                scaled_width: total_width,
                scaled_height: h,
                y_offset: y,
            });
            y += h;
        }

        Self {
            total_width,
            total_height: y,
            entries,
        }
    }

    /// Composite all display frames into a single BGRA buffer.
    fn composite(&self, frames: &[Option<DisplayFrame>]) -> Option<Vec<u8>> {
        if !frames.iter().all(|f| f.is_some()) {
            return None;
        }

        let row_bytes = self.total_width * 4;
        let mut out = vec![0u8; (row_bytes * self.total_height) as usize];

        for (i, frame_opt) in frames.iter().enumerate() {
            let frame = frame_opt.as_ref().unwrap();
            let entry = &self.entries[i];
            let src_row_bytes = frame.width * 4;

            for dst_y in 0..entry.scaled_height {
                let src_y = ((dst_y as f32 * frame.height as f32 / entry.scaled_height as f32) as u32)
                    .min(frame.height - 1);
                let dst_row = ((entry.y_offset + dst_y) * row_bytes) as usize;
                let src_row = (src_y * src_row_bytes) as usize;

                for dst_x in 0..entry.scaled_width {
                    let src_x = ((dst_x as f32 * frame.width as f32 / entry.scaled_width as f32) as u32)
                        .min(frame.width - 1);
                    let d = dst_row + (dst_x * 4) as usize;
                    let s = src_row + (src_x * 4) as usize;

                    if d + 3 < out.len() && s + 3 < frame.pixels.len() {
                        out[d..d + 4].copy_from_slice(&frame.pixels[s..s + 4]);
                    }
                }
            }
        }

        Some(out)
    }
}

/// Frame handler for one display in a desktop composite.
struct DesktopDisplayHandler {
    display_frames: Arc<Mutex<Vec<Option<DisplayFrame>>>>,
    display_index: usize,
    composite_ring: FrameRingBuffer,
    layout: CompositeLayout,
    frame_count: AtomicU64,
}

impl SCStreamOutputTrait for DesktopDisplayHandler {
    fn did_output_sample_buffer(
        &self,
        sample: CMSampleBuffer,
        output_type: SCStreamOutputType,
    ) {
        let Some(raw) = extract_pixels(&sample, output_type) else {
            return;
        };

        let n = self.frame_count.fetch_add(1, Ordering::Relaxed);
        if should_log_frame(n) {
            log::info!(
                "desktop: display={} frame={} {}x{}", self.display_index, n, raw.width, raw.height
            );
        }

        {
            let mut frames = self.display_frames.lock().unwrap();
            if self.display_index < frames.len() {
                frames[self.display_index] = Some(DisplayFrame {
                    pixels: raw.pixels,
                    width: raw.width,
                    height: raw.height,
                });
            }
        }

        // Try compositing all displays
        let frames = self.display_frames.lock().unwrap();
        if let Some(composite) = self.layout.composite(&frames) {
            drop(frames);
            self.composite_ring.push_frame(FrameData {
                pixels: composite,
                width: self.layout.total_width,
                height: self.layout.total_height,
                stride: self.layout.total_width * 4,
                timestamp_ns: std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .map(|d| d.as_nanos() as u64)
                    .unwrap_or(0),
                source_wid: crate::Q3IDE_DESKTOP_CAPTURE_ID,
            });
        }
    }
}

// ─── Backend ────────────────────────────────────────────────────────────────

/// macOS ScreenCaptureKit capture backend.
pub struct SCKBackend {
    window_sessions: HashMap<u32, WindowCaptureSession>,
    desktop_capture: Option<DesktopCaptureSession>,
}

impl SCKBackend {
    pub fn new() -> Result<Self> {
        Ok(Self {
            window_sessions: HashMap::new(),
            desktop_capture: None,
        })
    }
}

impl CaptureBackend for SCKBackend {
    fn list_windows(&self) -> Result<Vec<WindowInfo>> {
        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;

        Ok(content
            .windows()
            .iter()
            .map(|w| {
                let frame = w.frame();
                WindowInfo {
                    window_id: w.window_id(),
                    title: w.title().unwrap_or_default(),
                    app_name: w
                        .owning_application()
                        .map(|a| a.application_name())
                        .unwrap_or_default(),
                    width: frame.size().width as u32,
                    height: frame.size().height as u32,
                    is_on_screen: w.is_on_screen(),
                }
            })
            .collect())
    }

    fn list_displays(&self) -> Result<Vec<DisplayInfo>> {
        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;

        Ok(content
            .displays()
            .iter()
            .map(|d| {
                let frame = d.frame();
                DisplayInfo {
                    display_id: d.display_id(),
                    width: d.width(),
                    height: d.height(),
                    x: frame.origin().x as i32,
                    y: frame.origin().y as i32,
                }
            })
            .collect())
    }

    fn start_capture(&mut self, window_id: u32, _target_fps: u32) -> Result<()> {
        if self.window_sessions.contains_key(&window_id) {
            return Err(CaptureError::AlreadyCapturing(window_id));
        }

        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;

        let windows = content.windows();
        let sc_window = windows
            .iter()
            .find(|w| w.window_id() == window_id)
            .ok_or(CaptureError::WindowNotFound(window_id))?;

        let frame_rect = sc_window.frame();
        let mut w = frame_rect.size().width as u32;
        let mut h = frame_rect.size().height as u32;

        // Cap capture resolution — large windows (Retina 2x) can exceed
        // reasonable texture sizes and waste bandwidth.
        const MAX_CAPTURE_DIM: u32 = 1920;
        if w > MAX_CAPTURE_DIM || h > MAX_CAPTURE_DIM {
            let scale = MAX_CAPTURE_DIM as f64 / w.max(h) as f64;
            w = (w as f64 * scale) as u32;
            h = (h as f64 * scale) as u32;
        }

        log::info!("capture: starting wid={} {}x{} (source {}x{})",
            window_id, w, h,
            frame_rect.size().width as u32, frame_rect.size().height as u32);

        let filter = SCContentFilter::create()
            .with_window(sc_window)
            .build();

        let config = SCStreamConfiguration::new()
            .with_width(w)
            .with_height(h)
            .with_pixel_format(PixelFormat::BGRA)
            .with_shows_cursor(false)
            .with_queue_depth(3);

        let latest_frame: Arc<Mutex<Option<FrameData>>> = Arc::new(Mutex::new(None));

        let handler = WindowFrameHandler {
            latest_frame: Arc::clone(&latest_frame),
            window_id,
            frame_count: AtomicU64::new(0),
        };

        let mut stream = SCStream::new(&filter, &config);
        stream.add_output_handler(handler, SCStreamOutputType::Screen);

        stream.start_capture().map_err(|e| {
            log::error!("capture: FAILED wid={}: {}", window_id, e);
            CaptureError::Platform(format!("start_capture: {e}"))
        })?;

        log::info!("capture: started wid={}", window_id);

        self.window_sessions.insert(window_id, WindowCaptureSession {
            stream,
            latest_frame,
        });

        Ok(())
    }

    fn start_desktop_capture(&mut self, _target_fps: u32) -> Result<u32> {
        if self.desktop_capture.is_some() {
            return Err(CaptureError::AlreadyCapturing(DESKTOP_CAPTURE_ID));
        }

        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;

        let displays = content.displays();
        if displays.is_empty() {
            return Err(CaptureError::Platform("No displays found".to_string()));
        }

        let layout = CompositeLayout::from_displays(&displays);
        log::info!(
            "desktop: starting {} display(s), composite {}x{}",
            displays.len(), layout.total_width, layout.total_height
        );

        let display_frames: Arc<Mutex<Vec<Option<DisplayFrame>>>> =
            Arc::new(Mutex::new(vec![None; displays.len()]));
        let composite_ring = FrameRingBuffer::new();
        let mut streams = Vec::with_capacity(displays.len());

        for (idx, display) in displays.iter().enumerate() {
            let entry = &layout.entries[idx];
            let filter = SCContentFilter::create()
                .with_display(display)
                .with_excluding_windows(&[])
                .build();
            let config = SCStreamConfiguration::new()
                .with_width(entry.scaled_width)
                .with_height(entry.scaled_height)
                .with_pixel_format(PixelFormat::BGRA)
                .with_shows_cursor(true)
                .with_queue_depth(3);

            let handler = DesktopDisplayHandler {
                display_frames: Arc::clone(&display_frames),
                display_index: idx,
                composite_ring: composite_ring.clone(),
                layout: layout.clone(),
                frame_count: AtomicU64::new(0),
            };

            let mut stream = SCStream::new(&filter, &config);
            stream.add_output_handler(handler, SCStreamOutputType::Screen);
            stream.start_capture().map_err(|e| {
                CaptureError::Platform(format!("desktop start_capture: {e}"))
            })?;
            streams.push(stream);
        }

        self.desktop_capture = Some(DesktopCaptureSession {
            ring_buffer: composite_ring,
            streams,
            display_frames,
            layout,
        });

        Ok(DESKTOP_CAPTURE_ID)
    }

    fn start_display_capture(&mut self, display_id: u32, _target_fps: u32) -> Result<()> {
        if self.window_sessions.contains_key(&display_id) {
            return Err(CaptureError::AlreadyCapturing(display_id));
        }

        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;

        let displays = content.displays();
        let display = displays
            .iter()
            .find(|d| d.display_id() == display_id)
            .ok_or_else(|| CaptureError::Platform(format!("display {} not found", display_id)))?;

        let w = display.width();
        let h = display.height();

        log::info!("capture: starting display={} {}x{}", display_id, w, h);

        let filter = SCContentFilter::create()
            .with_display(display)
            .with_excluding_windows(&[])
            .build();

        let config = SCStreamConfiguration::new()
            .with_width(w)
            .with_height(h)
            .with_pixel_format(PixelFormat::BGRA)
            .with_shows_cursor(true)
            .with_queue_depth(3);

        let latest_frame: Arc<Mutex<Option<FrameData>>> = Arc::new(Mutex::new(None));

        let handler = WindowFrameHandler {
            latest_frame: Arc::clone(&latest_frame),
            window_id: display_id,
            frame_count: AtomicU64::new(0),
        };

        let mut stream = SCStream::new(&filter, &config);
        stream.add_output_handler(handler, SCStreamOutputType::Screen);

        stream.start_capture().map_err(|e| {
            log::error!("capture: FAILED display={}: {}", display_id, e);
            CaptureError::Platform(format!("start_capture display: {e}"))
        })?;

        log::info!("capture: started display={}", display_id);

        self.window_sessions.insert(display_id, WindowCaptureSession {
            stream,
            latest_frame,
        });

        Ok(())
    }

    fn stop_capture(&mut self, window_id: u32) {
        if window_id == DESKTOP_CAPTURE_ID {
            if let Some(session) = self.desktop_capture.take() {
                for stream in &session.streams {
                    let _ = stream.stop_capture();
                }
                log::info!("desktop: stopped");
            }
            return;
        }

        if let Some(session) = self.window_sessions.remove(&window_id) {
            let _ = session.stream.stop_capture();
            log::info!("capture: stopped wid={}", window_id);
        }
    }

    fn get_frame(&self, window_id: u32) -> Option<FrameData> {
        if window_id == DESKTOP_CAPTURE_ID {
            return self.desktop_capture
                .as_ref()
                .and_then(|s| s.ring_buffer.pop_frame());
        }

        let session = self.window_sessions.get(&window_id)?;
        session.latest_frame.lock().ok()?.clone()
    }

    fn shutdown(&mut self) {
        let window_count = self.window_sessions.len();
        let has_desktop = self.desktop_capture.is_some();

        if let Some(session) = self.desktop_capture.take() {
            for stream in &session.streams {
                let _ = stream.stop_capture();
            }
        }

        for (_, session) in self.window_sessions.drain() {
            let _ = session.stream.stop_capture();
        }

        log::info!(
            "capture: shutdown ({} windows, desktop={})", window_count, has_desktop
        );
    }
}
