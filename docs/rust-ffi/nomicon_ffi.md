# Rust FFI Guide (The Rustonomicon)

> **Source:** <https://doc.rust-lang.org/nomicon/ffi.html>
> **Description:** The official Rust FFI (Foreign Function Interface) chapter from The Rustonomicon, covering calling C from Rust, calling Rust from C, callbacks, linking, safety, unwinding, and more.

---

## Foreign Function Interface

### Introduction

This guide uses the [snappy](https://github.com/google/snappy) compression/decompression library as an introduction to writing bindings for foreign code. Rust is currently unable to call directly into a C++ library, but snappy includes a C interface (documented in [`snappy-c.h`](https://github.com/google/snappy/blob/master/snappy-c.h)).

### A note about libc

Many examples use [the `libc` crate](https://crates.io/crates/libc), which provides various type definitions for C types. To use these examples, add `libc` to your `Cargo.toml`:

```toml
[dependencies]
libc = "0.2.0"
```

### Prepare the build script

Because snappy is a static library by default, stdc++ is not linked in the output artifact. To use this foreign library in Rust, manually specify linking stdc++. The easiest way is by setting up a build script.

Edit `Cargo.toml`, inside `package` add `build = "build.rs"`:

```toml
[package]
...
build = "build.rs"
```

Then create `build.rs` at the root of your workspace:

```rust
// build.rs
fn main() {
    println!("cargo:rustc-link-lib=dylib=stdc++"); // This line may be unnecessary for some environments.
    println!("cargo:rustc-link-search=<YOUR SNAPPY LIBRARY PATH>");
}
```

### Calling foreign functions

A minimal example of calling a foreign function:

```rust
use libc::size_t;

#[link(name = "snappy")]
unsafe extern "C" {
    fn snappy_max_compressed_length(source_length: size_t) -> size_t;
}

fn main() {
    let x = unsafe { snappy_max_compressed_length(100) };
    println!("max compressed length of a 100 byte buffer: {}", x);
}
```

The `extern` block is a list of function signatures in a foreign library with the platform's C ABI. The `#[link(...)]` attribute instructs the linker to link against the snappy library.

Foreign functions are assumed to be unsafe, so calls must be wrapped with `unsafe {}` as a promise to the compiler that everything contained within is truly safe. C libraries often expose interfaces that aren't thread-safe, and almost any function taking a pointer argument isn't valid for all possible inputs.

The `extern` block can be extended to cover the entire snappy API:

```rust
use libc::{c_int, size_t};

#[link(name = "snappy")]
unsafe extern "C" {
    fn snappy_compress(input: *const u8,
                       input_length: size_t,
                       compressed: *mut u8,
                       compressed_length: *mut size_t) -> c_int;
    fn snappy_uncompress(compressed: *const u8,
                         compressed_length: size_t,
                         uncompressed: *mut u8,
                         uncompressed_length: *mut size_t) -> c_int;
    fn snappy_max_compressed_length(source_length: size_t) -> size_t;
    fn snappy_uncompressed_length(compressed: *const u8,
                                  compressed_length: size_t,
                                  result: *mut size_t) -> c_int;
    fn snappy_validate_compressed_buffer(compressed: *const u8,
                                         compressed_length: size_t) -> c_int;
}
```

### Creating a safe interface

The raw C API needs to be wrapped to provide memory safety and use higher-level concepts like vectors. A library can expose only the safe, high-level interface and hide unsafe internal details.

Wrapping functions that expect buffers involves using the `slice::raw` module to manipulate Rust's vectors as pointers to memory. Rust's vectors are guaranteed to be a contiguous block of memory where the length is the number of elements currently contained, and capacity is the total size in elements of allocated memory.

```rust
pub fn validate_compressed_buffer(src: &[u8]) -> bool {
    unsafe {
        snappy_validate_compressed_buffer(src.as_ptr(), src.len() as size_t) == 0
    }
}
```

The `validate_compressed_buffer` wrapper uses an `unsafe` block but makes the guarantee that calling it is safe for all inputs by leaving off `unsafe` from the function signature.

For `snappy_compress` and `snappy_uncompress`, a buffer must be allocated to hold the output:

```rust
pub fn compress(src: &[u8]) -> Vec<u8> {
    unsafe {
        let srclen = src.len() as size_t;
        let psrc = src.as_ptr();

        let mut dstlen = snappy_max_compressed_length(srclen);
        let mut dst = Vec::with_capacity(dstlen as usize);
        let pdst = dst.as_mut_ptr();

        snappy_compress(psrc, srclen, pdst, &mut dstlen);
        dst.set_len(dstlen as usize);
        dst
    }
}
```

Decompression is similar:

```rust
pub fn uncompress(src: &[u8]) -> Option<Vec<u8>> {
    unsafe {
        let srclen = src.len() as size_t;
        let psrc = src.as_ptr();

        let mut dstlen: size_t = 0;
        snappy_uncompressed_length(psrc, srclen, &mut dstlen);

        let mut dst = Vec::with_capacity(dstlen as usize);
        let pdst = dst.as_mut_ptr();

        if snappy_uncompress(psrc, srclen, pdst, &mut dstlen) == 0 {
            dst.set_len(dstlen as usize);
            Some(dst)
        } else {
            None // SNAPPY_INVALID_INPUT
        }
    }
}
```

### Destructors

Foreign libraries often hand off ownership of resources to the calling code. When this occurs, use Rust's destructors to provide safety and guarantee resource release, especially in case of a panic.

For more information, see the [Drop trait](https://doc.rust-lang.org/std/ops/trait.Drop.html).

### Calling Rust code from C

#### Rust side

First, create a lib crate. `lib.rs` should have:

```rust
#[unsafe(no_mangle)]
pub extern "C" fn hello_from_rust() {
    println!("Hello from Rust!");
}
```

The `extern "C"` makes this function adhere to the C calling convention. The `no_mangle` attribute turns off Rust's name mangling for a well-defined symbol.

To compile Rust code as a shared library callable from C, add to `Cargo.toml`:

```toml
[lib]
crate-type = ["cdylib"]
```

Run `cargo build` and you're ready.

#### C side

Create a C file to call the `hello_from_rust` function:

```c
extern void hello_from_rust();

int main(void) {
    hello_from_rust();
    return 0;
}
```

Compile with:

```bash
gcc call_rust.c -o call_rust -lrust_from_c -L./target/debug
```

Finally, call Rust from C:

```bash
$ LD_LIBRARY_PATH=./target/debug ./call_rust
Hello from Rust!
```

### Callbacks from C code to Rust functions

Some external libraries use callbacks to report back their current state or intermediate data. Pass functions defined in Rust to an external library by marking the callback function as `extern` with the correct calling convention.

**Rust code:**

```rust
extern fn callback(a: i32) {
    println!("I'm called from C with value {0}", a);
}

#[link(name = "extlib")]
unsafe extern {
   fn register_callback(cb: extern fn(i32)) -> i32;
   fn trigger_callback();
}

fn main() {
    unsafe {
        register_callback(callback);
        trigger_callback(); // Triggers the callback.
    }
}
```

**C code:**

```c
typedef void (*rust_callback)(int32_t);
rust_callback cb;

int32_t register_callback(rust_callback callback) {
    cb = callback;
    return 1;
}

void trigger_callback() {
  cb(7); // Will call callback(7) in Rust.
}
```

### Targeting callbacks to Rust objects

Pass a raw pointer to the object down to the C library, which includes it in the notification. This allows the callback to unsafely access the referenced Rust object.

**Rust code:**

```rust
struct RustObject {
    a: i32,
}

unsafe extern "C" fn callback(target: *mut RustObject, a: i32) {
    println!("I'm called from C with value {0}", a);
    unsafe {
        (*target).a = a;
    }
}

#[link(name = "extlib")]
unsafe extern {
   fn register_callback(target: *mut RustObject,
                        cb: unsafe extern fn(*mut RustObject, i32)) -> i32;
   fn trigger_callback();
}

fn main() {
    let mut rust_object = Box::new(RustObject { a: 5 });

    unsafe {
        register_callback(&mut *rust_object, callback);
        trigger_callback();
    }
}
```

**C code:**

```c
typedef void (*rust_callback)(void*, int32_t);
void* cb_target;
rust_callback cb;

int32_t register_callback(void* callback_target, rust_callback callback) {
    cb_target = callback_target;
    cb = callback;
    return 1;
}

void trigger_callback() {
  cb(cb_target, 7); // Will call callback(&rustObject, 7) in Rust.
}
```

### Asynchronous callbacks

When external libraries spawn their own threads and invoke callbacks from there, access to Rust data structures inside callbacks is especially unsafe. Proper synchronization mechanisms must be used. Rust channels (`std::sync::mpsc`) can forward data from the C thread that invoked the callback into a Rust thread.

If an asynchronous callback targets a special Rust object, no more callbacks must be performed by the C library after the Rust object is destroyed. Achieve this by unregistering the callback in the object's destructor.

### Linking

The `link` attribute on `extern` blocks instructs rustc how to link to native libraries. Two accepted forms:

- `#[link(name = "foo")]`
- `#[link(name = "foo", kind = "bar")]`

Three known types of native libraries:

- **Dynamic** - `#[link(name = "readline")]`
- **Static** - `#[link(name = "my_build_dependency", kind = "static")]`
- **Frameworks** - `#[link(name = "CoreFoundation", kind = "framework")]` (macOS only)

### Unsafe blocks

Some operations (dereferencing raw pointers, calling unsafe functions) are only allowed inside unsafe blocks. Unsafe blocks isolate unsafety and are a promise that it doesn't leak out.

```rust
unsafe fn kaboom(ptr: *const i32) -> i32 { *ptr }
```

### Accessing foreign globals

Declare them in `extern` blocks with the `static` keyword:

```rust
#[link(name = "readline")]
unsafe extern {
    static rl_readline_version: libc::c_int;
}

fn main() {
    println!("You have readline version {} installed.",
             unsafe { rl_readline_version as i32 });
}
```

For mutable globals:

```rust
use std::ffi::CString;
use std::ptr;

#[link(name = "readline")]
unsafe extern {
    static mut rl_prompt: *const libc::c_char;
}

fn main() {
    let prompt = CString::new("[my-awesome-shell] $").unwrap();
    unsafe {
        rl_prompt = prompt.as_ptr();
        println!("{:?}", rl_prompt);
        rl_prompt = ptr::null();
    }
}
```

### Foreign calling conventions

Most foreign code exposes a C ABI. Rust provides a way to specify other conventions:

```rust
#[cfg(all(target_os = "win32", target_arch = "x86"))]
#[link(name = "kernel32")]
unsafe extern "stdcall" {
    fn SetEnvironmentVariableA(n: *const u8, v: *const u8) -> libc::c_int;
}
```

Supported ABI constraints: `stdcall`, `aapcs`, `cdecl`, `fastcall`, `thiscall`, `vectorcall`, `Rust`, `system`, `C`, `win64`, `sysv64`.

The `system` ABI selects the appropriate ABI for interoperating with the target's libraries.

### Interoperability with foreign code

Rust guarantees `struct` layout is compatible with C's representation only if `#[repr(C)]` is applied. `#[repr(C, packed)]` lays out struct members without padding.

Key points:
- Rust's owned boxes (`Box<T>`) use non-nullable pointers
- References are safely non-nullable pointers
- Vectors and strings share basic memory layout
- Strings aren't NUL-terminated; use `CString` from `std::ffi` for C interop
- The `libc` crate includes type aliases and function definitions for the C standard library

### Variadic functions

In C, functions can be 'variadic'. Specify with `...` in foreign function declarations:

```rust
unsafe extern {
    fn foo(x: i32, ...);
}

fn main() {
    unsafe {
        foo(10, 20, 30, 40, 50);
    }
}
```

Normal Rust functions cannot be variadic.

### The "nullable pointer optimization"

Certain Rust types are defined to never be `null`: references (`&T`, `&mut T`), boxes (`Box<T>`), and function pointers (`extern "abi" fn()`).

As a special case, an `enum` with exactly two variants (one with no data, the other containing a non-nullable type) uses no extra space for a discriminant. The empty variant is represented by `null`.

The most common type using this is `Option<T>`, where `None` corresponds to `null`. So `Option<extern "C" fn(c_int) -> c_int>` correctly represents a nullable function pointer using the C ABI.

### FFI and unwinding

Most ABI strings come in two variants: one with `-unwind` suffix and one without.

If Rust `panic`s or foreign exceptions cross an FFI boundary, that boundary must use the appropriate `-unwind` ABI string. If unwinding shouldn't cross an ABI boundary, use non-`unwind` ABI strings.

#### Catching `panic` preemptively

Use `catch_unwind` to prevent panics from crossing FFI boundaries:

```rust
use std::panic::catch_unwind;

#[unsafe(no_mangle)]
pub extern "C" fn oh_no() -> i32 {
    let result = catch_unwind(|| {
        panic!("Oops!");
    });
    match result {
        Ok(_) => 0,
        Err(_) => 1,
    }
}
```

#### Rust `panic` with `"C-unwind"`

```rust
#[unsafe(no_mangle)]
unsafe extern "C-unwind" fn example() {
    panic!("Uh oh");
}
```

When compiled with `panic=unwind`, this function is permitted to unwind C++ stack frames.

#### `panic` stopped at an ABI boundary

```rust
#[unsafe(no_mangle)]
extern "C" fn assert_nonzero(input: u32) {
    assert!(input != 0)
}
```

If called with `0`, the runtime is guaranteed to safely abort the process.

### Representing opaque structs

For C libraries that provide pointers without revealing internal details, create opaque types in Rust:

```rust
#[repr(C)]
pub struct Foo {
    _data: (),
    _marker:
        core::marker::PhantomData<(*mut u8, core::marker::PhantomPinned)>,
}

unsafe extern "C" {
    pub fn foo(arg: *mut Foo);
}
```

By including at least one private field and no constructor, create an opaque type that can't be instantiated outside this module. Use `#[repr(C)]` for FFI. The marker ensures the compiler doesn't mark the struct as `Send`, `Sync`, and `Unpin`.

**Do NOT use empty enums as FFI types.** The compiler relies on empty enums being uninhabited, causing undefined behavior when handling `&Empty` values.
