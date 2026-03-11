/// Capture backend trait — platform-agnostic interface for window capture.
///
/// macOS: ScreenCaptureKit implementation
/// Future: PipeWire (Linux), DXGI Desktop Duplication (Windows)

/// Information about a capturable window.
#[derive(Debug, Clone)]
pub struct WindowInfo {
    pub window_id: u32,
    pub title: String,
    pub app_name: String,
    pub width: u32,
    pub height: u32,
    pub is_on_screen: bool,
    /// Quartz global X coordinate (y-up, points). Used for composite crop change detection.
    pub x: i32,
    /// Quartz global Y coordinate (y-up, points). Used for composite crop change detection.
    pub y: i32,
}

/// Information about a capturable display.
#[derive(Debug, Clone)]
pub struct DisplayInfo {
    pub display_id: u32,
    pub width: u32,
    pub height: u32,
    pub x: i32,
    pub y: i32,
}

/// A single captured frame.
#[derive(Clone)]
pub struct FrameData {
    /// BGRA8 pixel data
    pub pixels: Vec<u8>,
    pub width: u32,
    pub height: u32,
    /// Bytes per row (may include padding)
    pub stride: u32,
    /// Monotonic timestamp in nanoseconds
    pub timestamp_ns: u64,
    /// Source window ID (for validation — proves which capture produced this frame)
    pub source_wid: u32,
}

/// Error type for capture operations.
#[derive(Debug)]
#[allow(dead_code)]
pub enum CaptureError {
    /// Platform API not available
    NotAvailable(String),
    /// Window not found
    WindowNotFound(u32),
    /// Permission denied (Screen Recording permission)
    PermissionDenied,
    /// Capture already in progress for this window
    AlreadyCapturing(u32),
    /// Internal platform error
    Platform(String),
}

impl std::fmt::Display for CaptureError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CaptureError::NotAvailable(msg) => write!(f, "capture not available: {msg}"),
            CaptureError::WindowNotFound(id) => write!(f, "window {id} not found"),
            CaptureError::PermissionDenied => write!(f, "screen recording permission denied"),
            CaptureError::AlreadyCapturing(id) => write!(f, "already capturing window {id}"),
            CaptureError::Platform(msg) => write!(f, "platform error: {msg}"),
        }
    }
}

impl std::error::Error for CaptureError {}

pub type Result<T> = std::result::Result<T, CaptureError>;

/// Platform-agnostic capture backend interface.
pub trait CaptureBackend: Send {
    /// Enumerate all available windows.
    fn list_windows(&self) -> Result<Vec<WindowInfo>>;

    /// Enumerate all available displays.
    fn list_displays(&self) -> Result<Vec<DisplayInfo>>;

    /// Start capturing a specific window.
    /// target_fps: -1 = Apple decides (no cap), 0 = static image, N = max fps cap.
    fn start_capture(&mut self, window_id: u32, target_fps: i32) -> Result<()>;

    /// Start capturing all displays, composited vertically into a single frame.
    /// target_fps: -1 = Apple decides (no cap), 0 = static image, N = max fps cap.
    fn start_desktop_capture(&mut self, _target_fps: i32) -> Result<u32>;

    /// Start capturing a single display by its CGDirectDisplayID.
    /// target_fps: -1 = Apple decides (no cap), 0 = static image, N = max fps cap.
    fn start_display_capture(&mut self, display_id: u32, target_fps: i32) -> Result<()>;

    /// Stop capturing a specific window.
    fn stop_capture(&mut self, window_id: u32);

    /// Get the latest captured frame for a window.
    /// Returns None if no frame is available yet.
    fn get_frame(&self, window_id: u32) -> Option<FrameData>;

    /// Update the composite crop rect for a window whose screen position changed.
    /// Position (wx, wy, ww, wh) in Quartz y-up points — already fetched by poll.
    /// No-op for DEDICATED windows (they follow the window automatically).
    fn update_composite_crop(&mut self, window_id: u32, wx: f64, wy: f64, ww: f64, wh: f64);

    /// Shut down the backend and release all resources.
    fn shutdown(&mut self);
}
