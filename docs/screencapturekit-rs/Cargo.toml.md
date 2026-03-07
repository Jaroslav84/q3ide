# screencapturekit-rs - Cargo.toml Reference

> **Source:** https://github.com/svtlabs/screencapturekit-rs/blob/main/screencapturekit/Cargo.toml
> **Supplemental sources:** https://crates.io/crates/screencapturekit, https://docs.rs/crate/screencapturekit/1.5.0
> **Fetched:** 2026-03-07
>
> **Note:** The raw Cargo.toml file could not be fetched directly due to GitHub rate limits.
> This document is reconstructed from crates.io metadata, docs.rs build info, and README documentation.

## Package Metadata

```toml
[package]
name = "screencapturekit"
version = "1.5.3"  # Latest successful build as of 2026-03-05
license = "Apache-2.0 OR MIT"
repository = "https://github.com/doom-fish/screencapturekit-rs"
description = "Safe, idiomatic Rust bindings for Apple's ScreenCaptureKit framework"
edition = "2021"
```

## Dependencies

### Runtime Dependencies

**None.** The crate has zero runtime dependencies.

The main `screencapturekit` crate depends on its internal sys crate:

```toml
[dependencies]
screencapturekit-sys = { path = "../screencapturekit-sys", version = "..." }
```

### Dev Dependencies (for examples and tests only)

```toml
[dev-dependencies]
bevy = "0.15"
criterion = "0.8"
eframe = "0.30"
png = "0.18"
pollster = "0.4"
raw-window-handle = "0.6"
tokio = "1"
wgpu = "24"
winit = "0.30"
```

## Feature Flags

```toml
[features]
# Async support (runtime-agnostic)
async = []

# macOS version-gated features (cumulative)
macos_13_0 = []              # Audio capture, sync clock
macos_14_0 = ["macos_13_0"]  # Content picker, screenshots
macos_14_2 = ["macos_14_0"]  # Menu bar, child windows, presenter overlay
macos_14_4 = ["macos_14_2"]  # Current process shareable content
macos_15_0 = ["macos_14_4"]  # Recording output, HDR, microphone
macos_15_2 = ["macos_15_0"]  # Screenshot in rect, stream delegates
macos_26_0 = ["macos_15_2"]  # Advanced screenshot config
```

**Note:** macOS version features are cumulative -- enabling `macos_15_0` automatically enables `macos_14_4`, `macos_14_2`, `macos_14_0`, and `macos_13_0`.

## Usage in Cargo.toml

### Basic (synchronous only)
```toml
[dependencies]
screencapturekit = "1"
```

### With async support
```toml
[dependencies]
screencapturekit = { version = "1", features = ["async"] }
```

### With macOS 14.0 features (screenshots, content picker)
```toml
[dependencies]
screencapturekit = { version = "1", features = ["macos_14_0"] }
```

### With macOS 15.0 features (recording, HDR, microphone)
```toml
[dependencies]
screencapturekit = { version = "1", features = ["macos_15_0"] }
```

### Full features (async + latest macOS)
```toml
[dependencies]
screencapturekit = { version = "1", features = ["async", "macos_26_0"] }
```

## Platform Requirements

- **Minimum macOS:** 12.3 (Monterey)
- **Recommended:** 14.0+ (Sonoma) for full feature support
- **Architecture:** macOS-only (requires Xcode Command Line Tools)
- **Permission:** Screen Recording access required (System Preferences -> Privacy & Security)

## Build Information

- **Source Code Size:** 1.07 MB
- **Average Build Duration:** 15 seconds
- **Rust Edition:** 2021

## Workspace Structure

The repository is a Cargo workspace with the following members:

```toml
[workspace]
members = [
    "screencapturekit",
    "screencapturekit-sys",
]
```
