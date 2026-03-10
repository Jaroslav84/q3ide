/// macOS ScreenCaptureKit backend — CaptureRouter-driven hybrid.
///
/// COMPOSITE path: one SCStream per physical display, shared by all windows on it.
///   get_frame() crops the window's rect from the latest display pixel buffer.
///   Y-flip: SCWindow.frame() is Quartz y-up; pixel buffer is y-down.
///   crop_y = (display_height_pts - (window_quartz_y_rel + window_height_pts)) * scale_y
///
/// DEDICATED path: one SCStream per window via desktopIndependentWindow filter.
///   Captures GPU-composited content correctly. Required for browsers, Electron apps.
///
/// CaptureRouter selects the path at attach time (see router.rs for whitelists).
/// Detector watches composite windows for empty/dark frames and logs warnings.

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};

/// Global pause flag — set by q3ide_pause_all_streams / q3ide_resume_all_streams.
/// When true, get_frame() returns None for all windows (no texture uploads).
/// SCStreams stay warm; handlers still fire but frames are discarded at get_frame().
static STREAMS_PAUSED: AtomicBool = AtomicBool::new(false);

pub(crate) fn set_streams_paused(paused: bool) {
    STREAMS_PAUSED.store(paused, Ordering::Relaxed);
    log::info!("streams_paused={}", paused);
}
use std::sync::{Arc, Mutex};

use screencapturekit::prelude::*;
use screencapturekit::SCFrameStatus;

use crate::backend::{CaptureBackend, CaptureError, DisplayInfo, FrameData, Result, WindowInfo};
use crate::ringbuf::FrameRingBuffer;
use crate::router::{CaptureMode, CaptureRouter, DETECTOR_DARK_THRESHOLD, DETECTOR_EMPTY_THRESHOLD};

/// Special window ID for desktop capture (all displays composited).
pub const DESKTOP_CAPTURE_ID: u32 = 0xFFFF_FFFF;

// ─── Shared frame extraction ─────────────────────────────────────────────────

struct RawPixels {
    pixels: Vec<u8>,
    width: u32,
    height: u32,
    stride: u32,
    timestamp_ns: u64,
}

fn extract_pixels(sample: &CMSampleBuffer, output_type: SCStreamOutputType) -> Option<RawPixels> {
    if output_type != SCStreamOutputType::Screen {
        return None;
    }
    match sample.frame_status() {
        Some(SCFrameStatus::Complete) | Some(SCFrameStatus::Started) | None => {}
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
    Some(RawPixels { pixels, width: width as u32, height: height as u32, stride: pixel_buffer.bytes_per_row() as u32, timestamp_ns })
}

fn should_log_frame(count: u64) -> bool {
    count < 3 || count % 300 == 0
}

// ─── DEDICATED path: per-window SCStream ─────────────────────────────────────

struct DedicatedSession {
    stream: SCStream,
    latest_frame: Arc<Mutex<Option<FrameData>>>,
}

struct DedicatedFrameHandler {
    latest_frame: Arc<Mutex<Option<FrameData>>>,
    window_id: u32,
    frame_count: AtomicU64,
}

impl SCStreamOutputTrait for DedicatedFrameHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, output_type: SCStreamOutputType) {
        let Some(raw) = extract_pixels(&sample, output_type) else { return };
        let n = self.frame_count.fetch_add(1, Ordering::Relaxed);
        if should_log_frame(n) {
            log::info!("dedicated: wid={} frame={} {}x{}", self.window_id, n, raw.width, raw.height);
        }
        if let Ok(mut slot) = self.latest_frame.lock() {
            *slot = Some(FrameData {
                pixels: raw.pixels,
                width: raw.width,
                height: raw.height,
                stride: raw.stride,
                timestamp_ns: raw.timestamp_ns,
                source_wid: self.window_id,
            });
        }
    }
}

// ─── COMPOSITE path: per-display SCStream, crop per window ───────────────────

/// Crop rect for one window on a display composite stream (computed at attach time).
struct WindowCompositeInfo {
    display_id: u32,
    crop_x: u32,
    crop_y: u32, // y-down pixel space (top of window)
    crop_w: u32,
    crop_h: u32,
}

struct CompositeSession {
    stream: SCStream,
    latest_frame: Arc<Mutex<Option<FrameData>>>,
    window_count: usize,
}

struct CompositeHandler {
    latest_frame: Arc<Mutex<Option<FrameData>>>,
    display_id: u32,
    frame_count: AtomicU64,
}

impl SCStreamOutputTrait for CompositeHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, output_type: SCStreamOutputType) {
        let Some(raw) = extract_pixels(&sample, output_type) else { return };
        let n = self.frame_count.fetch_add(1, Ordering::Relaxed);
        if should_log_frame(n) {
            log::info!("composite: display={} frame={} {}x{}", self.display_id, n, raw.width, raw.height);
        }
        if let Ok(mut slot) = self.latest_frame.lock() {
            *slot = Some(FrameData {
                pixels: raw.pixels,
                width: raw.width,
                height: raw.height,
                stride: raw.stride,
                timestamp_ns: raw.timestamp_ns,
                source_wid: self.display_id,
            });
        }
    }
}

/// Find the display with the most overlap with the window's Quartz rect.
fn find_display_for_window<'a>(displays: &'a [SCDisplay], wx: f64, wy: f64, ww: f64, wh: f64) -> Option<&'a SCDisplay> {
    let mut best: Option<(&SCDisplay, f64)> = None;
    for d in displays {
        let df = d.frame();
        let (dx, dy, dw, dh) = (df.origin().x, df.origin().y, df.size().width, df.size().height);
        let ox = (wx + ww).min(dx + dw) - wx.max(dx);
        let oy = (wy + wh).min(dy + dh) - wy.max(dy);
        let area = ox.max(0.0) * oy.max(0.0);
        if area > best.as_ref().map_or(-1.0, |b| b.1) {
            best = Some((d, area));
        }
    }
    best.map(|b| b.0)
}

/// Copy the window's crop rect out of a full-display FrameData.
fn crop_display_frame(frame: &FrameData, info: &WindowCompositeInfo, window_id: u32) -> Option<FrameData> {
    let x0 = info.crop_x.min(frame.width);
    let y0 = info.crop_y.min(frame.height);
    let x1 = (info.crop_x + info.crop_w).min(frame.width);
    let y1 = (info.crop_y + info.crop_h).min(frame.height);
    let out_w = x1.saturating_sub(x0);
    let out_h = y1.saturating_sub(y0);
    if out_w == 0 || out_h == 0 {
        return None;
    }
    let mut pixels = vec![0u8; (out_w * out_h * 4) as usize];
    for row in 0..out_h {
        let src_off = ((y0 + row) * frame.stride + x0 * 4) as usize;
        let dst_off = (row * out_w * 4) as usize;
        let len = (out_w * 4) as usize;
        if src_off + len <= frame.pixels.len() {
            pixels[dst_off..dst_off + len].copy_from_slice(&frame.pixels[src_off..src_off + len]);
        }
    }
    Some(FrameData { pixels, width: out_w, height: out_h, stride: out_w * 4, timestamp_ns: frame.timestamp_ns, source_wid: window_id })
}

// ─── Desktop composite capture ────────────────────────────────────────────────

#[allow(dead_code)]
struct DesktopSession {
    ring_buffer: FrameRingBuffer,
    streams: Vec<SCStream>,
    display_frames: Arc<Mutex<Vec<Option<DesktopDisplayFrame>>>>,
    layout: DesktopLayout,
}

#[derive(Clone)]
struct DesktopDisplayFrame {
    pixels: Vec<u8>,
    width: u32,
    height: u32,
}

#[derive(Clone)]
struct DesktopLayout {
    total_width: u32,
    total_height: u32,
    entries: Vec<DesktopLayoutEntry>,
}

#[derive(Clone)]
struct DesktopLayoutEntry {
    scaled_width: u32,
    scaled_height: u32,
    y_offset: u32,
}

impl DesktopLayout {
    fn from_displays(displays: &[SCDisplay]) -> Self {
        let total_width = displays.iter().map(|d| d.width()).max().unwrap_or(1920);
        let mut entries = Vec::with_capacity(displays.len());
        let mut y = 0u32;
        for d in displays {
            let h = (total_width as f32 * d.height() as f32 / d.width() as f32) as u32;
            entries.push(DesktopLayoutEntry { scaled_width: total_width, scaled_height: h, y_offset: y });
            y += h;
        }
        Self { total_width, total_height: y, entries }
    }

    fn composite(&self, frames: &[Option<DesktopDisplayFrame>]) -> Option<Vec<u8>> {
        if !frames.iter().all(|f| f.is_some()) {
            return None;
        }
        let row_bytes = self.total_width * 4;
        let mut out = vec![0u8; (row_bytes * self.total_height) as usize];
        for (i, frame_opt) in frames.iter().enumerate() {
            let frame = frame_opt.as_ref().expect("all frames Some — guarded by all(is_some) above");
            let entry = &self.entries[i];
            let src_row_bytes = frame.width * 4;
            for dst_y in 0..entry.scaled_height {
                let src_y = ((dst_y as f32 * frame.height as f32 / entry.scaled_height as f32) as u32).min(frame.height - 1);
                let dst_row = ((entry.y_offset + dst_y) * row_bytes) as usize;
                let src_row = (src_y * src_row_bytes) as usize;
                for dst_x in 0..entry.scaled_width {
                    let src_x = ((dst_x as f32 * frame.width as f32 / entry.scaled_width as f32) as u32).min(frame.width - 1);
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

struct DesktopDisplayHandler {
    display_frames: Arc<Mutex<Vec<Option<DesktopDisplayFrame>>>>,
    display_index: usize,
    composite_ring: FrameRingBuffer,
    layout: DesktopLayout,
    frame_count: AtomicU64,
}

impl SCStreamOutputTrait for DesktopDisplayHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, output_type: SCStreamOutputType) {
        let Some(raw) = extract_pixels(&sample, output_type) else { return };
        let n = self.frame_count.fetch_add(1, Ordering::Relaxed);
        if should_log_frame(n) {
            log::info!("desktop: display={} frame={} {}x{}", self.display_index, n, raw.width, raw.height);
        }
        {
            let mut frames = self.display_frames.lock().expect("display_frames mutex poisoned");
            if self.display_index < frames.len() {
                frames[self.display_index] = Some(DesktopDisplayFrame { pixels: raw.pixels, width: raw.width, height: raw.height });
            }
        }
        let frames = self.display_frames.lock().expect("display_frames mutex poisoned");
        if let Some(composite) = self.layout.composite(&frames) {
            drop(frames);
            self.composite_ring.push_frame(FrameData {
                pixels: composite,
                width: self.layout.total_width,
                height: self.layout.total_height,
                stride: self.layout.total_width * 4,
                timestamp_ns: std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_nanos() as u64).unwrap_or(0),
                source_wid: crate::Q3IDE_DESKTOP_CAPTURE_ID,
            });
        }
    }
}

// ─── q3ide desktop display stream ────────────────────────────────────────────

struct DisplayStream {
    stream: SCStream,
    latest_frame: Arc<Mutex<Option<FrameData>>>,
}

struct DisplayStreamHandler {
    latest_frame: Arc<Mutex<Option<FrameData>>>,
    display_id: u32,
    frame_count: AtomicU64,
}

impl SCStreamOutputTrait for DisplayStreamHandler {
    fn did_output_sample_buffer(&self, sample: CMSampleBuffer, output_type: SCStreamOutputType) {
        let Some(raw) = extract_pixels(&sample, output_type) else { return };
        let n = self.frame_count.fetch_add(1, Ordering::Relaxed);
        if should_log_frame(n) {
            log::info!("display: id={} frame={} {}x{}", self.display_id, n, raw.width, raw.height);
        }
        if let Ok(mut slot) = self.latest_frame.lock() {
            *slot = Some(FrameData {
                pixels: raw.pixels,
                width: raw.width,
                height: raw.height,
                stride: raw.stride,
                timestamp_ns: raw.timestamp_ns,
                source_wid: self.display_id,
            });
        }
    }
}

// ─── Detector state ───────────────────────────────────────────────────────────

struct DetectorState {
    /// Consecutive frames where display_frame was present but crop returned nothing.
    empty_count: u32,
    /// Consecutive dark frames (crop succeeded but pixels are GPU-black).
    dark_count: u32,
    /// App name, for log messages.
    app_name: String,
}

// ─── Backend ──────────────────────────────────────────────────────────────────

pub struct SCKBackend {
    /// COMPOSITE: per-display streams (shared by windows on that display).
    composite_sessions: HashMap<u32, CompositeSession>,
    /// COMPOSITE: maps window_id → crop info + display_id.
    window_map: HashMap<u32, WindowCompositeInfo>,
    /// COMPOSITE: detector state per window (interior mutability for get_frame).
    detector: Mutex<HashMap<u32, DetectorState>>,
    /// DEDICATED: per-window streams.
    dedicated_sessions: HashMap<u32, DedicatedSession>,
    /// q3ide desktop display streams.
    display_streams: HashMap<u32, DisplayStream>,
    desktop_capture: Option<DesktopSession>,
}

impl SCKBackend {
    pub fn new() -> Result<Self> {
        Ok(Self {
            composite_sessions: HashMap::new(),
            window_map: HashMap::new(),
            detector: Mutex::new(HashMap::new()),
            dedicated_sessions: HashMap::new(),
            display_streams: HashMap::new(),
            desktop_capture: None,
        })
    }

    fn start_composite(&mut self, window_id: u32, app_name: &str, target_fps: i32, content: &SCShareableContent) -> Result<()> {
        let windows = content.windows();
        let sc_window = windows.iter().find(|w| w.window_id() == window_id)
            .ok_or(CaptureError::WindowNotFound(window_id))?;

        // Quartz global coords (y-up).
        let wf = sc_window.frame();
        let (wx, wy, ww, wh) = (wf.origin().x, wf.origin().y, wf.size().width, wf.size().height);

        let displays = content.displays();
        let display = find_display_for_window(&displays, wx, wy, ww, wh).ok_or_else(|| {
            CaptureError::Platform(format!("wid={}: no display for rect ({:.0},{:.0} {:.0}x{:.0})", window_id, wx, wy, ww, wh))
        })?;

        let display_id = display.display_id();
        let df = display.frame();
        let (dx, dy, dpw, dph) = (df.origin().x, df.origin().y, df.size().width, df.size().height);
        let (dw, dh) = (display.width(), display.height());
        let scale_x = dw as f64 / dpw.max(1.0);
        let scale_y = dh as f64 / dph.max(1.0);

        // Window position relative to display origin (Quartz y-up points).
        let rel_x = wx - dx;
        let rel_y = wy - dy; // distance from display bottom to window bottom

        // Convert to pixel y-down: distance from display top to window top.
        let crop_x = (rel_x * scale_x).max(0.0) as u32;
        let crop_y = ((dph - (rel_y + wh)) * scale_y).max(0.0) as u32;
        let crop_w = (ww * scale_x) as u32;
        let crop_h = (wh * scale_y) as u32;

        log::info!(
            "composite: wid={} app='{}' win=({:.0},{:.0} {:.0}x{:.0}pt) \
             disp=({:.0},{:.0} {:.0}x{:.0}pt={}x{}px scale={:.2}x{:.2}) \
             crop=({},{} {}x{}px)",
            window_id, app_name, wx, wy, ww, wh,
            dx, dy, dpw, dph, dw, dh, scale_x, scale_y,
            crop_x, crop_y, crop_w, crop_h
        );

        self.window_map.insert(window_id, WindowCompositeInfo { display_id, crop_x, crop_y, crop_w, crop_h });

        // Init detector state for this window.
        if let Ok(mut det) = self.detector.lock() {
            det.insert(window_id, DetectorState { empty_count: 0, dark_count: 0, app_name: app_name.to_string() });
        }

        // Start display stream if first window on this display.
        if !self.composite_sessions.contains_key(&display_id) {
            let all_windows = content.windows();
            let quake_wins: Vec<&SCWindow> = all_windows.iter()
                .filter(|w| w.owning_application().map(|a| {
                    let n = a.application_name().to_lowercase();
                    n.contains("quake3") || n.contains("quake 3")
                }).unwrap_or(false))
                .collect();

            let filter = SCContentFilter::create()
                .with_display(display)
                .with_excluding_windows(&quake_wins)
                .build();
            let mut config = SCStreamConfiguration::new()
                .with_width(dw).with_height(dh)
                .with_pixel_format(PixelFormat::BGRA)
                .with_shows_cursor(false)
                .with_queue_depth(3);
            if target_fps >= 0 {
                config = config.with_fps(target_fps as u32);
            }

            let latest_frame: Arc<Mutex<Option<FrameData>>> = Arc::new(Mutex::new(None));
            let handler = CompositeHandler { latest_frame: Arc::clone(&latest_frame), display_id, frame_count: AtomicU64::new(0) };
            let mut stream = SCStream::new(&filter, &config);
            stream.add_output_handler(handler, SCStreamOutputType::Screen);
            stream.start_capture().map_err(|e| {
                log::error!("composite: FAILED display={}: {}", display_id, e);
                CaptureError::Platform(format!("composite start display={}: {e}", display_id))
            })?;
            log::info!("composite: started display={} {}x{}px", display_id, dw, dh);
            self.composite_sessions.insert(display_id, CompositeSession { stream, latest_frame, window_count: 0 });
        }

        self.composite_sessions.get_mut(&display_id).expect("session inserted above").window_count += 1;
        log::info!("composite: display={} now has {} window(s)", display_id,
            self.composite_sessions[&display_id].window_count);
        Ok(())
    }

    fn start_dedicated(&mut self, window_id: u32, app_name: &str, target_fps: i32, content: &SCShareableContent) -> Result<()> {
        let windows = content.windows();
        let sc_window = windows.iter().find(|w| w.window_id() == window_id)
            .ok_or(CaptureError::WindowNotFound(window_id))?;

        let wf = sc_window.frame();
        let (wx, wy, ww, wh) = (wf.origin().x, wf.origin().y, wf.size().width, wf.size().height);

        // SCStreamConfiguration expects pixel dimensions, not points.
        // Derive scale from the display this window is on (same as COMPOSITE path).
        let displays = content.displays();
        let scale = find_display_for_window(&displays, wx, wy, ww, wh)
            .map(|d| {
                let df = d.frame();
                let dpw = df.size().width.max(1.0);
                d.width() as f64 / dpw
            })
            .unwrap_or(2.0); // safe fallback: assume Retina 2x

        let pw = ((ww * scale) as u32).max(1);
        let ph = ((wh * scale) as u32).max(1);

        log::info!("dedicated: wid={} app='{}' {:.0}x{:.0}pt → {}x{}px (scale={:.2})",
            window_id, app_name, ww, wh, pw, ph, scale);

        let filter = SCContentFilter::create().with_window(sc_window).build();
        let mut config = SCStreamConfiguration::new()
            .with_width(pw).with_height(ph)
            .with_pixel_format(PixelFormat::BGRA)
            .with_shows_cursor(false)
            .with_queue_depth(3);
        if target_fps >= 0 {
            config = config.with_fps(target_fps as u32);
        }

        let latest_frame: Arc<Mutex<Option<FrameData>>> = Arc::new(Mutex::new(None));
        let handler = DedicatedFrameHandler { latest_frame: Arc::clone(&latest_frame), window_id, frame_count: AtomicU64::new(0) };
        let mut stream = SCStream::new(&filter, &config);
        stream.add_output_handler(handler, SCStreamOutputType::Screen);
        stream.start_capture().map_err(|e| {
            log::error!("dedicated: FAILED wid={}: {}", window_id, e);
            CaptureError::Platform(format!("dedicated start wid={}: {e}", window_id))
        })?;
        log::info!("dedicated: started wid={}", window_id);
        self.dedicated_sessions.insert(window_id, DedicatedSession { stream, latest_frame });
        Ok(())
    }
}

impl CaptureBackend for SCKBackend {
    fn list_windows(&self) -> Result<Vec<WindowInfo>> {
        if !crate::has_screen_recording_permission() {
            return Err(CaptureError::PermissionDenied);
        }
        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;
        Ok(content.windows().iter().map(|w| {
            let f = w.frame();
            WindowInfo {
                window_id: w.window_id(),
                title: w.title().unwrap_or_default(),
                app_name: w.owning_application().map(|a| a.application_name()).unwrap_or_default(),
                width: f.size().width as u32,
                height: f.size().height as u32,
                is_on_screen: w.is_on_screen(),
                x: f.origin().x as i32,
                y: f.origin().y as i32,
            }
        }).collect())
    }

    fn list_displays(&self) -> Result<Vec<DisplayInfo>> {
        if !crate::has_screen_recording_permission() {
            return Err(CaptureError::PermissionDenied);
        }
        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;
        Ok(content.displays().iter().map(|d| {
            let f = d.frame();
            DisplayInfo { display_id: d.display_id(), width: d.width(), height: d.height(), x: f.origin().x as i32, y: f.origin().y as i32 }
        }).collect())
    }

    fn start_capture(&mut self, window_id: u32, target_fps: i32) -> Result<()> {
        if self.window_map.contains_key(&window_id) || self.dedicated_sessions.contains_key(&window_id) {
            return Err(CaptureError::AlreadyCapturing(window_id));
        }
        if !crate::has_screen_recording_permission() {
            return Err(CaptureError::PermissionDenied);
        }
        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;

        let windows = content.windows();
        let sc_window = windows.iter().find(|w| w.window_id() == window_id)
            .ok_or(CaptureError::WindowNotFound(window_id))?;

        let app_name = sc_window.owning_application()
            .map(|a| a.application_name())
            .unwrap_or_default();

        // Minimized windows sit in the Dock — their frame() is the Dock icon rect
        // (~64x64pt). Composite would crop that tiny region and show whatever
        // else is drawn there. DEDICATED captures the window's own GPU buffer
        // regardless of minimized/hidden state.
        let wf = sc_window.frame();
        let (ww, wh) = (wf.size().width, wf.size().height);
        let is_minimized = !sc_window.is_on_screen() && (ww < 200.0 || wh < 200.0);
        if is_minimized {
            log::info!(
                "router: wid={} app='{}' minimized ({:.0}x{:.0}pt, on_screen=false) → DEDICATED",
                window_id, app_name, ww, wh
            );
            return self.start_dedicated(window_id, &app_name, target_fps, &content);
        }

        match CaptureRouter::resolve(&app_name) {
            CaptureMode::Composite => self.start_composite(window_id, &app_name, target_fps, &content),
            CaptureMode::Dedicated => self.start_dedicated(window_id, &app_name, target_fps, &content),
        }
    }

    fn start_desktop_capture(&mut self, target_fps: i32) -> Result<u32> {
        if self.desktop_capture.is_some() {
            return Err(CaptureError::AlreadyCapturing(DESKTOP_CAPTURE_ID));
        }
        if !crate::has_screen_recording_permission() {
            return Err(CaptureError::PermissionDenied);
        }
        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;
        let displays = content.displays();
        if displays.is_empty() {
            return Err(CaptureError::Platform("no displays found".to_string()));
        }
        let layout = DesktopLayout::from_displays(&displays);
        log::info!("desktop: {} display(s) composite {}x{}", displays.len(), layout.total_width, layout.total_height);

        let display_frames: Arc<Mutex<Vec<Option<DesktopDisplayFrame>>>> = Arc::new(Mutex::new(vec![None; displays.len()]));
        let composite_ring = FrameRingBuffer::new();
        let mut streams = Vec::with_capacity(displays.len());

        for (idx, display) in displays.iter().enumerate() {
            let entry = &layout.entries[idx];
            let filter = SCContentFilter::create().with_display(display).with_excluding_windows(&[]).build();
            let mut config = SCStreamConfiguration::new()
                .with_width(entry.scaled_width).with_height(entry.scaled_height)
                .with_pixel_format(PixelFormat::BGRA)
                .with_shows_cursor(true)
                .with_queue_depth(3);
            if target_fps >= 0 {
                config = config.with_fps(target_fps as u32);
            }
            let handler = DesktopDisplayHandler {
                display_frames: Arc::clone(&display_frames),
                display_index: idx,
                composite_ring: composite_ring.clone(),
                layout: layout.clone(),
                frame_count: AtomicU64::new(0),
            };
            let mut stream = SCStream::new(&filter, &config);
            stream.add_output_handler(handler, SCStreamOutputType::Screen);
            stream.start_capture().map_err(|e| CaptureError::Platform(format!("desktop start: {e}")))?;
            streams.push(stream);
        }
        self.desktop_capture = Some(DesktopSession { ring_buffer: composite_ring, streams, display_frames, layout });
        Ok(DESKTOP_CAPTURE_ID)
    }

    fn start_display_capture(&mut self, display_id: u32, target_fps: i32) -> Result<()> {
        if self.display_streams.contains_key(&display_id) {
            return Err(CaptureError::AlreadyCapturing(display_id));
        }
        if !crate::has_screen_recording_permission() {
            return Err(CaptureError::PermissionDenied);
        }
        let content = SCShareableContent::get()
            .map_err(|e| CaptureError::Platform(format!("SCShareableContent: {e}")))?;
        let displays = content.displays();
        let display = displays.iter().find(|d| d.display_id() == display_id)
            .ok_or_else(|| CaptureError::Platform(format!("display {} not found", display_id)))?;
        let (w, h) = (display.width(), display.height());

        let all_windows = content.windows();
        let quake_wins: Vec<&SCWindow> = all_windows.iter()
            .filter(|win| win.owning_application().map(|a| {
                let n = a.application_name().to_lowercase();
                n.contains("quake3") || n.contains("quake 3")
            }).unwrap_or(false))
            .collect();

        let filter = SCContentFilter::create().with_display(display).with_excluding_windows(&quake_wins).build();
        let mut config = SCStreamConfiguration::new()
            .with_width(w).with_height(h)
            .with_pixel_format(PixelFormat::BGRA)
            .with_shows_cursor(true)
            .with_queue_depth(3);
        if target_fps >= 0 {
            config = config.with_fps(target_fps as u32);
        }

        let latest_frame: Arc<Mutex<Option<FrameData>>> = Arc::new(Mutex::new(None));
        let handler = DisplayStreamHandler { latest_frame: Arc::clone(&latest_frame), display_id, frame_count: AtomicU64::new(0) };
        let mut stream = SCStream::new(&filter, &config);
        stream.add_output_handler(handler, SCStreamOutputType::Screen);
        stream.start_capture().map_err(|e| {
            log::error!("display: FAILED id={}: {}", display_id, e);
            CaptureError::Platform(format!("start_display_capture id={}: {e}", display_id))
        })?;
        log::info!("display: started id={} {}x{}px", display_id, w, h);
        self.display_streams.insert(display_id, DisplayStream { stream, latest_frame });
        Ok(())
    }

    fn stop_capture(&mut self, window_id: u32) {
        if window_id == DESKTOP_CAPTURE_ID {
            if let Some(s) = self.desktop_capture.take() {
                for stream in &s.streams { let _ = stream.stop_capture(); }
                log::info!("desktop: stopped");
            }
            return;
        }

        // COMPOSITE window
        if let Some(info) = self.window_map.remove(&window_id) {
            if let Ok(mut det) = self.detector.lock() { det.remove(&window_id); }
            let stop_display = if let Some(s) = self.composite_sessions.get_mut(&info.display_id) {
                s.window_count = s.window_count.saturating_sub(1);
                s.window_count == 0
            } else { false };
            if stop_display {
                if let Some(s) = self.composite_sessions.remove(&info.display_id) {
                    let _ = s.stream.stop_capture();
                    log::info!("composite: stopped display={}", info.display_id);
                }
            } else {
                log::info!("composite: removed wid={} (display={} has {} left)", window_id, info.display_id,
                    self.composite_sessions.get(&info.display_id).map_or(0, |s| s.window_count));
            }
            return;
        }

        // DEDICATED window
        if let Some(s) = self.dedicated_sessions.remove(&window_id) {
            let _ = s.stream.stop_capture();
            log::info!("dedicated: stopped wid={}", window_id);
            return;
        }

        // q3ide desktop display stream
        if let Some(s) = self.display_streams.remove(&window_id) {
            let _ = s.stream.stop_capture();
            log::info!("display: stopped id={}", window_id);
        }
    }

    fn get_frame(&self, window_id: u32) -> Option<FrameData> {
        if STREAMS_PAUSED.load(Ordering::Relaxed) {
            return None;
        }
        if window_id == DESKTOP_CAPTURE_ID {
            return self.desktop_capture.as_ref().and_then(|s| s.ring_buffer.pop_frame());
        }

        // COMPOSITE path
        if let Some(info) = self.window_map.get(&window_id) {
            let session = self.composite_sessions.get(&info.display_id)?;
            let guard = session.latest_frame.lock().ok()?;

            // Display frame not arrived yet — don't charge detector.
            let display_frame = guard.as_ref()?;

            let result = crop_display_frame(display_frame, info, window_id);

            // Detector: track empty crop and dark frames.
            if let Ok(mut det) = self.detector.lock() {
                if let Some(state) = det.get_mut(&window_id) {
                    match &result {
                        None => {
                            state.empty_count += 1;
                            state.dark_count = 0;
                            if state.empty_count == DETECTOR_EMPTY_THRESHOLD {
                                log::warn!(
                                    "router: wid={} app='{}' — {} consecutive empty crops on COMPOSITE. \
                                     Window may have moved. Consider adding to WHITELIST_DEDICATED.",
                                    window_id, state.app_name, state.empty_count
                                );
                            }
                        }
                        Some(frame) => {
                            state.empty_count = 0;
                            if CaptureRouter::is_dark_frame(&frame.pixels, frame.width, frame.height, frame.stride) {
                                state.dark_count += 1;
                                if state.dark_count == DETECTOR_DARK_THRESHOLD {
                                    log::warn!(
                                        "router: wid={} app='{}' — {} consecutive dark frames on COMPOSITE. \
                                         Likely GPU-accelerated content. Add to WHITELIST_DEDICATED.",
                                        window_id, state.app_name, state.dark_count
                                    );
                                }
                            } else {
                                state.dark_count = 0;
                            }
                        }
                    }
                }
            }

            return result;
        }

        // DEDICATED path
        if let Some(s) = self.dedicated_sessions.get(&window_id) {
            return s.latest_frame.lock().ok()?.clone();
        }

        // q3ide desktop display stream
        let s = self.display_streams.get(&window_id)?;
        s.latest_frame.lock().ok()?.clone()
    }

    fn update_window_crop(&mut self, window_id: u32) {
        // Only composite windows have a crop rect to update.
        let info = match self.window_map.get(&window_id) {
            Some(i) => i,
            None => return, // dedicated or unknown — no-op
        };
        let display_id = info.display_id;

        let content = match SCShareableContent::get() {
            Ok(c) => c,
            Err(e) => {
                log::warn!("update_window_crop: SCShareableContent failed: {}", e);
                return;
            }
        };

        let windows = content.windows();
        let sc_window = match windows.iter().find(|w| w.window_id() == window_id) {
            Some(w) => w,
            None => {
                log::warn!("update_window_crop: wid={} not found in SCK", window_id);
                return;
            }
        };

        let displays = content.displays();
        let wf = sc_window.frame();
        let (wx, wy, ww, wh) = (wf.origin().x, wf.origin().y, wf.size().width, wf.size().height);

        let display = match displays.iter().find(|d| d.display_id() == display_id) {
            Some(d) => d,
            None => {
                log::warn!("update_window_crop: display_id={} not found", display_id);
                return;
            }
        };

        let df = display.frame();
        let (dx, dy, dpw, dph) = (df.origin().x, df.origin().y, df.size().width, df.size().height);
        let (dw, dh) = (display.width(), display.height());
        let scale_x = dw as f64 / dpw.max(1.0);
        let scale_y = dh as f64 / dph.max(1.0);

        let rel_x = wx - dx;
        let rel_y = wy - dy;
        let crop_x = (rel_x * scale_x).max(0.0) as u32;
        let crop_y = ((dph - (rel_y + wh)) * scale_y).max(0.0) as u32;
        let crop_w = (ww * scale_x) as u32;
        let crop_h = (wh * scale_y) as u32;

        log::info!(
            "update_crop: wid={} new crop=({},{} {}x{}px)",
            window_id, crop_x, crop_y, crop_w, crop_h
        );

        if let Some(entry) = self.window_map.get_mut(&window_id) {
            entry.crop_x = crop_x;
            entry.crop_y = crop_y;
            entry.crop_w = crop_w;
            entry.crop_h = crop_h;
        }
    }

    fn shutdown(&mut self) {
        self.window_map.clear();
        if let Ok(mut det) = self.detector.lock() { det.clear(); }
        for (did, s) in self.composite_sessions.drain() {
            let _ = s.stream.stop_capture();
            log::info!("composite: shutdown display={}", did);
        }
        for (wid, s) in self.dedicated_sessions.drain() {
            let _ = s.stream.stop_capture();
            log::info!("dedicated: shutdown wid={}", wid);
        }
        for (did, s) in self.display_streams.drain() {
            let _ = s.stream.stop_capture();
            log::info!("display: shutdown id={}", did);
        }
        if let Some(s) = self.desktop_capture.take() {
            for stream in &s.streams { let _ = stream.stop_capture(); }
        }
        log::info!("capture: shutdown complete");
    }
}
