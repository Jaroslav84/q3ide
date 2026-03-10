/// CaptureRouter — decides capture mode (COMPOSITE vs DEDICATED) for each window.
///
/// COMPOSITE: one SCStream per physical display, shared by all windows on it.
///   Crop is done in get_frame(). No per-app camera icon. No stream count limit.
///   Works for CPU-rendered apps (terminals, text editors, most native AppKit apps).
///
/// DEDICATED: one SCStream per window via desktopIndependentWindow filter.
///   Always correct — captures the window's own GPU layers in isolation.
///   Required for hardware-accelerated apps (browsers, Electron, Metal rendering).
///
/// Resolution order:
///   1. WHITELIST_COMPOSITE match → COMPOSITE immediately, no detector needed.
///   2. WHITELIST_DEDICATED match → DEDICATED immediately.
///   3. Unknown app             → COMPOSITE by default, detector watches frames.
///
/// Detector (in SCKBackend::get_frame):
///   Counts consecutive empty/dark frames for composite windows.
///   At DETECTOR_EMPTY_THRESHOLD, logs a warning with a switch recommendation.
///   Auto-switch is Phase 2.

// ══════════════════════════════════════════════════════════════════════════════
// WHITELIST_COMPOSITE — CPU-rendered apps, safe for display composite.
// Add app name substrings (case-insensitive).
// ══════════════════════════════════════════════════════════════════════════════
pub const WHITELIST_COMPOSITE: &[&str] = &[
    "iterm",       // iTerm2
    "terminal",    // macOS Terminal.app
    "alacritty",   // Alacritty terminal
    "kitty",       // kitty terminal
    "wezterm",     // WezTerm terminal
    "ghostty",     // Ghostty terminal
    "hyper",       // Hyper terminal (Electron, but CPU-rendered text)
    "tabby",       // Tabby terminal
    "xterm",       // xterm / uxterm
    "cool-retro",  // cool-retro-term
];

// ══════════════════════════════════════════════════════════════════════════════
// WHITELIST_DEDICATED — GPU/hardware-accelerated apps, require dedicated stream.
// Add app name substrings (case-insensitive).
// ══════════════════════════════════════════════════════════════════════════════
pub const WHITELIST_DEDICATED: &[&str] = &[
    // Browsers
    "safari",
    "chrome",
    "firefox",
    "arc",
    "brave",
    "edge",
    "opera",
    "vivaldi",
    "dia",
    // IDEs / code editors (GPU-composited)
    "xcode",
    "cursor",
    "windsurf",
    "zed",
    "intellij",
    "webstorm",
    "goland",
    "clion",
    "rider",
    "rubymine",
    "fleet",       // JetBrains Fleet
    "sourcetree",  // custom-sized, GPU-composited
    // Communication (Electron with GPU layers)
    "slack",
    "discord",
    "zoom",
    "teams",
    "webex",
    "loom",
    // Creative / design
    "figma",
    "sketch",
    "affinity",
    // Media
    "spotify",
    "vlc",
    "iina",
    // Productivity (Electron)
    "notion",
    "obsidian",
    "linear",
    "asana",
    "jira",
];

// ══════════════════════════════════════════════════════════════════════════════
// DETECTOR thresholds — tune here if detector is too sensitive / slow to react.
// ══════════════════════════════════════════════════════════════════════════════

/// Consecutive empty-crop frames before detector fires.
/// Empty crop = display stream is live but window rect produced zero pixels
/// (coordinates mismatch or window moved off-screen).
pub const DETECTOR_EMPTY_THRESHOLD: u32 = 20;

/// Consecutive dark frames before detector fires.
/// Dark = crop succeeded but all sampled pixels are near-black — GPU content
/// not composited into the display frame (Safari-class apps).
pub const DETECTOR_DARK_THRESHOLD: u32 = 30;

/// Pixel brightness threshold for "dark" detection (per channel, 0-255).
/// If all sampled pixels have R, G, B all below this value → dark frame.
pub const DETECTOR_DARK_PIXEL_MAX: u8 = 12;

// ──────────────────────────────────────────────────────────────────────────────

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CaptureMode {
    /// Shared display stream + crop per window.
    Composite,
    /// Isolated per-window SCStream (desktopIndependentWindow).
    Dedicated,
}

pub struct CaptureRouter;

impl CaptureRouter {
    /// Resolve initial capture mode from app name.
    /// Called once at attach time — O(whitelist_len) string search.
    pub fn resolve(app_name: &str) -> CaptureMode {
        let lower = app_name.to_lowercase();

        if WHITELIST_COMPOSITE.iter().any(|&k| lower.contains(k)) {
            log::info!("router: '{}' → COMPOSITE (whitelist_composite)", app_name);
            return CaptureMode::Composite;
        }

        if WHITELIST_DEDICATED.iter().any(|&k| lower.contains(k)) {
            log::info!("router: '{}' → DEDICATED (whitelist_dedicated)", app_name);
            return CaptureMode::Dedicated;
        }

        // Unknown app: start composite, detector will watch.
        log::info!("router: '{}' → COMPOSITE (unknown, detector active)", app_name);
        CaptureMode::Composite
    }

    /// Sample up to 5 pixels spread across the frame.
    /// Returns true if all sampled pixels are "dark" (likely GPU black frame).
    pub fn is_dark_frame(pixels: &[u8], width: u32, height: u32, stride: u32) -> bool {
        if pixels.is_empty() || width == 0 || height == 0 {
            return true;
        }

        // Sample center + 4 quadrant centers (inset 25%).
        let sample_uvs: [(f32, f32); 5] = [
            (0.5, 0.5),
            (0.25, 0.25),
            (0.75, 0.25),
            (0.25, 0.75),
            (0.75, 0.75),
        ];

        for (u, v) in &sample_uvs {
            let x = ((u * width as f32) as u32).min(width - 1);
            let y = ((v * height as f32) as u32).min(height - 1);
            let off = (y * stride + x * 4) as usize;
            if off + 2 >= pixels.len() {
                continue;
            }
            // BGRA layout: B=off+0 G=off+1 R=off+2
            let b = pixels[off];
            let g = pixels[off + 1];
            let r = pixels[off + 2];
            if r > DETECTOR_DARK_PIXEL_MAX || g > DETECTOR_DARK_PIXEL_MAX || b > DETECTOR_DARK_PIXEL_MAX {
                return false; // Found a bright pixel — not dark
            }
        }

        true // All sampled pixels were dark
    }
}
