// Simple test to verify screen capture works
use std::time::Duration;

fn main() {
    println!("Testing ScreenCaptureKit...");
    
    // Initialize logging
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info"))
        .init();
    
    // Try to get shareable content
    match screencapturekit::prelude::SCShareableContent::get() {
        Ok(content) => {
            println!("✓ Got shareable content");
            let windows = content.windows();
            println!("  Found {} windows", windows.len());
            
            for (i, w) in windows.iter().filter(|w| w.is_on_screen()).take(5).enumerate() {
                let title = w.title().unwrap_or_default();
                let app = w.owning_application()
                    .map(|a| a.application_name())
                    .unwrap_or_default();
                println!("  [{}] {} - {} (wid={})", i, app, title, w.window_id());
            }
        }
        Err(e) => {
            println!("✗ Failed to get shareable content: {}", e);
            println!("\nThis usually means Screen Recording permission is not granted.");
            std::process::exit(1);
        }
    }
    
    println!("\nCapture test complete!");
}
