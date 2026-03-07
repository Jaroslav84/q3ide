# FFI Error Handling: Preventing Panics Across Boundaries

> **Sources:**
> - <https://doc.rust-lang.org/std/panic/fn.catch_unwind.html> (catch_unwind docs)
> - <https://doc.rust-lang.org/nomicon/ffi.html> (Rustonomicon FFI chapter)
> - <https://nrc.github.io/error-docs/rust-errors/interop.html> (Error Handling in Rust - FFI interop)
> - <https://effective-rust.com/panic.html> (Effective Rust: Item 18 - Don't panic)
> - <https://rust-lang.github.io/rfcs/1236-stabilize-catch-panic.html> (RFC 1236: catch_panic)
> - <https://rust-lang.github.io/rfcs/2945-c-unwind-abi.html> (RFC 2945: C-unwind ABI)
> - <https://docs.rs/ffi_helpers/latest/ffi_helpers/panic/index.html> (ffi_helpers crate)
> - <https://internals.rust-lang.org/t/unhandled-panics-in-rust-vs-in-ffi/17981> (Rust internals discussion)
> - <https://medium.com/@QuarkAndCode/ffi-best-practices-for-rust-deno-mojo-5b9950dde5ce> (FFI best practices)
>
> **Description:** Comprehensive guide to handling errors across Rust FFI boundaries without panicking, using error codes, catch_unwind, and Result-to-error-code patterns. Critical for the q3ide-capture dylib where panics would crash the Quake3e engine.

---

## The Core Problem

**Panicking across an FFI boundary is undefined behavior.** If C calls into Rust and Rust panics (unwinds), the behavior is undefined. This can corrupt the call stack, crash the host process, or cause silent data corruption.

For q3ide-capture, this means: if a Rust function called by the Quake3e engine panics, the entire game crashes with undefined behavior. This must never happen.

## Strategy Overview

1. **Never panic** in FFI-exposed functions
2. **Use `catch_unwind`** as a safety net to convert panics to error codes
3. **Return error codes** (integers or enums) across the boundary
4. **Store detailed error information** in thread-local storage for retrieval
5. **Validate all inputs** before processing

## Pattern 1: Error Code Returns

The simplest and most common pattern. Return an integer where 0 = success and negative values indicate errors.

```rust
use std::os::raw::c_int;

/// Error codes returned by q3ide functions
#[repr(C)]
pub enum Q3IDEError {
    Success = 0,
    NullPointer = -1,
    NotInitialized = -2,
    NoFrame = -3,
    InvalidArgument = -4,
    CaptureError = -5,
    InternalError = -99,
}

#[no_mangle]
pub extern "C" fn q3ide_init() -> *mut CaptureHandle {
    // If anything here can fail, handle it explicitly
    match CaptureHandle::new() {
        Ok(handle) => Box::into_raw(Box::new(handle)),
        Err(_) => std::ptr::null_mut(), // Return null on failure
    }
}

#[no_mangle]
pub extern "C" fn q3ide_get_frame(
    handle: *mut CaptureHandle,
    frame_out: *mut FrameData,
) -> c_int {
    if handle.is_null() || frame_out.is_null() {
        return Q3IDEError::NullPointer as c_int;
    }

    unsafe {
        match (*handle).get_latest_frame() {
            Ok(Some(frame)) => {
                *frame_out = frame;
                Q3IDEError::Success as c_int
            }
            Ok(None) => Q3IDEError::NoFrame as c_int,
            Err(_) => Q3IDEError::CaptureError as c_int,
        }
    }
}
```

## Pattern 2: catch_unwind Safety Net

Use `std::panic::catch_unwind` to catch any unexpected panics and convert them to error codes. This is your last line of defense.

```rust
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::os::raw::c_int;

#[no_mangle]
pub extern "C" fn q3ide_process(handle: *mut CaptureHandle) -> c_int {
    // catch_unwind prevents panics from crossing the FFI boundary
    let result = catch_unwind(AssertUnwindSafe(|| {
        if handle.is_null() {
            return Q3IDEError::NullPointer as c_int;
        }

        unsafe {
            match (*handle).process() {
                Ok(_) => Q3IDEError::Success as c_int,
                Err(e) => {
                    eprintln!("q3ide_process error: {}", e);
                    Q3IDEError::CaptureError as c_int
                }
            }
        }
    }));

    match result {
        Ok(code) => code,
        Err(_) => {
            // A panic occurred! Log it and return error code.
            eprintln!("PANIC in q3ide_process - caught at FFI boundary");
            Q3IDEError::InternalError as c_int
        }
    }
}
```

### Important Notes on catch_unwind

- `catch_unwind` only catches **unwinding** panics, not aborts (`panic = "abort"` in Cargo.toml)
- The closure must be `UnwindSafe`. Use `AssertUnwindSafe` wrapper when needed
- `catch_unwind` is NOT a general-purpose exception mechanism -- it's a safety net
- After catching a panic, the state may be inconsistent; be cautious about continuing

## Pattern 3: FFI Wrapper Macro

Create a macro to standardize error handling across all FFI functions:

```rust
use std::panic::{catch_unwind, AssertUnwindSafe};

/// Wraps a closure in catch_unwind and converts panics to error codes.
/// Returns Q3IDEError::InternalError (-99) on panic.
macro_rules! ffi_guard {
    ($body:expr) => {{
        let result = catch_unwind(AssertUnwindSafe(|| $body));
        match result {
            Ok(val) => val,
            Err(panic_info) => {
                // Log the panic
                if let Some(msg) = panic_info.downcast_ref::<&str>() {
                    eprintln!("q3ide PANIC: {}", msg);
                } else if let Some(msg) = panic_info.downcast_ref::<String>() {
                    eprintln!("q3ide PANIC: {}", msg);
                } else {
                    eprintln!("q3ide PANIC: <unknown>");
                }
                Q3IDEError::InternalError as i32
            }
        }
    }};
}

// Usage:
#[no_mangle]
pub extern "C" fn q3ide_init(config: *const Config) -> *mut CaptureHandle {
    let result = catch_unwind(AssertUnwindSafe(|| {
        if config.is_null() {
            return std::ptr::null_mut();
        }
        match CaptureHandle::new(unsafe { &*config }) {
            Ok(h) => Box::into_raw(Box::new(h)),
            Err(_) => std::ptr::null_mut(),
        }
    }));
    result.unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn q3ide_get_frame(
    handle: *mut CaptureHandle,
    out: *mut FrameData,
) -> i32 {
    ffi_guard!({
        if handle.is_null() || out.is_null() {
            return Q3IDEError::NullPointer as i32;
        }
        unsafe {
            match (*handle).get_frame() {
                Ok(frame) => {
                    *out = frame;
                    Q3IDEError::Success as i32
                }
                Err(_) => Q3IDEError::CaptureError as i32,
            }
        }
    })
}

#[no_mangle]
pub extern "C" fn q3ide_shutdown(handle: *mut CaptureHandle) -> i32 {
    ffi_guard!({
        if handle.is_null() {
            return Q3IDEError::NullPointer as i32;
        }
        unsafe {
            let _ = Box::from_raw(handle);
        }
        Q3IDEError::Success as i32
    })
}
```

## Pattern 4: Thread-Local Error Storage

Store detailed error messages in thread-local storage so C code can retrieve them:

```rust
use std::cell::RefCell;
use std::ffi::{c_char, CString};

thread_local! {
    static LAST_ERROR: RefCell<Option<CString>> = RefCell::new(None);
}

/// Store an error message for later retrieval by C code.
fn set_last_error(msg: &str) {
    LAST_ERROR.with(|cell| {
        *cell.borrow_mut() = CString::new(msg).ok();
    });
}

/// Clear the last error.
fn clear_last_error() {
    LAST_ERROR.with(|cell| {
        *cell.borrow_mut() = None;
    });
}

/// Get the last error message. Returns null if no error.
/// The returned pointer is valid until the next q3ide call on this thread.
#[no_mangle]
pub extern "C" fn q3ide_last_error() -> *const c_char {
    LAST_ERROR.with(|cell| {
        match cell.borrow().as_ref() {
            Some(err) => err.as_ptr(),
            None => std::ptr::null(),
        }
    })
}

// Usage in FFI functions:
#[no_mangle]
pub extern "C" fn q3ide_attach(handle: *mut CaptureHandle, window_id: u64) -> i32 {
    ffi_guard!({
        clear_last_error();

        if handle.is_null() {
            set_last_error("Handle is null");
            return Q3IDEError::NullPointer as i32;
        }

        unsafe {
            match (*handle).attach_window(window_id) {
                Ok(_) => Q3IDEError::Success as i32,
                Err(e) => {
                    set_last_error(&format!("Failed to attach window {}: {}", window_id, e));
                    Q3IDEError::CaptureError as i32
                }
            }
        }
    })
}
```

C usage:

```c
int result = q3ide_attach(handle, window_id);
if (result != 0) {
    const char* err = q3ide_last_error();
    if (err) {
        Com_Printf("q3ide error: %s\n", err);
    }
}
```

## Pattern 5: Result-to-Error-Code Conversion

A systematic approach to converting Rust `Result` types to C error codes:

```rust
/// Internal error type with rich information
#[derive(Debug)]
pub enum CaptureError {
    PermissionDenied,
    WindowNotFound(u64),
    FrameBufferFull,
    ScreenCaptureError(String),
    IoError(std::io::Error),
}

impl CaptureError {
    /// Convert to C-compatible error code
    fn to_error_code(&self) -> i32 {
        match self {
            CaptureError::PermissionDenied => -1,
            CaptureError::WindowNotFound(_) => -2,
            CaptureError::FrameBufferFull => -3,
            CaptureError::ScreenCaptureError(_) => -4,
            CaptureError::IoError(_) => -5,
        }
    }
}

/// Helper: convert Result to error code, storing error details
fn result_to_code<T>(result: Result<T, CaptureError>) -> i32 {
    match result {
        Ok(_) => 0,
        Err(e) => {
            set_last_error(&format!("{:?}", e));
            e.to_error_code()
        }
    }
}

/// Helper: convert Result to error code with output parameter
fn result_to_code_with_output<T: Copy>(
    result: Result<T, CaptureError>,
    out: *mut T,
) -> i32 {
    match result {
        Ok(val) => {
            if !out.is_null() {
                unsafe { *out = val; }
            }
            0
        }
        Err(e) => {
            set_last_error(&format!("{:?}", e));
            e.to_error_code()
        }
    }
}
```

## Pattern 6: Custom Panic Hook

Set a custom panic hook to log panics before they are caught:

```rust
use std::panic;

/// Call this once during initialization to set up panic logging.
fn install_panic_hook() {
    panic::set_hook(Box::new(|info| {
        let msg = if let Some(s) = info.payload().downcast_ref::<&str>() {
            s.to_string()
        } else if let Some(s) = info.payload().downcast_ref::<String>() {
            s.clone()
        } else {
            "Unknown panic".to_string()
        };

        let location = info.location().map_or_else(
            || "unknown location".to_string(),
            |loc| format!("{}:{}:{}", loc.file(), loc.line(), loc.column()),
        );

        eprintln!("[q3ide-capture] PANIC at {}: {}", location, msg);

        // Could also write to a log file, send telemetry, etc.
    }));
}

#[no_mangle]
pub extern "C" fn q3ide_init() -> *mut CaptureHandle {
    // Install panic hook on first call
    static INIT: std::sync::Once = std::sync::Once::new();
    INIT.call_once(|| {
        install_panic_hook();
    });

    // ... rest of init ...
    std::ptr::null_mut()
}
```

## Common Pitfalls

### 1. Forgetting to Handle Allocation Failures

```rust
// BAD: Vec::with_capacity can panic if allocation fails
#[no_mangle]
pub extern "C" fn allocate_buffer(size: usize) -> *mut u8 {
    let mut buf = Vec::with_capacity(size); // PANIC if OOM!
    buf.as_mut_ptr()
}

// GOOD: Use try_reserve or catch_unwind
#[no_mangle]
pub extern "C" fn allocate_buffer(size: usize) -> *mut u8 {
    let result = catch_unwind(|| {
        let mut buf = Vec::with_capacity(size);
        let ptr = buf.as_mut_ptr();
        std::mem::forget(buf); // Don't drop the Vec
        ptr
    });
    result.unwrap_or(std::ptr::null_mut())
}
```

### 2. Unwrap/Expect in FFI Code

```rust
// BAD: unwrap() will panic on None/Err
#[no_mangle]
pub extern "C" fn get_name(ctx: *const Context) -> *const c_char {
    unsafe { CStr::from_ptr((*ctx).name).to_str().unwrap().as_ptr() as *const c_char }
}

// GOOD: Handle errors explicitly
#[no_mangle]
pub extern "C" fn get_name(ctx: *const Context) -> *const c_char {
    if ctx.is_null() {
        return std::ptr::null();
    }
    unsafe {
        match CStr::from_ptr((*ctx).name).to_str() {
            Ok(s) => s.as_ptr() as *const c_char,
            Err(_) => std::ptr::null(),
        }
    }
}
```

### 3. Index Out of Bounds

```rust
// BAD: array[i] can panic
#[no_mangle]
pub extern "C" fn get_item(arr: *const Item, len: usize, index: usize) -> Item {
    let slice = unsafe { std::slice::from_raw_parts(arr, len) };
    slice[index] // PANIC if index >= len!
}

// GOOD: Bounds check first
#[no_mangle]
pub extern "C" fn get_item(
    arr: *const Item,
    len: usize,
    index: usize,
    out: *mut Item,
) -> i32 {
    if arr.is_null() || out.is_null() || index >= len {
        return -1;
    }
    let slice = unsafe { std::slice::from_raw_parts(arr, len) };
    unsafe { *out = slice[index]; }
    0
}
```

### 4. Integer Overflow

```rust
// BAD: arithmetic overflow panics in debug mode
#[no_mangle]
pub extern "C" fn compute(a: u32, b: u32) -> u32 {
    a * b // PANIC on overflow in debug!
}

// GOOD: Use checked/wrapping/saturating arithmetic
#[no_mangle]
pub extern "C" fn compute(a: u32, b: u32, out: *mut u32) -> i32 {
    match a.checked_mul(b) {
        Some(result) => {
            if !out.is_null() { unsafe { *out = result; } }
            0
        }
        None => -1, // Overflow
    }
}
```

## Cargo.toml Considerations

```toml
[profile.release]
# Consider using panic = "abort" if you ALSO use catch_unwind
# Note: catch_unwind becomes a no-op with panic = "abort"
# For FFI libraries, usually keep panic = "unwind" so catch_unwind works
panic = "unwind"

# Alternatively, if you're absolutely sure no panics can occur:
# panic = "abort"  # Smaller binary, but panics terminate the process
```

## Complete Example: q3ide-capture FFI Layer

```rust
//! FFI boundary for q3ide-capture.
//! All functions in this module use catch_unwind and return error codes.
//! No panic can escape this module.

use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ffi::c_int;

mod capture; // Internal implementation

// Re-export the repr(C) types
pub use capture::{CaptureHandle, FrameData};

#[repr(C)]
pub enum Q3IDEResult {
    Ok = 0,
    ErrNull = -1,
    ErrNotInit = -2,
    ErrNoFrame = -3,
    ErrCapture = -4,
    ErrPermission = -5,
    ErrInternal = -99,
}

macro_rules! ffi_guard {
    ($body:expr) => {
        match catch_unwind(AssertUnwindSafe(|| $body)) {
            Ok(v) => v,
            Err(_) => Q3IDEResult::ErrInternal as c_int,
        }
    };
}

#[no_mangle]
pub extern "C" fn q3ide_init() -> *mut CaptureHandle {
    catch_unwind(AssertUnwindSafe(|| {
        match capture::CaptureHandle::new() {
            Ok(h) => Box::into_raw(Box::new(h)),
            Err(_) => std::ptr::null_mut(),
        }
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn q3ide_get_frame(
    handle: *mut CaptureHandle,
    out: *mut FrameData,
) -> c_int {
    ffi_guard!({
        if handle.is_null() || out.is_null() {
            return Q3IDEResult::ErrNull as c_int;
        }
        unsafe {
            match (*handle).get_frame() {
                Ok(Some(frame)) => {
                    *out = frame;
                    Q3IDEResult::Ok as c_int
                }
                Ok(None) => Q3IDEResult::ErrNoFrame as c_int,
                Err(_) => Q3IDEResult::ErrCapture as c_int,
            }
        }
    })
}

#[no_mangle]
pub extern "C" fn q3ide_shutdown(handle: *mut CaptureHandle) {
    let _ = catch_unwind(AssertUnwindSafe(|| {
        if !handle.is_null() {
            unsafe { let _ = Box::from_raw(handle); }
        }
    }));
}
```

## Summary of Rules

1. **Every `extern "C"` function must be panic-safe** -- wrap with `catch_unwind` or guarantee no panics
2. **Never use `.unwrap()` or `.expect()`** in FFI-facing code
3. **Return error codes**, not Result or Option
4. **Null-check all pointers** before use
5. **Use `#[repr(C)]`** on all types that cross the boundary
6. **Use `AssertUnwindSafe`** when catch_unwind complains about captured references
7. **Log panics** so debugging is possible
8. **Store detailed errors** in thread-local storage for retrieval by C code
9. **Keep `panic = "unwind"`** in Cargo.toml so catch_unwind works
10. **Test FFI functions** with null pointers, invalid handles, and edge cases
