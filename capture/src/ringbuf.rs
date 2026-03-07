/// Lock-free frame ring buffer using crossbeam's ArrayQueue.
///
/// The capture thread pushes frames via `force_push` (overwrites oldest),
/// and the engine thread pops the latest frame without blocking.

use crossbeam_queue::ArrayQueue;
use std::sync::Arc;

use crate::backend::FrameData;

/// Default ring buffer capacity — holds N frames.
/// Small because we only care about the latest frame.
const DEFAULT_CAPACITY: usize = 3;

/// A lock-free ring buffer for captured frames.
///
/// Producer (capture callback): calls `push_frame` which uses `force_push`
/// to always accept new frames, dropping the oldest if full.
///
/// Consumer (engine render thread): calls `pop_frame` to get the latest frame.
#[derive(Clone)]
pub struct FrameRingBuffer {
    queue: Arc<ArrayQueue<FrameData>>,
}

impl FrameRingBuffer {
    /// Create a new ring buffer with the default capacity.
    pub fn new() -> Self {
        Self::with_capacity(DEFAULT_CAPACITY)
    }

    /// Create a new ring buffer with a specific capacity.
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            queue: Arc::new(ArrayQueue::new(capacity)),
        }
    }

    /// Push a frame into the ring buffer.
    /// If the buffer is full, the oldest frame is dropped.
    pub fn push_frame(&self, frame: FrameData) {
        // force_push returns Some(old_frame) if the queue was full.
        // We intentionally drop the old frame.
        let _ = self.queue.force_push(frame);
    }

    /// Pop the latest frame from the ring buffer.
    /// Returns None if no frames are available.
    pub fn pop_frame(&self) -> Option<FrameData> {
        // Drain all frames, keeping only the latest.
        let mut latest = self.queue.pop()?;
        while let Some(newer) = self.queue.pop() {
            latest = newer;
        }
        Some(latest)
    }

    /// Check if the buffer is empty.
    #[allow(dead_code)]
    pub fn is_empty(&self) -> bool {
        self.queue.is_empty()
    }

    /// Get the number of frames currently in the buffer.
    #[allow(dead_code)]
    pub fn len(&self) -> usize {
        self.queue.len()
    }
}

impl Default for FrameRingBuffer {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_frame(w: u32, h: u32, ts: u64) -> FrameData {
        FrameData {
            pixels: vec![0u8; (w * h * 4) as usize],
            width: w,
            height: h,
            stride: w * 4,
            timestamp_ns: ts,
            source_wid: 0,
        }
    }

    #[test]
    fn test_push_pop() {
        let buf = FrameRingBuffer::new();
        assert!(buf.is_empty());

        buf.push_frame(make_frame(100, 100, 1));
        assert_eq!(buf.len(), 1);

        let frame = buf.pop_frame().unwrap();
        assert_eq!(frame.timestamp_ns, 1);
        assert!(buf.is_empty());
    }

    #[test]
    fn test_pop_returns_latest() {
        let buf = FrameRingBuffer::with_capacity(3);
        buf.push_frame(make_frame(100, 100, 1));
        buf.push_frame(make_frame(100, 100, 2));
        buf.push_frame(make_frame(100, 100, 3));

        let frame = buf.pop_frame().unwrap();
        assert_eq!(frame.timestamp_ns, 3);
        assert!(buf.is_empty());
    }

    #[test]
    fn test_force_push_overwrites_oldest() {
        let buf = FrameRingBuffer::with_capacity(2);
        buf.push_frame(make_frame(100, 100, 1));
        buf.push_frame(make_frame(100, 100, 2));
        buf.push_frame(make_frame(100, 100, 3)); // should drop frame 1

        let frame = buf.pop_frame().unwrap();
        assert_eq!(frame.timestamp_ns, 3);
    }
}
