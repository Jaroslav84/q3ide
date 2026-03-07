# Rust cdylib: Building C-ABI Dynamic Libraries

> **Sources:**
> - <https://doc.rust-lang.org/nomicon/ffi.html> (The Rustonomicon FFI chapter)
> - <https://rust-lang.github.io/rfcs/1510-cdylib.html> (RFC 1510: cdylib crate type)
> - <https://docs.rust-embedded.org/book/interoperability/rust-with-c.html> (Embedded Rust Book)
> - <https://github.com/NenX/Rust-and-C-Interoperability-Practical-Guide> (Interop guide)
> - <https://developers.redhat.com/articles/2022/09/05/how-create-c-binding-rust-library> (Red Hat guide)
>
> **Description:** Complete guide to creating a Rust cdylib (C-compatible dynamic library) with `#[no_mangle]`, `extern "C"`, `#[repr(C)]`, and proper type mappings. Directly applicable to building the q3ide-capture dylib.

---

## What is a cdylib?

A `cdylib` is a Rust crate type that produces a C-compatible dynamic library:

- `.dylib` on macOS
- `.so` on Linux
- `.dll` on Windows

Unlike `dylib` (which produces a Rust-ABI dynamic library), `cdylib` strips out Rust-specific metadata and produces a clean dynamic library suitable for consumption by C, C++, Python, or any other language that can load shared libraries.

## Project Setup

### Cargo.toml

```toml
[package]
name = "q3ide-capture"
version = "0.1.0"
edition = "2021"

[lib]
name = "q3ide_capture"
crate-type = ["cdylib"]

# Optionally also build a regular Rust lib for testing
# crate-type = ["cdylib", "rlib"]

[dependencies]
crossbeam = "0.8"
```

Key points:
- `crate-type = ["cdylib"]` tells Cargo to produce a C-compatible dynamic library
- The `name` field under `[lib]` determines the output filename (e.g., `libq3ide_capture.dylib`)
- You can specify multiple crate types if you also want a Rust library for testing

### Build

```bash
cargo build --release
# Output: target/release/libq3ide_capture.dylib (macOS)
# Output: target/release/libq3ide_capture.so    (Linux)
# Output: target/release/q3ide_capture.dll       (Windows)
```

## Core Concepts

### `#[no_mangle]`

The Rust compiler mangles function names to include type information and prevent collisions. The `#[no_mangle]` attribute disables this, preserving the exact function name so C code can find the symbol.

```rust
// Without #[no_mangle]: symbol might be _ZN7my_lib5hello17h8f4e5a6b3c2d1e0fE
// With #[no_mangle]:    symbol is exactly "hello"

#[no_mangle]
pub extern "C" fn hello() {
    println!("Hello from Rust!");
}
```

In newer Rust editions, `#[unsafe(no_mangle)]` is preferred to make the unsafe nature explicit:

```rust
#[unsafe(no_mangle)]
pub extern "C" fn hello() {
    println!("Hello from Rust!");
}
```

### `extern "C"`

Tells the Rust compiler to use the C calling convention (ABI) instead of the default Rust ABI. This ensures that:
- Arguments are passed in the order C expects
- Return values use the C convention
- Stack cleanup follows C rules

```rust
#[no_mangle]
pub extern "C" fn add(a: i32, b: i32) -> i32 {
    a + b
}
```

### `#[repr(C)]`

Ensures a Rust struct or enum has the same memory layout as its C equivalent. Without this, the Rust compiler is free to reorder fields and add padding differently from C.

```rust
#[repr(C)]
pub struct FrameData {
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub pixel_data: *const u8,
    pub timestamp_ns: u64,
}
```

## Complete Example: Minimal cdylib

### src/lib.rs

```rust
use std::ffi::{c_char, c_int, CStr, CString};
use std::ptr;

/// Opaque handle to internal state.
/// C code receives and passes this pointer but cannot inspect it.
pub struct CaptureContext {
    name: String,
    counter: u32,
}

/// Initialize the capture system. Returns an opaque handle.
/// Caller must eventually call `capture_shutdown` to free the handle.
///
/// Returns null on failure.
#[no_mangle]
pub extern "C" fn capture_init() -> *mut CaptureContext {
    let ctx = Box::new(CaptureContext {
        name: String::from("default"),
        counter: 0,
    });
    Box::into_raw(ctx) // Transfer ownership to C
}

/// Get the current counter value.
/// Returns -1 if the handle is null.
#[no_mangle]
pub extern "C" fn capture_get_counter(ctx: *mut CaptureContext) -> c_int {
    if ctx.is_null() {
        return -1;
    }
    unsafe {
        (*ctx).counter as c_int
    }
}

/// Increment the counter. Returns the new value.
/// Returns -1 if the handle is null.
#[no_mangle]
pub extern "C" fn capture_increment(ctx: *mut CaptureContext) -> c_int {
    if ctx.is_null() {
        return -1;
    }
    unsafe {
        (*ctx).counter += 1;
        (*ctx).counter as c_int
    }
}

/// Shut down the capture system and free the handle.
/// After calling this, the handle is invalid and must not be used.
#[no_mangle]
pub extern "C" fn capture_shutdown(ctx: *mut CaptureContext) {
    if !ctx.is_null() {
        // Reconstruct the Box to properly drop it
        unsafe {
            let _ = Box::from_raw(ctx);
        }
    }
}
```

### Corresponding C Header

```c
#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdint.h>

/* Opaque handle type */
typedef struct CaptureContext CaptureContext;

/* Initialize the capture system. Returns NULL on failure. */
CaptureContext* capture_init(void);

/* Get the current counter value. Returns -1 on error. */
int capture_get_counter(CaptureContext* ctx);

/* Increment the counter. Returns the new value, or -1 on error. */
int capture_increment(CaptureContext* ctx);

/* Shut down and free the capture context. */
void capture_shutdown(CaptureContext* ctx);

#endif /* CAPTURE_H */
```

### C Code That Uses It

```c
#include <stdio.h>
#include "capture.h"

int main(void) {
    CaptureContext* ctx = capture_init();
    if (ctx == NULL) {
        fprintf(stderr, "Failed to initialize capture\n");
        return 1;
    }

    printf("Counter: %d\n", capture_get_counter(ctx));  // 0
    capture_increment(ctx);
    capture_increment(ctx);
    printf("Counter: %d\n", capture_get_counter(ctx));  // 2

    capture_shutdown(ctx);
    return 0;
}
```

### Compile and Run

```bash
# Build the Rust dylib
cargo build --release

# Compile the C program (macOS)
gcc main.c -o main -L target/release -lq3ide_capture

# Run (macOS)
DYLD_LIBRARY_PATH=target/release ./main

# Run (Linux)
LD_LIBRARY_PATH=target/release ./main
```

## Type Mappings: Rust to C

| Rust Type | C Type | Notes |
|-----------|--------|-------|
| `bool` | `bool` / `_Bool` | C99 `<stdbool.h>` |
| `i8` / `u8` | `int8_t` / `uint8_t` | `<stdint.h>` |
| `i16` / `u16` | `int16_t` / `uint16_t` | |
| `i32` / `u32` | `int32_t` / `uint32_t` | |
| `i64` / `u64` | `int64_t` / `uint64_t` | |
| `f32` | `float` | |
| `f64` | `double` | |
| `usize` | `size_t` | `<stddef.h>` |
| `isize` | `ptrdiff_t` | `<stddef.h>` |
| `*const T` | `const T*` | |
| `*mut T` | `T*` | |
| `std::ffi::c_char` | `char` | |
| `std::ffi::c_int` | `int` | |
| `std::ffi::c_void` | `void` | |
| `Option<extern "C" fn()>` | `void (*)(void)` | Nullable function pointer |
| `Box<T>` (via `into_raw`) | `T*` | Ownership transfer |

## Opaque Types Pattern

The recommended pattern for exposing complex Rust types to C is through opaque pointers:

```rust
/// Internal Rust struct - not exposed to C
pub struct InternalState {
    // Complex Rust types that C cannot understand
    data: Vec<u8>,
    map: std::collections::HashMap<String, i32>,
}

/// Create - returns opaque pointer
#[no_mangle]
pub extern "C" fn state_new() -> *mut InternalState {
    Box::into_raw(Box::new(InternalState {
        data: Vec::new(),
        map: std::collections::HashMap::new(),
    }))
}

/// Use - receives opaque pointer
#[no_mangle]
pub extern "C" fn state_data_len(state: *const InternalState) -> usize {
    if state.is_null() {
        return 0;
    }
    unsafe { (*state).data.len() }
}

/// Destroy - consumes opaque pointer
#[no_mangle]
pub extern "C" fn state_free(state: *mut InternalState) {
    if !state.is_null() {
        unsafe { let _ = Box::from_raw(state); }
    }
}
```

## Repr(C) Structs for Data Transfer

When you need to pass structured data across the boundary:

```rust
/// This struct has guaranteed C-compatible memory layout
#[repr(C)]
pub struct FrameInfo {
    pub width: u32,
    pub height: u32,
    pub bytes_per_pixel: u32,
    pub pixel_format: u32,
    pub data: *const u8,
    pub data_len: usize,
    pub timestamp_ns: u64,
}

#[repr(C)]
pub enum CaptureStatus {
    Success = 0,
    NoFrame = 1,
    Error = 2,
    NotInitialized = 3,
}

#[no_mangle]
pub extern "C" fn get_frame(ctx: *mut CaptureContext, out: *mut FrameInfo) -> CaptureStatus {
    if ctx.is_null() || out.is_null() {
        return CaptureStatus::NotInitialized;
    }
    // ... fill in FrameInfo ...
    CaptureStatus::Success
}
```

## String Handling

Strings require special care because Rust strings are UTF-8 without null terminators, while C strings are null-terminated byte arrays.

```rust
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

/// Returns a newly allocated C string. Caller must free with `free_string`.
#[no_mangle]
pub extern "C" fn get_version() -> *mut c_char {
    let version = CString::new("1.0.0").unwrap();
    version.into_raw() // Transfer ownership to C
}

/// Free a string previously returned by a Rust function.
#[no_mangle]
pub extern "C" fn free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = CString::from_raw(s); // Retake ownership and drop
        }
    }
}

/// Accept a C string from the caller.
#[no_mangle]
pub extern "C" fn set_name(ctx: *mut CaptureContext, name: *const c_char) -> bool {
    if ctx.is_null() || name.is_null() {
        return false;
    }
    unsafe {
        let c_str = CStr::from_ptr(name);
        match c_str.to_str() {
            Ok(s) => {
                (*ctx).name = s.to_owned();
                true
            }
            Err(_) => false, // Invalid UTF-8
        }
    }
}
```

## Array / Buffer Passing

```rust
/// Fill a caller-provided buffer with data.
/// Returns the number of bytes written, or -1 on error.
#[no_mangle]
pub extern "C" fn read_data(
    ctx: *const CaptureContext,
    buf: *mut u8,
    buf_len: usize,
) -> i64 {
    if ctx.is_null() || buf.is_null() {
        return -1;
    }
    unsafe {
        let slice = std::slice::from_raw_parts_mut(buf, buf_len);
        // Fill the buffer...
        let bytes_written = std::cmp::min(buf_len, 100);
        for i in 0..bytes_written {
            slice[i] = i as u8;
        }
        bytes_written as i64
    }
}
```

## Best Practices Summary

1. **Always use `#[no_mangle]`** on exported functions
2. **Always use `extern "C"`** on exported functions
3. **Always use `#[repr(C)]`** on structs/enums passed across the boundary
4. **Always null-check** pointer arguments before dereferencing
5. **Use opaque pointers** for complex Rust types (Box::into_raw / Box::from_raw)
6. **Never panic across FFI** - use `catch_unwind` and error codes (see ffi_error_handling.md)
7. **Document ownership** - make clear who allocates and who frees
8. **Use C-compatible types** - prefer `c_int`, `c_char`, fixed-width integers
9. **Prefix all symbols** - e.g., `q3ide_` to avoid name collisions
10. **Generate headers** with cbindgen rather than maintaining them manually
