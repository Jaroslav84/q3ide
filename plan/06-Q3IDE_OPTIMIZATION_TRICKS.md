# List of currently in effect optimizations

- **Mirror windows**: works efficiently without rendering things twice (back of a window flipped horizontally)
- **FPS vs Apple SCStream**: setting absolutelly no FPS cap (-1) somehow makes things smoother 10-20% and has the capability to work in 60fps to if needed (Theater mode)
- **Display Composition Stream vs Per-window SCStream**: both have their advantage and disadvantage. That's why we use hybrid system.
- **Pause SCStream**: we can pause streams to get back the 90 FPS in game (`STREAMS_PAUSED` AtomicBool — `get_frame()` returns None, last frame frozen on GPU, streams stay warm). This can be leveraged in a BIG BIG matter.
- **Idle SCStream**: apple idle detector is not that great for use. We sampel every 1s for changes and pause streams :) FPS saver feature!

