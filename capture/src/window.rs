/// Window enumeration utilities.
///
/// Provides helper functions for finding windows by title substring,
/// used by the `/q3ide_attach <window_title>` console command.

use crate::backend::WindowInfo;

/// Find windows whose title contains the given substring (case-insensitive).
pub fn find_windows_by_title<'a>(
    windows: &'a [WindowInfo],
    query: &str,
) -> Vec<&'a WindowInfo> {
    let query_lower = query.to_lowercase();
    windows
        .iter()
        .filter(|w| {
            w.title.to_lowercase().contains(&query_lower)
                || w.app_name.to_lowercase().contains(&query_lower)
        })
        .collect()
}

/// Format a window list for console display.
pub fn format_window_list(windows: &[WindowInfo]) -> String {
    if windows.is_empty() {
        return "No windows found.".to_string();
    }

    let mut out = String::new();
    out.push_str(&format!("{:<6} {:<20} {:<30} {:>10}\n", "ID", "App", "Title", "Size"));
    out.push_str(&"-".repeat(70));
    out.push('\n');

    for w in windows {
        if !w.is_on_screen {
            continue;
        }
        let title = if w.title.len() > 28 {
            format!("{}...", &w.title[..25])
        } else {
            w.title.clone()
        };
        out.push_str(&format!(
            "{:<6} {:<20} {:<30} {:>4}x{:<4}\n",
            w.window_id,
            truncate(&w.app_name, 18),
            title,
            w.width,
            w.height,
        ));
    }

    out
}

fn truncate(s: &str, max: usize) -> String {
    if s.len() > max {
        format!("{}...", &s[..max.saturating_sub(3)])
    } else {
        s.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_win(id: u32, app: &str, title: &str) -> WindowInfo {
        WindowInfo {
            window_id: id,
            title: title.to_string(),
            app_name: app.to_string(),
            width: 800,
            height: 600,
            is_on_screen: true,
        }
    }

    #[test]
    fn test_find_by_title() {
        let windows = vec![
            make_win(1, "iTerm2", "~/Projects/q3ide — zsh"),
            make_win(2, "Safari", "Google"),
            make_win(3, "iTerm2", "~/Documents — bash"),
        ];

        let found = find_windows_by_title(&windows, "iterm");
        assert_eq!(found.len(), 2);

        let found = find_windows_by_title(&windows, "q3ide");
        assert_eq!(found.len(), 1);
        assert_eq!(found[0].window_id, 1);
    }
}
