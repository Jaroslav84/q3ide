//! q3ide-capture — Screen capture dylib for q3ide.
//!
//! C-ABI interface for the Quake3e engine to:
//! - Enumerate macOS windows and displays
//! - Start/stop per-window capture via ScreenCaptureKit
//! - Poll latest captured frames (one per window, no cross-invalidation)
//!
//! All state is held in an opaque handle (`Q3ideCapture`).

mod backend;
mod ringbuf;
mod screencapturekit;
mod window;

extern "C" {
    fn sc_check_screen_recording_permission() -> bool;
}

/// Safe wrapper — callable from any module without repeating the unsafe block.
pub(crate) fn has_screen_recording_permission() -> bool {
    // SAFETY: Swift @_cdecl function with no invariants, always safe to call.
    unsafe { sc_check_screen_recording_permission() }
}

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_float, c_int, c_uint};
use std::ptr;

use crate::backend::CaptureBackend;
use crate::screencapturekit::SCKBackend;

/// Opaque capture context handle passed across FFI.
pub struct Q3ideCapture {
    backend: Box<dyn CaptureBackend>,
    /// Window snapshot for change detection. None = not yet initialized (first poll call sets baseline).
    known_windows: std::sync::Mutex<Option<HashMap<u32, backend::WindowInfo>>>,
}

/// Frame data returned to the engine via C-ABI.
#[repr(C)]
pub struct Q3ideFrame {
    /// Pointer to BGRA8 pixel data. Valid until next `q3ide_get_frame` call for this window.
    pub pixels: *const u8,
    /// Width in pixels.
    pub width: c_uint,
    /// Height in pixels.
    pub height: c_uint,
    /// Bytes per row (stride), may include padding.
    pub stride: c_uint,
    /// Frame timestamp in nanoseconds (monotonic).
    pub timestamp_ns: u64,
    /// Source window ID — the capture session that produced this frame.
    /// Engine MUST verify this matches the requested window_id.
    pub source_wid: c_uint,
}

/// Window info returned to the engine for `/q3ide_list`.
#[repr(C)]
pub struct Q3ideWindowInfo {
    pub window_id: c_uint,
    /// Null-terminated UTF-8 title string. Owned by the library.
    pub title: *const c_char,
    /// Null-terminated UTF-8 app name string. Owned by the library.
    pub app_name: *const c_char,
    pub width: c_uint,
    pub height: c_uint,
    pub is_on_screen: c_int,
}

/// Window list returned by `q3ide_list_windows`.
#[repr(C)]
pub struct Q3ideWindowList {
    pub windows: *const Q3ideWindowInfo,
    pub count: c_uint,
}

/// Error codes returned by C-ABI functions.
#[repr(C)]
pub enum Q3ideError {
    Ok = 0,
    NotAvailable = 1,
    WindowNotFound = 2,
    PermissionDenied = 3,
    AlreadyCapturing = 4,
    PlatformError = 5,
    NullPointer = 6,
}

// ─── Per-window frame storage ─────────────────────────────────────────────────
// Each window's frame data must outlive the q3ide_get_frame call (the engine
// reads the pixel pointer). We store one frame PER WINDOW to prevent
// cross-contamination — calling get_frame(B) must NOT invalidate A's pixels.
use std::cell::RefCell;
use std::collections::HashMap;
thread_local! {
    static LAST_FRAMES: RefCell<HashMap<c_uint, backend::FrameData>> = RefCell::new(HashMap::new());
}

// ─── C-ABI exports ──────────────────────────────────────────────────────────

/// Initialize the capture system. Returns an opaque handle.
/// Returns NULL on failure (check stderr/logs for details).
///
/// # Safety
/// Caller must eventually call `q3ide_shutdown` to free resources.
#[no_mangle]
pub extern "C" fn q3ide_init() -> *mut Q3ideCapture {
    // Set up logging — write to logs/q3ide_capture.log and stderr
    use std::io::Write;
    let _ = env_logger::Builder::from_env(
        env_logger::Env::default().default_filter_or("info")
    )
    .format(|buf, record| {
        writeln!(
            buf,
            "[{}] [{}] [{}] {}",
            chrono::Local::now().format("%Y-%m-%d %H:%M:%S"),
            record.level(),
            record.target(),
            record.args()
        )
    })
    .try_init();

    // Also write to a log file
    if let Ok(cwd) = std::env::current_dir() {
        let log_dir = cwd.join("logs");
        let _ = std::fs::create_dir_all(&log_dir);
        let log_path = log_dir.join("q3ide_capture.log");
        if let Ok(file) = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(&log_path)
        {
            // Redirect stderr to the log file so env_logger output goes there
            use std::os::unix::io::IntoRawFd;
            unsafe {
                libc::dup2(file.into_raw_fd(), 2);
            }
            log::info!("Capture logging to {:?}", log_path);
        }
    }

    log::info!("q3ide_init: starting capture system");

    // Check screen recording permission before any SCK calls.
    // Without this, the first SCShareableContent::get() call triggers a TCC dialog.
    // On fullscreen apps the dialog is hidden; macOS aborts the process after ~15s.
    if !has_screen_recording_permission() {
        log::warn!(
            "q3ide: SCREEN RECORDING PERMISSION NOT GRANTED.\n  \
             Go to: System Settings → Privacy & Security → Screen Recording\n  \
             Enable the Quake3e entry, then relaunch the game."
        );
    }

    let backend = match SCKBackend::new() {
        Ok(b) => b,
        Err(e) => {
            log::error!("q3ide_init failed: {e}");
            return ptr::null_mut();
        }
    };

    let ctx = Box::new(Q3ideCapture {
        backend: Box::new(backend),
        known_windows: std::sync::Mutex::new(None),
    });

    Box::into_raw(ctx)
}

/// List available windows for capture.
///
/// Returns a `Q3ideWindowList`. The caller must free it with `q3ide_free_window_list`.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_list_windows(handle: *mut Q3ideCapture) -> Q3ideWindowList {
    let empty = Q3ideWindowList {
        windows: ptr::null(),
        count: 0,
    };

    if handle.is_null() {
        return empty;
    }

    let ctx = &*handle;
    let windows = match ctx.backend.list_windows() {
        Ok(w) => w,
        Err(e) => {
            log::error!("q3ide_list_windows failed: {e}");
            return empty;
        }
    };

    if windows.is_empty() {
        return empty;
    }

    let mut c_windows: Vec<Q3ideWindowInfo> = Vec::with_capacity(windows.len());
    // We need to keep CStrings alive — leak them intentionally.
    // They get freed in q3ide_free_window_list.
    for w in &windows {
        let title = CString::new(w.title.as_str()).unwrap_or_default();
        let app_name = CString::new(w.app_name.as_str()).unwrap_or_default();

        c_windows.push(Q3ideWindowInfo {
            window_id: w.window_id,
            title: title.into_raw(),
            app_name: app_name.into_raw(),
            width: w.width,
            height: w.height,
            is_on_screen: w.is_on_screen as c_int,
        });
    }

    let count = c_windows.len() as c_uint;
    let ptr = c_windows.as_ptr();
    std::mem::forget(c_windows);

    Q3ideWindowList {
        windows: ptr,
        count,
    }
}

/// Free a window list returned by `q3ide_list_windows`.
///
/// # Safety
/// `list` must have been returned by `q3ide_list_windows`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_free_window_list(list: Q3ideWindowList) {
    if list.windows.is_null() || list.count == 0 {
        return;
    }

    let windows = Vec::from_raw_parts(
        list.windows as *mut Q3ideWindowInfo,
        list.count as usize,
        list.count as usize,
    );

    for w in &windows {
        if !w.title.is_null() {
            let _ = CString::from_raw(w.title as *mut c_char);
        }
        if !w.app_name.is_null() {
            let _ = CString::from_raw(w.app_name as *mut c_char);
        }
    }
    // Vec drops here, freeing the window info array
}

/// Start capturing a window by its ID.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_start_capture(
    handle: *mut Q3ideCapture,
    window_id: c_uint,
    target_fps: c_uint,
) -> Q3ideError {
    if handle.is_null() {
        return Q3ideError::NullPointer;
    }

    let ctx = &mut *handle;
    match ctx.backend.start_capture(window_id, target_fps) {
        Ok(()) => Q3ideError::Ok,
        Err(e) => {
            log::error!("q3ide_start_capture failed: {e}");
            match e {
                backend::CaptureError::WindowNotFound(_) => Q3ideError::WindowNotFound,
                backend::CaptureError::PermissionDenied => Q3ideError::PermissionDenied,
                backend::CaptureError::AlreadyCapturing(_) => Q3ideError::AlreadyCapturing,
                backend::CaptureError::NotAvailable(_) => Q3ideError::NotAvailable,
                backend::CaptureError::Platform(_) => Q3ideError::PlatformError,
            }
        }
    }
}

/// Stop capturing a window.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_stop_capture(handle: *mut Q3ideCapture, window_id: c_uint) {
    if handle.is_null() {
        return;
    }
    let ctx = &mut *handle;
    ctx.backend.stop_capture(window_id);
}

/// Get the latest captured frame for a window.
///
/// Returns a `Q3ideFrame`. The pixel data pointer is valid until the next
/// call to `q3ide_get_frame` **for the same window_id**. Frames from
/// different windows do not invalidate each other.
///
/// Returns a frame with `pixels = NULL` if no frame is available.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_get_frame(
    handle: *mut Q3ideCapture,
    window_id: c_uint,
) -> Q3ideFrame {
    let empty = Q3ideFrame {
        pixels: ptr::null(),
        width: 0,
        height: 0,
        stride: 0,
        timestamp_ns: 0,
        source_wid: 0,
    };

    if handle.is_null() {
        return empty;
    }

    let ctx = &*handle;
    match ctx.backend.get_frame(window_id) {
        Some(frame) => {
            let result = Q3ideFrame {
                pixels: frame.pixels.as_ptr(),
                width: frame.width,
                height: frame.height,
                stride: frame.stride,
                timestamp_ns: frame.timestamp_ns,
                source_wid: frame.source_wid,
            };
            // Store frame per-window so each window's pixels stay alive
            // independently — getting frame B does NOT invalidate frame A's pointer.
            LAST_FRAMES.with(|cell| {
                cell.borrow_mut().insert(window_id, frame);
            });
            result
        }
        None => empty,
    }
}

/// Shut down the capture system and free all resources.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
/// After calling this, `handle` is invalid and must not be used.
#[no_mangle]
pub unsafe extern "C" fn q3ide_shutdown(handle: *mut Q3ideCapture) {
    if handle.is_null() {
        return;
    }
    let mut ctx = Box::from_raw(handle);
    ctx.backend.shutdown();
    // ctx drops here, freeing all resources
}

/// Get the formatted window list as a string for console display.
/// Returns a null-terminated string. Caller must free with `q3ide_free_string`.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_list_windows_formatted(
    handle: *mut Q3ideCapture,
) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }

    let ctx = &*handle;
    let windows = match ctx.backend.list_windows() {
        Ok(w) => w,
        Err(e) => {
            log::error!("q3ide_list_windows_formatted failed: {e}");
            return ptr::null_mut();
        }
    };

    let formatted = window::format_window_list(&windows);
    match CString::new(formatted) {
        Ok(s) => s.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Free a string returned by `q3ide_list_windows_formatted`.
///
/// # Safety
/// `s` must have been returned by a q3ide function that allocates strings.
#[no_mangle]
pub unsafe extern "C" fn q3ide_free_string(s: *mut c_char) {
    if !s.is_null() {
        let _ = CString::from_raw(s);
    }
}

/// A single window change event.
#[repr(C)]
pub struct Q3ideWindowChange {
    pub window_id: c_uint,
    /// Added/resized: heap-allocated null-terminated app name, freed by q3ide_free_change_list.
    /// Removed: null.
    pub app_name: *mut c_char,
    pub width: c_uint,
    pub height: c_uint,
    /// 1 = added, 0 = removed, 2 = resized.
    pub is_added: c_int,
}

/// List of window changes since the last `q3ide_poll_window_changes` call.
#[repr(C)]
pub struct Q3ideWindowChangeList {
    pub changes: *mut Q3ideWindowChange,
    pub count: c_uint,
}

/// Poll for window open/close events since the last call.
///
/// First call always returns an empty list and establishes the baseline snapshot.
/// Subsequent calls return diffs. Caller must free the list with `q3ide_free_change_list`.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_poll_window_changes(
    handle: *mut Q3ideCapture,
) -> Q3ideWindowChangeList {
    let empty = Q3ideWindowChangeList { changes: ptr::null_mut(), count: 0 };
    if handle.is_null() {
        return empty;
    }
    let ctx = &mut *handle;

    let current = match ctx.backend.list_windows() {
        Ok(w) => w,
        Err(e) => {
            log::warn!("q3ide_poll_window_changes: list_windows failed: {e}");
            return empty;
        }
    };

    let mut known_lock = ctx.known_windows.lock().unwrap();

    // First call: establish baseline, return no changes.
    if known_lock.is_none() {
        let map: HashMap<u32, backend::WindowInfo> =
            current.into_iter().map(|w| (w.window_id, w)).collect();
        *known_lock = Some(map);
        return empty;
    }

    let known = known_lock.as_ref().unwrap();
    let mut changes: Vec<Q3ideWindowChange> = Vec::new();

    // Removed: in known but not in current.
    let current_ids: std::collections::HashSet<u32> =
        current.iter().map(|w| w.window_id).collect();
    for id in known.keys() {
        if !current_ids.contains(id) {
            changes.push(Q3ideWindowChange {
                window_id: *id,
                app_name: ptr::null_mut(),
                width: 0,
                height: 0,
                is_added: 0,
            });
        }
    }

    // Added: in current but not in known.
    for w in &current {
        if !known.contains_key(&w.window_id) {
            let app_name = CString::new(w.app_name.as_str()).unwrap_or_default().into_raw();
            changes.push(Q3ideWindowChange {
                window_id: w.window_id,
                app_name,
                width: w.width,
                height: w.height,
                is_added: 1,
            });
        }
    }

    // Resized: in both known and current, but dimensions changed.
    for w in &current {
        if let Some(prev) = known.get(&w.window_id) {
            if prev.width != w.width || prev.height != w.height {
                let app_name = CString::new(w.app_name.as_str()).unwrap_or_default().into_raw();
                changes.push(Q3ideWindowChange {
                    window_id: w.window_id,
                    app_name,
                    width: w.width,
                    height: w.height,
                    is_added: 2,
                });
            }
        }
    }

    // Update snapshot to current.
    let new_known: HashMap<u32, backend::WindowInfo> =
        current.into_iter().map(|w| (w.window_id, w)).collect();
    *known_lock = Some(new_known);

    if changes.is_empty() {
        return empty;
    }

    changes.shrink_to_fit(); /* capacity must == len before forget, else free corrupts heap */
    let count = changes.len() as c_uint;
    let ptr = changes.as_mut_ptr();
    std::mem::forget(changes);
    Q3ideWindowChangeList { changes: ptr, count }
}

/// Free a change list returned by `q3ide_poll_window_changes`.
///
/// # Safety
/// `list` must have been returned by `q3ide_poll_window_changes`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_free_change_list(list: Q3ideWindowChangeList) {
    if list.changes.is_null() || list.count == 0 {
        return;
    }
    let changes =
        Vec::from_raw_parts(list.changes, list.count as usize, list.count as usize);
    for ch in &changes {
        if !ch.app_name.is_null() {
            let _ = CString::from_raw(ch.app_name);
        }
    }
    // Vec drops here, freeing the changes array.
}

/// Attach a window by title substring match.
/// Convenience for `/q3ide_attach <title>`.
/// Returns the window ID on success, 0 on failure.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
/// `title_query` must be a valid null-terminated UTF-8 string.
#[no_mangle]
pub unsafe extern "C" fn q3ide_attach_by_title(
    handle: *mut Q3ideCapture,
    title_query: *const c_char,
    target_fps: c_uint,
) -> c_uint {
    if handle.is_null() || title_query.is_null() {
        return 0;
    }

    let query = match CStr::from_ptr(title_query).to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };

    let ctx = &mut *handle;
    let windows = match ctx.backend.list_windows() {
        Ok(w) => w,
        Err(_) => return 0,
    };

    let matches = window::find_windows_by_title(&windows, query);
    if let Some(win) = matches.first() {
        let wid = win.window_id;
        match ctx.backend.start_capture(wid, target_fps) {
            Ok(()) => wid,
            Err(e) => {
                log::error!("q3ide_attach_by_title: failed to start capture: {e}");
                0
            }
        }
    } else {
        log::warn!("q3ide_attach_by_title: no window matching '{query}'");
        0
    }
}

/// Display info returned to the engine.
#[repr(C)]
pub struct Q3ideDisplayInfo {
    pub display_id: c_uint,
    pub width: c_uint,
    pub height: c_uint,
    pub x: c_int,
    pub y: c_int,
}

/// Display list returned by `q3ide_list_displays`.
#[repr(C)]
pub struct Q3ideDisplayList {
    pub displays: *const Q3ideDisplayInfo,
    pub count: c_uint,
}

/// Special window ID used for desktop capture (all displays composited).
/// Use this ID with q3ide_get_frame to retrieve the composited desktop frame.
pub const Q3IDE_DESKTOP_CAPTURE_ID: c_uint = 0xFFFF_FFFF;

/// List available displays.
///
/// Returns a `Q3ideDisplayList`. The caller must free it with `q3ide_free_display_list`.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_list_displays(handle: *mut Q3ideCapture) -> Q3ideDisplayList {
    let empty = Q3ideDisplayList {
        displays: ptr::null(),
        count: 0,
    };

    if handle.is_null() {
        return empty;
    }

    let ctx = &*handle;
    let displays = match ctx.backend.list_displays() {
        Ok(d) => d,
        Err(e) => {
            log::error!("q3ide_list_displays failed: {e}");
            return empty;
        }
    };

    if displays.is_empty() {
        return empty;
    }

    let c_displays: Vec<Q3ideDisplayInfo> = displays
        .iter()
        .map(|d| Q3ideDisplayInfo {
            display_id: d.display_id,
            width: d.width,
            height: d.height,
            x: d.x,
            y: d.y,
        })
        .collect();

    let count = c_displays.len() as c_uint;
    let ptr = c_displays.as_ptr();
    std::mem::forget(c_displays);

    Q3ideDisplayList { displays: ptr, count }
}

/// Free a display list returned by `q3ide_list_displays`.
///
/// # Safety
/// `list` must have been returned by `q3ide_list_displays`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_free_display_list(list: Q3ideDisplayList) {
    if list.displays.is_null() || list.count == 0 {
        return;
    }

    let _displays = Vec::from_raw_parts(
        list.displays as *mut Q3ideDisplayInfo,
        list.count as usize,
        list.count as usize,
    );
    // Vec drops here, freeing the display info array
}

/// Start capturing a single display by its CGDirectDisplayID.
///
/// The display is captured independently — frames are keyed by display_id.
/// Use `q3ide_get_frame(handle, display_id)` to retrieve frames, and
/// `q3ide_stop_capture(handle, display_id)` to stop.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_start_display_capture(
    handle: *mut Q3ideCapture,
    display_id: c_uint,
    target_fps: c_uint,
) -> Q3ideError {
    if handle.is_null() {
        return Q3ideError::NullPointer;
    }

    let ctx = &mut *handle;
    match ctx.backend.start_display_capture(display_id, target_fps) {
        Ok(()) => Q3ideError::Ok,
        Err(e) => {
            log::error!("q3ide_start_display_capture failed: {e}");
            match e {
                backend::CaptureError::WindowNotFound(_) => Q3ideError::WindowNotFound,
                backend::CaptureError::PermissionDenied => Q3ideError::PermissionDenied,
                backend::CaptureError::AlreadyCapturing(_) => Q3ideError::AlreadyCapturing,
                backend::CaptureError::NotAvailable(_) => Q3ideError::NotAvailable,
                backend::CaptureError::Platform(_) => Q3ideError::PlatformError,
            }
        }
    }
}

// ─── Input injection ─────────────────────────────────────────────────────────

/// CGPoint used for mouse event injection.
#[repr(C)]
#[derive(Clone, Copy)]
struct CGPoint {
    x: f64,
    y: f64,
}

// CoreGraphics CGEvent C-ABI. Only compiled on macOS — the dylib is macOS-only.
#[cfg(target_os = "macos")]
#[link(name = "CoreGraphics", kind = "framework")]
extern "C" {
    fn CGEventCreateMouseEvent(
        source: *mut std::ffi::c_void,
        mouse_type: u32,
        mouse_cursor_position: CGPoint,
        mouse_button: u32,
    ) -> *mut std::ffi::c_void;

    fn CGEventCreateKeyboardEvent(
        source: *mut std::ffi::c_void,
        virtual_key: u16,
        key_down: u8,
    ) -> *mut std::ffi::c_void;

    fn CGEventPost(tap: u32, event: *mut std::ffi::c_void);

    fn CFRelease(cf: *mut std::ffi::c_void);
}

// CGEventType constants
const CG_LEFT_MOUSE_DOWN: u32 = 1;
const CG_LEFT_MOUSE_UP: u32 = 2;
// CGMouseButton::Left
const CG_MOUSE_BUTTON_LEFT: u32 = 0;
// CGEventTapLocation::HID
const CG_EVENT_TAP_HID: u32 = 0;

/// Map Q3 key codes (from keycodes.h) to macOS CGKeyCode (virtual key codes).
/// ASCII letters / digits pass through as-is for the most common layout;
/// special keys (arrows, function keys) are mapped by table lookup.
fn q3key_to_cgkeycode(key: i32) -> Option<u16> {
    // ASCII lowercase letters → CGKeyCode (ANSI layout)
    if key >= b'a' as i32 && key <= b'z' as i32 {
        const LETTER_MAP: [u16; 26] = [
            0,  // a
            11, // b
            8,  // c
            2,  // d
            14, // e
            3,  // f
            5,  // g
            4,  // h
            34, // i
            38, // j
            40, // k
            37, // l
            46, // m
            45, // n
            31, // o
            35, // p
            12, // q
            15, // r
            1,  // s
            17, // t
            32, // u
            9,  // v
            13, // w
            7,  // x
            16, // y
            6,  // z
        ];
        return Some(LETTER_MAP[(key - b'a' as i32) as usize]);
    }
    // Digits 0-9
    if key >= b'0' as i32 && key <= b'9' as i32 {
        const DIGIT_MAP: [u16; 10] = [29, 18, 19, 20, 21, 23, 22, 26, 28, 25];
        return Some(DIGIT_MAP[(key - b'0' as i32) as usize]);
    }
    match key {
        9   => Some(48),  // K_TAB
        13  => Some(36),  // K_ENTER
        27  => Some(53),  // K_ESCAPE
        32  => Some(49),  // K_SPACE
        39  => Some(39),  // K_QUOTE (')
        44  => Some(43),  // K_COMMA
        45  => Some(27),  // K_MINUS
        46  => Some(47),  // K_DOT
        47  => Some(44),  // K_SLASH
        59  => Some(41),  // K_SEMICOLON
        61  => Some(24),  // K_EQUAL
        91  => Some(33),  // K_BRACKET_OPEN  [
        92  => Some(42),  // K_BACKSLASH
        93  => Some(30),  // K_BRACKET_CLOSE ]
        127 => Some(51),  // K_BACKSPACE
        // K_COMMAND = 128
        128 => Some(55),
        // K_CAPSLOCK = 129
        129 => Some(57),
        // K_UPARROW = 132
        132 => Some(126),
        // K_DOWNARROW = 133
        133 => Some(125),
        // K_LEFTARROW = 134
        134 => Some(123),
        // K_RIGHTARROW = 135
        135 => Some(124),
        // K_ALT = 136
        136 => Some(58),
        // K_CTRL = 137
        137 => Some(59),
        // K_SHIFT = 138
        138 => Some(56),
        // K_INS = 139
        139 => Some(114),
        // K_DEL = 140
        140 => Some(117),
        // K_PGDN = 141
        141 => Some(121),
        // K_PGUP = 142
        142 => Some(116),
        // K_HOME = 143
        143 => Some(115),
        // K_END = 144
        144 => Some(119),
        // K_F1..K_F15 = 145..159
        145 => Some(122), // F1
        146 => Some(120), // F2
        147 => Some(99),  // F3
        148 => Some(118), // F4
        149 => Some(96),  // F5
        150 => Some(97),  // F6
        151 => Some(98),  // F7
        152 => Some(100), // F8
        153 => Some(101), // F9
        154 => Some(109), // F10
        155 => Some(103), // F11
        156 => Some(111), // F12
        _ => None,
    }
}

/// Inject a left mouse click at UV coordinates within a captured window.
///
/// `uv_x` and `uv_y` are in [0.0, 1.0] relative to the window's content area.
/// The window's current screen bounds are looked up via ScreenCaptureKit.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_inject_click(
    _handle: *mut Q3ideCapture,
    window_id: c_uint,
    uv_x: c_float,
    uv_y: c_float,
) {
    #[cfg(target_os = "macos")]
    {
        // Use ::screencapturekit to reference the external crate,
        // not the local `mod screencapturekit` module in this file.
        use ::screencapturekit::shareable_content::{SCShareableContent, SCWindow};

        let content: SCShareableContent = match SCShareableContent::get() {
            Ok(c) => c,
            Err(e) => {
                log::warn!("q3ide_inject_click: SCShareableContent failed: {}", e);
                return;
            }
        };

        let windows = content.windows();
        let win: Option<&SCWindow> = windows.iter().find(|w| w.window_id() == window_id);
        let Some(win) = win else {
            log::warn!("q3ide_inject_click: window {} not found", window_id);
            return;
        };

        let frame = win.frame();
        let origin = frame.origin();
        let size = frame.size();
        let screen_x = origin.x + uv_x as f64 * size.width;
        let screen_y = origin.y + uv_y as f64 * size.height;
        let pos = CGPoint { x: screen_x, y: screen_y };

        let down_ev = CGEventCreateMouseEvent(
            std::ptr::null_mut(),
            CG_LEFT_MOUSE_DOWN,
            pos,
            CG_MOUSE_BUTTON_LEFT,
        );
        if !down_ev.is_null() {
            CGEventPost(CG_EVENT_TAP_HID, down_ev);
            CFRelease(down_ev);
        }

        let up_ev = CGEventCreateMouseEvent(
            std::ptr::null_mut(),
            CG_LEFT_MOUSE_UP,
            pos,
            CG_MOUSE_BUTTON_LEFT,
        );
        if !up_ev.is_null() {
            CGEventPost(CG_EVENT_TAP_HID, up_ev);
            CFRelease(up_ev);
        }

        log::info!(
            "q3ide_inject_click: wid={} uv=({:.3},{:.3}) screen=({:.0},{:.0})",
            window_id, uv_x, uv_y, screen_x, screen_y
        );
    }
}

/// Inject a keyboard key event into a captured window.
///
/// `q3key` is a Q3 keycode (from keycodes.h). `is_down` is 1 for press, 0 for release.
/// Maps Q3 key codes to macOS CGKeyCode (ANSI layout).
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_inject_key(
    _handle: *mut Q3ideCapture,
    window_id: c_uint,
    q3key: c_int,
    is_down: c_int,
) {
    #[cfg(target_os = "macos")]
    {
        let Some(cgkey) = q3key_to_cgkeycode(q3key) else {
            log::debug!("q3ide_inject_key: no CGKeyCode mapping for Q3 key {}", q3key);
            return;
        };

        let ev = CGEventCreateKeyboardEvent(
            std::ptr::null_mut(),
            cgkey,
            if is_down != 0 { 1 } else { 0 },
        );
        if !ev.is_null() {
            CGEventPost(CG_EVENT_TAP_HID, ev);
            CFRelease(ev);
        }

        log::debug!(
            "q3ide_inject_key: wid={} q3key={} cgkey={} down={}",
            window_id, q3key, cgkey, is_down
        );
    }
}

/// Start capturing all displays, composited vertically into a single frame.
///
/// All displays are scaled to the same width (max display width) while
/// maintaining aspect ratio, then stacked vertically.
///
/// Returns the special Q3IDE_DESKTOP_CAPTURE_ID on success, 0 on failure.
/// Use this ID with q3ide_get_frame to retrieve composited frames.
///
/// # Safety
/// `handle` must be a valid pointer from `q3ide_init`.
#[no_mangle]
pub unsafe extern "C" fn q3ide_start_desktop_capture(
    handle: *mut Q3ideCapture,
    target_fps: c_uint,
) -> c_uint {
    if handle.is_null() {
        return 0;
    }

    let ctx = &mut *handle;
    match ctx.backend.start_desktop_capture(target_fps) {
        Ok(id) => {
            log::info!("q3ide_start_desktop_capture: started with id={}", id);
            id
        }
        Err(e) => {
            log::error!("q3ide_start_desktop_capture failed: {e}");
            0
        }
    }
}
