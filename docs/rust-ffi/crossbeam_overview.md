# Crossbeam: Tools for Concurrent Programming in Rust

> **Source:** <https://docs.rs/crossbeam/latest/crossbeam/>
> **Description:** API documentation for the `crossbeam` crate (v0.8.4), providing concurrent data structures, channels, memory management, and synchronization primitives. Licensed under MIT OR Apache-2.0.

---

## Overview

Crossbeam provides a comprehensive set of tools for concurrent programming in Rust. It is organized into several modules covering atomics, data structures, memory management, thread synchronization, and utilities.

## Modules

### Atomics

#### `AtomicCell<T>`

A thread-safe mutable memory location. Unlike `std::sync::Mutex`, `AtomicCell` can store values without heap allocation and uses atomic operations when possible.

```rust
use crossbeam::atomic::AtomicCell;

let cell = AtomicCell::new(42);
assert_eq!(cell.load(), 42);
cell.store(15);
assert_eq!(cell.load(), 15);
```

#### `AtomicConsume`

A trait for reading primitive atomic types using "consume" ordering semantics, which can be more efficient than acquire ordering on certain architectures (notably ARM).

### Data Structures

#### `deque` Module - Work-Stealing Deques

Work-stealing deques designed for task scheduler implementations. A worker thread has a local deque, and other threads can steal tasks from it.

```rust
use crossbeam::deque::{Worker, Stealer, Steal};

let w = Worker::new_fifo();
let s = w.stealer();

w.push(1);
w.push(2);
w.push(3);

assert_eq!(s.steal(), Steal::Success(1));
assert_eq!(w.pop(), Some(3));
```

Types:
- `Worker<T>` - The owner side of a deque (push/pop)
- `Stealer<T>` - A handle for stealing from the deque
- `Steal<T>` - Result of a steal operation (`Success(T)`, `Empty`, `Retry`)
- `Injector<T>` - A global queue that feeds into worker deques

#### `ArrayQueue<T>` - Bounded MPMC Queue

A bounded multi-producer multi-consumer queue that allocates a fixed-capacity buffer on construction. Having a buffer allocated upfront makes this queue faster than `SegQueue`.

```rust
use crossbeam::queue::ArrayQueue;

let q = ArrayQueue::new(2);

assert_eq!(q.push('a'), Ok(()));
assert_eq!(q.push('b'), Ok(()));
assert_eq!(q.push('c'), Err('c')); // Queue is full

assert_eq!(q.pop(), Some('a'));
assert_eq!(q.pop(), Some('b'));
assert_eq!(q.pop(), None); // Queue is empty
```

**Ring buffer mode with `force_push`:**

```rust
use crossbeam::queue::ArrayQueue;

let q = ArrayQueue::new(2);
q.push(10).unwrap();
q.push(20).unwrap();

// force_push overwrites the oldest element when full
assert_eq!(q.force_push(30), Some(10)); // Returns displaced element
assert_eq!(q.pop(), Some(20));
assert_eq!(q.pop(), Some(30));
```

Methods:
- `new(cap: usize) -> ArrayQueue<T>` - Creates a queue with specified capacity (panics if zero)
- `push(&self, value: T) -> Result<(), T>` - Push, returning value back if full
- `force_push(&self, value: T) -> Option<T>` - Push, displacing oldest if full
- `pop(&self) -> Option<T>` - Remove and return next element
- `capacity(&self) -> usize` - Maximum number of elements
- `len(&self) -> usize` - Current number of elements
- `is_empty(&self) -> bool` - Whether empty
- `is_full(&self) -> bool` - Whether at capacity

Trait implementations: `Debug`, `Drop`, `IntoIterator`, `Send`, `Sync`, `RefUnwindSafe`, `UnwindSafe` (where `T: Send`).

#### `SegQueue<T>` - Unbounded MPMC Queue

An unbounded multi-producer multi-consumer queue. Dynamically allocates small buffer segments as needed.

```rust
use crossbeam::queue::SegQueue;

let q = SegQueue::new();

q.push('a');
q.push('b');

assert_eq!(q.pop(), Some('a'));
assert_eq!(q.pop(), Some('b'));
assert_eq!(q.pop(), None);
```

### Memory Management

#### `epoch` Module - Epoch-Based Garbage Collection

An epoch-based garbage collection mechanism for safe memory reclamation in lock-free data structures. Provides safe deferred deallocation without relying on a global GC.

Key types:
- `Guard` - A guard that keeps the current thread pinned
- `Atomic<T>` - An atomic pointer with epoch-based reclamation
- `Owned<T>` - A uniquely owned heap-allocated value
- `Shared<'g, T>` - A pointer to a heap-allocated value, valid for the lifetime of a guard

```rust
use crossbeam::epoch;

let guard = epoch::pin();
// ... perform lock-free operations while pinned ...
drop(guard); // Unpin when done
```

### Thread Synchronization

#### `channel` Module - MPMC Channels

Multi-producer multi-consumer channels supporting message passing, more capable than `std::sync::mpsc`.

**Bounded channel:**

```rust
use crossbeam::channel::bounded;

let (s, r) = bounded(5);

s.send("hello").unwrap();
assert_eq!(r.recv(), Ok("hello"));
```

**Unbounded channel:**

```rust
use crossbeam::channel::unbounded;

let (s, r) = unbounded();

s.send(1).unwrap();
s.send(2).unwrap();

assert_eq!(r.recv(), Ok(1));
assert_eq!(r.recv(), Ok(2));
```

**Select from multiple channels:**

```rust
use crossbeam::channel::{bounded, select};

let (s1, r1) = bounded(1);
let (s2, r2) = bounded(1);

s1.send(10).unwrap();

select! {
    recv(r1) -> msg => println!("received {:?} from r1", msg),
    recv(r2) -> msg => println!("received {:?} from r2", msg),
}
```

#### `Parker`

A thread parking primitive for thread coordination. More ergonomic than using `std::thread::park`.

```rust
use crossbeam::sync::Parker;
use std::thread;
use std::time::Duration;

let p = Parker::new();
let u = p.unparker().clone();

thread::spawn(move || {
    thread::sleep(Duration::from_millis(500));
    u.unpark();
});

p.park(); // Blocks until unparked
```

#### `ShardedLock<T>`

A sharded reader-writer lock optimized for fast concurrent reads. Uses multiple internal locks, sharded by thread, to reduce contention on the reader side.

```rust
use crossbeam::sync::ShardedLock;

let lock = ShardedLock::new(5);

// Multiple concurrent readers
{
    let r = lock.read().unwrap();
    assert_eq!(*r, 5);
}

// Exclusive writer
{
    let mut w = lock.write().unwrap();
    *w = 10;
}
```

#### `WaitGroup`

Synchronization primitive for coordinating computation boundaries, similar to Go's `sync.WaitGroup`.

```rust
use crossbeam::sync::WaitGroup;
use std::thread;

let wg = WaitGroup::new();

for _ in 0..4 {
    let wg = wg.clone();
    thread::spawn(move || {
        // Do some work...
        drop(wg); // Signal completion
    });
}

wg.wait(); // Wait for all threads to finish
```

### Utilities

#### `Backoff`

Exponential backoff implementation for spin loop optimization. Useful for reducing contention in lock-free algorithms.

```rust
use crossbeam::utils::Backoff;

let backoff = Backoff::new();

loop {
    // Try to do something...
    if successful {
        break;
    }
    backoff.spin(); // Exponential backoff
    if backoff.is_completed() {
        // Fall back to blocking
        thread::park();
        break;
    }
}
```

#### `CachePadded<T>`

Padding and alignment utility aligned to cache line length. Prevents false sharing between threads.

```rust
use crossbeam::utils::CachePadded;

let padded = CachePadded::new(42);
assert_eq!(*padded, 42);
```

#### `scope` Function

Enables spawning threads that safely borrow stack-local variables. Scoped threads are guaranteed to join before the scope exits.

```rust
use crossbeam::scope;

let mut data = vec![1, 2, 3];

scope(|s| {
    s.spawn(|_| {
        // Can borrow data because scope guarantees join
        println!("{:?}", data);
    });
}).unwrap();
```

## Feature Flags

The crossbeam crate re-exports functionality from sub-crates. You can depend on sub-crates individually for finer-grained dependency control:

- `crossbeam-channel` - MPMC channels
- `crossbeam-deque` - Work-stealing deques
- `crossbeam-epoch` - Epoch-based GC
- `crossbeam-queue` - Concurrent queues (ArrayQueue, SegQueue)
- `crossbeam-utils` - Utilities (Backoff, CachePadded, scope, Parker, WaitGroup, ShardedLock)

## Platform Support

Supported across `aarch64`, `i686`, `x86_64` on Linux, macOS, and Windows.

## Documentation Coverage

100% of the crate is documented.
