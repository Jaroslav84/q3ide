# The `notify` Crate: File System Watching for Q3IDE Batch 10/11

Reference for Q3IDE's Rust file-watcher daemon using FSEvents on macOS.

## Cargo.toml Setup

```toml
[dependencies]
notify = "8.2"
notify-debouncer-mini = "0.4"
crossbeam = "0.8"
```

- **notify 8.2**: Latest stable, with macOS FSEvents support by default
- **notify-debouncer-mini**: Lightweight debouncing to avoid duplicate events on file save
- **crossbeam**: Channel primitives for daemon loop communication

## Watcher Selection

### RecommendedWatcher (Use This)
- Auto-selects best backend per platform
- macOS: FSEvents (efficient, system-level notifications)
- Linux: inotify
- Windows: ReadDirectoryChangesW

```rust
use notify::recommended_watcher;
let watcher = recommended_watcher(event_handler)?;
```

### PollWatcher (Not For This Project)
- Polling-based fallback for network filesystems
- Higher CPU, worse latency — avoid for source monitoring

## Basic Setup with Debouncer

```rust
use notify_debouncer_mini::new_debouncer;
use notify::RecursiveMode;
use std::path::Path;
use std::time::Duration;

// Create debouncer with 500ms threshold
let (tx, rx) = crossbeam::channel::unbounded();
let mut debouncer = new_debouncer(Duration::from_millis(500), tx)?;

// Watch source directories
debouncer.watcher()
    .watch(Path::new("quake3e/code"), RecursiveMode::Recursive)?;
debouncer.watcher()
    .watch(Path::new("capture/src"), RecursiveMode::Recursive)?;

// Daemon loop
while let Ok(result) = rx.recv() {
    match result {
        Ok(events) => {
            for event in events {
                if should_rebuild(&event.path) {
                    // Trigger UML cache update
                }
            }
        }
        Err(e) => eprintln!("Watch error: {}", e),
    }
}
```

## Event Types & Filtering

```rust
use notify_debouncer_mini::DebouncedEvent;
use notify::EventKind;

// DebouncedEvent fields
for event in events {
    match event.kind {
        EventKind::Create(_) => println!("File created"),
        EventKind::Modify(_) => println!("File modified"),
        EventKind::Remove(_) => println!("File removed"),
        _ => {}
    }

    let path = &event.path;
}

// Filter helper
fn should_rebuild(path: &Path) -> bool {
    matches!(path.extension().and_then(|s| s.to_str()),
        Some("c" | "h" | "rs"))
}
```

## Why Use Debouncer?

Without debouncing, a single file save can emit 5-10 events:
- Create (for temp file)
- Modify (bulk write)
- Modify (metadata)
- Remove (temp)
- Rename/Move

**notify-debouncer-mini** coalesces these into one `DebouncedEvent` per file within the 500ms window.

### Debouncer vs Raw notify

| Feature | Raw notify | Debouncer-mini |
|---------|-----------|-----------------|
| Event per save | 5–10 | 1 |
| Setup | Direct watcher | `new_debouncer(Duration, tx)` |
| Performance | Higher CPU | Lower CPU |
| Latency | Instant | 500ms delay |

For UML cache updates, 500ms debounce is acceptable; avoids thrashing.

## Crossbeam Channel Integration

```rust
use crossbeam::channel::{unbounded, Sender};

// In main
let (tx, rx) = unbounded();

// Pass to debouncer
let mut debouncer = new_debouncer(Duration::from_millis(500), tx)?;

// Daemon thread or loop
std::thread::spawn(move || {
    while let Ok(result) = rx.recv() {
        match result {
            Ok(events) => process_events(events),
            Err(e) => eprintln!("Error: {}", e),
        }
    }
});
```

- `unbounded()`: No queue limit, best for file events
- `recv()`: Blocks until event, safe across threads
- `send()`: Used internally by debouncer

## Error Handling

```rust
use notify::Error as NotifyError;

match debouncer.watcher().watch(path, RecursiveMode::Recursive) {
    Ok(()) => println!("Watching {}", path.display()),
    Err(NotifyError::PathNotFound) => eprintln!("Path not found: {}", path.display()),
    Err(NotifyError::Io(e)) => eprintln!("I/O error: {}", e),
    Err(e) => eprintln!("Watch error: {}", e),
}

// Handle event errors in loop
match rx.recv() {
    Ok(Ok(events)) => {}, // Success
    Ok(Err(e)) => eprintln!("Event error: {}", e), // Event processing failed
    Err(_) => break, // Channel closed (shutdown)
}
```

## macOS FSEvents Specifics

FSEvents is the native macOS backend, selected by default.

```rust
// No special config needed — just use RecommendedWatcher
let mut watcher = recommended_watcher(|res| match res {
    Ok(_) => {},
    Err(e) => eprintln!("FSEvents error: {}", e),
})?;
```

**Latency**: FSEvents batches events and may deliver them in clusters. Debouncer handles this by coalescing within the timeout window.

**Security**: FSEvents cannot observe files not owned by the process. For Q3IDE (user-owned source files), not an issue.

## Complete Example: Minimal File Watcher

```rust
use notify_debouncer_mini::new_debouncer;
use notify::RecursiveMode;
use std::path::Path;
use std::time::Duration;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let (tx, rx) = crossbeam::channel::unbounded();

    let mut debouncer = new_debouncer(
        Duration::from_millis(500),
        tx,
    )?;

    let watch_paths = vec!["quake3e/code", "capture/src"];
    for path in watch_paths {
        debouncer.watcher()
            .watch(Path::new(path), RecursiveMode::Recursive)?;
    }

    println!("Watching for changes...");
    loop {
        match rx.recv() {
            Ok(Ok(events)) => {
                for event in events {
                    if is_source_file(&event.path) {
                        println!("Update: {}", event.path.display());
                        // Trigger UML cache rebuild
                    }
                }
            }
            Ok(Err(e)) => eprintln!("Event error: {}", e),
            Err(_) => break,
        }
    }
    Ok(())
}

fn is_source_file(path: &Path) -> bool {
    matches!(path.extension().and_then(|s| s.to_str()),
        Some("c" | "h" | "rs"))
}
```

## References

- [notify docs.rs](https://docs.rs/notify/latest/notify/)
- [notify-debouncer-mini docs.rs](https://docs.rs/notify-debouncer-mini/latest/notify_debouncer_mini/)
- [GitHub: notify-rs/notify](https://github.com/notify-rs/notify)
