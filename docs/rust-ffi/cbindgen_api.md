# cbindgen API Reference

> **Source:** <https://docs.rs/cbindgen/latest/cbindgen/>
> **Description:** API documentation for the `cbindgen` crate (v0.29.2), a tool for generating C bindings to Rust code. Licensed under MPL-2.0.

---

## Overview

The `cbindgen` crate provides both a CLI tool and a library API for generating C/C++ header files from Rust source code. When used as a library (typically in `build.rs`), it offers programmatic control over the generation process.

## Core Structs

### `Builder`

The main entry point for generating bindings programmatically. Facilitates configuration and generation of bindings headers.

```rust
use cbindgen;

let bindings = cbindgen::Builder::new()
    .with_crate(crate_dir)
    .with_config(config)
    .generate()
    .expect("Unable to generate bindings");

bindings.write_to_file("output.h");
```

Key methods:
- `new()` - Create a new Builder
- `with_crate(path)` - Set the crate directory to parse
- `with_config(config)` - Set the configuration
- `with_src(path)` - Add a source file to parse
- `with_language(Language)` - Set output language
- `with_include_guard(name)` - Set include guard name
- `with_namespace(ns)` - Set C++ namespace
- `with_no_includes()` - Disable default includes
- `with_sys_include(header)` - Add a system include
- `with_include(header)` - Add a local include
- `with_after_include(line)` - Add content after includes
- `with_tab_width(width)` - Set indentation width
- `with_header(header)` - Set header comment text
- `with_trailer(trailer)` - Set trailer text
- `with_autogen_warning(warning)` - Set auto-generation warning
- `with_item_prefix(prefix)` - Set prefix for all items
- `with_parse_deps(enabled)` - Enable/disable dependency parsing
- `with_parse_include(crate_names)` - Whitelist crates for parsing
- `with_parse_exclude(crate_names)` - Blacklist crates from parsing
- `with_parse_expand(crate_names)` - Expand macros from crates
- `with_documentation(enabled)` - Enable/disable doc comments
- `with_target(target)` - Set Rust target triple
- `with_define(key, value, cfg)` - Add a define
- `generate()` - Generate bindings (returns `Result<Bindings, Error>`)

### `Bindings`

Represents a bindings header ready for output.

Key methods:
- `write_to_file(path)` - Write the header to a file
- `write(writer)` - Write the header to any `Write` implementor

### `Config`

Central configuration object for customizing generated bindings. All options from `cbindgen.toml` are available as fields.

Key methods:
- `from_file(path)` - Load config from a TOML file
- `from_root_or_default(path)` - Load from path or use defaults

### Specialized Config Structs

- **`ConstantConfig`** - Customization settings for generated constants
- **`CythonConfig`** - Cython-specific binding settings
- **`EnumConfig`** - Settings for generated enum output (rename rules, variant body style, prefix/postfix stripping)
- **`ExportConfig`** - Controls item export behavior (include/exclude lists, prefix, renaming)
- **`FunctionConfig`** - Settings for generated functions (rename rules, argument naming, Swift name annotations)
- **`LayoutConfig`** - Configuration for types with layout modifiers (packed, aligned)
- **`MacroExpansionConfig`** - Custom macro expansion settings (bitflags support)
- **`MangleConfig`** - Name mangling configuration
- **`ParseConfig`** - Parsing behavior settings (dependency parsing, expand, include/exclude)
- **`ParseExpandConfig`** - Settings for `rustc -Zunpretty=expanded` execution
- **`PtrConfig`** - Pointer-specific configuration (non-null annotations)
- **`StructConfig`** - Settings for generated struct output (rename rules, derive helpers, associated constants)

## Enumerations

### `Language`

Target language selection:
- `C` - Generate C headers
- `Cxx` - Generate C++ headers (default)
- `Cython` - Generate Cython bindings

### `Style`

Struct and enum generation styling:
- `Both` - Generate both typedef and named struct/enum
- `Tag` - Generate only tagged struct/enum
- `Type` - Generate only typedef

### `Braces`

Brace style options:
- `SameLine` - Opening brace on same line
- `NextLine` - Opening brace on next line

### `RenameRule`

Identifier transformation rules:
- `None` - No renaming
- `GeckoCase` - Mozilla Gecko style
- `LowerCase` - all_lowercase
- `UpperCase` - ALL_UPPERCASE
- `PascalCase` - PascalCase
- `CamelCase` - camelCase
- `SnakeCase` - snake_case
- `ScreamingSnakeCase` - SCREAMING_SNAKE_CASE
- `QualifiedScreamingSnakeCase` - QUALIFIED_SCREAMING_SNAKE_CASE

### `SortKey`

Function ordering specification:
- `Name` - Sort by name
- `None` - Preserve source order

### `DocumentationStyle`

Comment formatting for documentation:
- `C` - `/* ... */` style
- `C99` - `// ...` style
- `Cxx` - `/// ...` style
- `Doxy` - Doxygen style
- `Auto` - Automatically choose based on target language

### `DocumentationLength`

Controls documentation verbosity:
- `Full` - Include all documentation
- `Short` - Include only first line

### `Layout`

Line formatting approach:
- `Horizontal` - Prefer horizontal layout
- `Vertical` - Prefer vertical layout
- `Auto` - Automatically choose

### `LineEndingStyle`

Line ending convention:
- `LF` - Unix-style line endings
- `CR` - Classic Mac-style line endings
- `CRLF` - Windows-style line endings
- `Native` - Use platform native line endings

### `Profile`

Cargo profile selection for macro expansion:
- `Debug` - Use debug profile
- `Release` - Use release profile

### `ItemType`

Categorizes different generatable items:
- `Constants`
- `Globals`
- `Enums`
- `Structs`
- `Unions`
- `Typedefs`
- `OpaqueItems`
- `Functions`

### `Error`

Error type for the crate, covering parse errors, IO errors, and other failure modes.

## Primary Functions

### `generate()`

Build script utility that generates bindings using optional `cbindgen.toml` configuration found in the crate root:

```rust
// build.rs
fn main() {
    cbindgen::generate(std::env::var("CARGO_MANIFEST_DIR").unwrap())
        .expect("Unable to generate bindings")
        .write_to_file("bindings.h");
}
```

### `generate_with_config()`

Build script utility with explicit custom configuration:

```rust
// build.rs
fn main() {
    let config = cbindgen::Config {
        language: cbindgen::Language::C,
        ..Default::default()
    };

    cbindgen::generate_with_config(
        std::env::var("CARGO_MANIFEST_DIR").unwrap(),
        config,
    )
    .expect("Unable to generate bindings")
    .write_to_file("bindings.h");
}
```

## Platform Support

The crate supports multiple platform targets including:
- `aarch64-apple-darwin` (Apple Silicon macOS)
- `aarch64-unknown-linux-gnu` (ARM64 Linux)
- `x86_64-apple-darwin` (Intel macOS)
- `x86_64-unknown-linux-gnu` (Intel Linux)
- `x86_64-pc-windows-msvc` (Windows)
- `i686-unknown-linux-gnu` (32-bit Linux)

## Documentation Coverage

Approximately 75.49% of the crate API is documented.
