# cbindgen - C/C++ Header Generator for Rust

> **Source:** <https://github.com/mozilla/cbindgen>
> **Description:** README and overview of cbindgen, a tool that generates C/C++11 header files from Rust libraries exposing a public C API. Used by Mozilla Firefox (webrender, stylo) and other production projects.

---

## Overview

**cbindgen** creates C/C++11 headers for Rust libraries which expose a public C API. Machine-generated headers are based on your actual Rust code, and cbindgen developers have worked closely with Rust developers to ensure the headers reflect actual guarantees about Rust's type layout and ABI.

## Key Features

- **Dual Language Support**: Generate both C++ and C headers from the same Rust library
- **C++ Ergonomics**: Leverages operator overloads, constructors, enum classes, and templates to make the API more ergonomic and Rust-like
- **Flexibility**: Can be used as a standalone CLI program or integrated into `build.rs`
- **Minimal Dependencies**: cbindgen is a simple Rust library with no interesting dependencies
- **Production-Proven**: Used by Mozilla Firefox (webrender, stylo), maturin, tquic, metatensor, and others

## Installation

### Via Cargo

```bash
cargo install --force cbindgen
```

### Via Homebrew (macOS)

```bash
brew install cbindgen
```

## Quick Start

### Requirements

- A `cbindgen.toml` configuration file (can be empty initially)
- A Rust crate with a public C API using `#[no_mangle]` and `extern "C"`

### Basic Usage

Generate a C++ header (default):

```bash
cbindgen --config cbindgen.toml --crate my_rust_library --output my_header.h
```

Generate a C header:

```bash
cbindgen --config cbindgen.toml --crate my_rust_library --output my_header.h --lang c
```

Use `cbindgen --help` for additional options.

## Using cbindgen in build.rs

You can integrate cbindgen directly into your build process:

```rust
// build.rs
extern crate cbindgen;

use std::env;

fn main() {
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file("target/include/my_library.h");
}
```

Or using a configuration file:

```rust
// build.rs
extern crate cbindgen;

use std::env;
use std::path::PathBuf;

fn main() {
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let config = cbindgen::Config::from_file("cbindgen.toml")
        .expect("Unable to find cbindgen.toml");

    cbindgen::generate_with_config(&crate_dir, config)
        .expect("Unable to generate bindings")
        .write_to_file("target/include/my_library.h");
}
```

## What cbindgen Generates

Given Rust code like:

```rust
#[repr(C)]
pub struct Point {
    pub x: f64,
    pub y: f64,
}

#[repr(C)]
pub enum Color {
    Red,
    Green,
    Blue,
}

#[no_mangle]
pub extern "C" fn draw_point(point: Point, color: Color) {
    // ...
}
```

cbindgen generates a C header:

```c
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    double x;
    double y;
} Point;

typedef enum {
    Red,
    Green,
    Blue,
} Color;

void draw_point(Point point, Color color);
```

Or a C++ header with enum classes, namespaces, and operator overloads.

## Documentation

- **Full User Docs**: [docs.md](https://github.com/mozilla/cbindgen/blob/main/docs.md)
- **Configuration Template**: [template.toml](https://github.com/mozilla/cbindgen/blob/main/template.toml)
- **API Reference**: [docs.rs/cbindgen](https://docs.rs/cbindgen)

## Supported Rust Constructs

cbindgen can generate headers for:

- Functions with `#[no_mangle]` and `extern "C"`
- Structs with `#[repr(C)]`
- Enums with `#[repr(C)]` or `#[repr(u8)]`, etc.
- Type aliases
- Constants and statics
- Opaque types (generates forward declarations)
- Generic types (generates C++ templates or specialized C structs)

## Limitations

- Cannot handle all Rust types (e.g., trait objects, closures)
- Development has been largely adhoc, as features have been added to support the use cases of the maintainers
- Certain scenarios may lack support until someone contributes implementation work

## License

Mozilla Public License 2.0 (MPL-2.0)
