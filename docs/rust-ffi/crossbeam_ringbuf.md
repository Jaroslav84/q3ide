# Crossbeam ArrayQueue as a Ring Buffer

> **Sources:**
> - <https://docs.rs/crossbeam/latest/crossbeam/queue/struct.ArrayQueue.html> (official API docs)
> - <https://docs.rs/crossbeam-queue/latest/crossbeam_queue/struct.ArrayQueue.html> (crossbeam-queue docs)
> - <https://github.com/crossbeam-rs/crossbeam/blob/master/crossbeam-queue/src/array_queue.rs> (source code)
> - <https://github.com/crossbeam-rs/crossbeam/commit/bd75c3c45edb78a731956c01458b75e5b69a8146> (force_push commit)
> - <https://blog.logrocket.com/concurrent-programming-rust-crossbeam/> (tutorial)
>
> **Description:** How to use crossbeam's `ArrayQueue` with `force_push` as a lock-free ring buffer for concurrent producer-consumer patterns, directly applicable to the q3ide-capture frame buffer.

---

## Overview

`ArrayQueue<T>` from the `crossbeam` crate is a bounded, lock-free, multi-producer multi-consumer (MPMC) queue. It pre-allocates a fixed-capacity buffer at construction time, making it faster than dynamically-growing alternatives like `SegQueue`.

The key feature for ring buffer usage is `force_push`, which overwrites the oldest element when the queue is full, enabling classic ring buffer semantics without blocking.

## Cargo.toml

```toml
[dependencies]
crossbeam = "0.8"

# Or use just the queue sub-crate:
# crossbeam-queue = "0.3"
```

## API Reference

### Constructor

```rust
use crossbeam::queue::ArrayQueue;

// Create a queue with capacity for 64 elements
// Panics if cap is 0
let q: ArrayQueue<u8> = ArrayQueue::new(64);
```

### Standard Push (Rejects When Full)

```rust
let q = ArrayQueue::new(2);

assert_eq!(q.push('a'), Ok(()));
assert_eq!(q.push('b'), Ok(()));
assert_eq!(q.push('c'), Err('c'));  // Queue is full, returns the value back
```

### Force Push (Ring Buffer Mode)

```rust
let q = ArrayQueue::new(2);

assert_eq!(q.force_push(10), None);     // Queue: [10]
assert_eq!(q.force_push(20), None);     // Queue: [10, 20]
assert_eq!(q.force_push(30), Some(10)); // Queue: [20, 30], returns evicted 10

assert_eq!(q.pop(), Some(20));
assert_eq!(q.pop(), Some(30));
assert_eq!(q.pop(), None);
```

`force_push` returns:
- `None` if the push succeeded without eviction (space was available)
- `Some(old_value)` if the push succeeded by evicting the oldest element

### Pop

```rust
let q = ArrayQueue::new(2);
q.push(1).unwrap();
q.push(2).unwrap();

assert_eq!(q.pop(), Some(1));  // FIFO order
assert_eq!(q.pop(), Some(2));
assert_eq!(q.pop(), None);     // Empty
```

### Capacity and Length Queries

```rust
let q = ArrayQueue::new(100);
q.push(1).unwrap();
q.push(2).unwrap();

assert_eq!(q.capacity(), 100);
assert_eq!(q.len(), 2);
assert_eq!(q.is_empty(), false);
assert_eq!(q.is_full(), false);
```

## Ring Buffer Pattern for Frame Capture

This is the pattern most relevant to q3ide-capture, where a producer thread captures screen frames and a consumer thread (the engine) reads the latest frame:

```rust
use crossbeam::queue::ArrayQueue;
use std::sync::Arc;
use std::thread;
use std::time::Duration;

/// Represents a captured frame
struct Frame {
    width: u32,
    height: u32,
    data: Vec<u8>,
    timestamp: u64,
}

fn main() {
    // Ring buffer with capacity for 3 frames
    // Only the most recent frames matter; old ones can be dropped
    let ring: Arc<ArrayQueue<Frame>> = Arc::new(ArrayQueue::new(3));

    // Producer: captures frames
    let producer_ring = Arc::clone(&ring);
    let producer = thread::spawn(move || {
        for i in 0..10 {
            let frame = Frame {
                width: 1920,
                height: 1080,
                data: vec![0u8; 1920 * 1080 * 4], // RGBA
                timestamp: i,
            };

            // force_push ensures we always accept new frames,
            // dropping the oldest if the consumer is behind
            let evicted = producer_ring.force_push(frame);
            if let Some(old) = evicted {
                eprintln!("Dropped frame with timestamp {}", old.timestamp);
            }

            thread::sleep(Duration::from_millis(16)); // ~60fps
        }
    });

    // Consumer: reads frames
    let consumer_ring = Arc::clone(&ring);
    let consumer = thread::spawn(move || {
        loop {
            match consumer_ring.pop() {
                Some(frame) => {
                    println!(
                        "Rendering frame: {}x{} @ ts={}",
                        frame.width, frame.height, frame.timestamp
                    );
                }
                None => {
                    // No frame available, spin briefly
                    thread::sleep(Duration::from_millis(1));
                }
            }
        }
    });

    producer.join().unwrap();
    // Consumer runs indefinitely in this example
}
```

## Multi-Producer Multi-Consumer Example

ArrayQueue supports multiple producers and consumers without additional synchronization:

```rust
use crossbeam::queue::ArrayQueue;
use std::sync::Arc;
use std::thread;

fn main() {
    let queue = Arc::new(ArrayQueue::new(100));

    // Spawn 4 producer threads
    let mut producers = vec![];
    for id in 0..4 {
        let q = Arc::clone(&queue);
        producers.push(thread::spawn(move || {
            for i in 0..25 {
                let value = id * 25 + i;
                while q.push(value).is_err() {
                    // Queue full, retry
                    thread::yield_now();
                }
            }
        }));
    }

    // Spawn 2 consumer threads
    let mut consumers = vec![];
    for _ in 0..2 {
        let q = Arc::clone(&queue);
        consumers.push(thread::spawn(move || {
            let mut received = vec![];
            loop {
                match q.pop() {
                    Some(v) => received.push(v),
                    None => {
                        if Arc::strong_count(&q) <= 3 {
                            // Producers are done (only consumers + main hold refs)
                            break;
                        }
                        thread::yield_now();
                    }
                }
            }
            received
        }));
    }

    for p in producers {
        p.join().unwrap();
    }

    let mut all_values = vec![];
    for c in consumers {
        all_values.extend(c.join().unwrap());
    }
    all_values.sort();

    assert_eq!(all_values, (0..100).collect::<Vec<_>>());
    println!("All 100 values received correctly!");
}
```

## Draining the Queue to Get the Latest Item

A common pattern when you only need the most recent item (e.g., the latest frame):

```rust
use crossbeam::queue::ArrayQueue;

fn get_latest<T>(queue: &ArrayQueue<T>) -> Option<T> {
    let mut latest = queue.pop()?;
    // Drain remaining items, keeping only the last one
    while let Some(newer) = queue.pop() {
        // Drop `latest`, keep `newer`
        latest = newer;
    }
    Some(latest)
}

// Usage:
let q = ArrayQueue::new(10);
q.push(1).unwrap();
q.push(2).unwrap();
q.push(3).unwrap();

// Gets 3, the most recent value
assert_eq!(get_latest(&q), Some(3));
// Queue is now empty
assert_eq!(q.pop(), None);
```

## Thread Safety

`ArrayQueue<T>` implements:
- `Send` where `T: Send`
- `Sync` where `T: Send`

This means it can be safely shared across threads via `Arc<ArrayQueue<T>>` as long as the contained type is `Send`.

## Performance Characteristics

- **Lock-free**: Uses atomic operations, no mutexes
- **Pre-allocated**: Fixed buffer allocated at construction, no runtime allocations during push/pop
- **Cache-friendly**: Contiguous buffer layout
- **Faster than SegQueue**: Due to pre-allocation and bounded nature
- **Suitable for real-time**: No blocking, predictable latency (especially with `force_push`)

## Comparison with Alternatives

| Feature | `ArrayQueue` | `SegQueue` | `channel::bounded` |
|---------|-------------|------------|-------------------|
| Bounded | Yes | No | Yes |
| Lock-free | Yes | Yes | No (uses parking) |
| Ring buffer | Yes (`force_push`) | N/A | No |
| Pre-allocated | Yes | No | Yes |
| Blocking recv | No | No | Yes |
| Best for | Low-latency ring buffers | Unbounded work queues | Message passing |
