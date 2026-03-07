# Fabien Sanglard's Quake 3 Code Review

> **Source:** [Fabien Sanglard - Quake 3 Source Code Review](https://fabiensanglard.net/quake3/)
>
> This document summarizes the comprehensive code review of idTech3 by Fabien Sanglard, covering the 3D engine powering Quake III Arena and Quake Live. The review spans five parts: Architecture, Renderer, Networking, Virtual Machines, and Bot AI.

## Engine Overview

idTech3 evolved from idTech2 with major innovations:

- Dual-core renderer with material-based shaders built on the OpenGL Fixed Pipeline
- Snapshot-based network model
- Integrated virtual machine system
- Bot artificial intelligence

The engine's working title was "Trinity" -- referencing the Trinity River in Dallas (following Intel's river-naming convention for projects), not the Matrix film as some speculated.

## Project Structure

The codebase comprises eight Visual Studio projects:

| Project | Type | Purpose |
|---------|------|---------|
| **quake3** | Executable | Main engine |
| **renderer** | Static Library | OpenGL-based rendering |
| **botlib** | Static Library | AI systems |
| **cgame** | Dynamic Library / QVM Bytecode | Client-side game logic |
| **game** | Dynamic Library / QVM Bytecode | Server-side game logic |
| **q3_ui** | Dynamic Library / QVM Bytecode | User interface |

Debug builds utilize three virtual machine DLLs; release builds compile these into QVM bytecode.

### Code Statistics

| Metric | Count |
|--------|-------|
| Total files | 919 |
| Lines of code | 341,994 |
| Blank lines | 68,293 |
| Comment lines | 95,509 |

The toolchain (LCC compiler, q3asm, q3map, q3radiant) comprises approximately 30% of the codebase.

## Core Design Principles

### Unified Event Queue

> "Every single input (keyboard, win32 message, mouse, UDP socket) is converted into an event_t and placed in a centralized event queue."

This unified approach enables:
- Comprehensive input journaling for bug recreation
- Reproducible game states
- Clean separation between input sources and consumers

### Client-Server Architecture

The architecture enforces an explicit networking split even in single-player:

- **Server Side**: Maintains authoritative game state, determines what clients need, manages network propagation
- **Client Side**: Predicts entity positions (latency compensation), renders the game world from a viewpoint

John Carmack noted this architectural decision proved valuable despite requiring additional work from licensees. Even single-player games run a local server.

## Engine Main Loop

The main execution loop demonstrates how inputs flow to outputs:

```
1. IN_Frame()        -- Collect joystick/mouse inputs into event queue
2. Com_EventLoop()   -- Pump Win32 messages, UDP sockets, console commands
3. SV_Frame()        -- Server processing: bot logic, game VM calls
4. CL_Frame()        -- Client processing: send commands, update screen
```

### Rendering Flow

Rendering occurs through VM system calls rather than direct function invocation:

1. `quake3.exe` sends `CG_DRAW_ACTIVE_FRAME` message to the Client VM
2. The Client VM performs entity culling and prediction
3. The Client VM calls for rendering via `CG_R_RENDERSCENE` system call
4. `quake3.exe` receives the system call and calls `RE_RenderScene`
5. The renderer processes and draws the scene

This emphasizes the virtual machines' central role in the architecture.

## Memory Management

Two custom allocators handle different allocation patterns:

### Zone Allocator

- **Purpose**: Runtime, short-term, and small memory allocations
- **Pattern**: Frequent alloc/free cycles
- **Usage**: Strings, temporary buffers, command processing

### Hunk Allocator

- **Purpose**: Level-load operations, large long-term allocations
- **Pattern**: Bulk allocation at map load, freed on map change
- **Usage**: Geometry, maps, textures, animations from pak files
- **Design**: Double-ended stack (high/low) for efficient bulk operations

## Renderer Architecture

The renderer project functions as a **pluggable module**, theoretically allowing Direct3D or software rendering implementations. Key characteristics:

- Built on OpenGL 1.X fixed pipeline
- Material-based shader system (text-defined surface appearances)
- BSP/PVS/Lightmap rendering foundation
- Frontend/backend split with optional SMP support
- Producer/Consumer pattern for render command passing

### Shader System Innovation

The shader system was a significant innovation for 1999, providing space for visual variety before hardware vertex, geometry, and fragment shaders existed. Shaders are defined in text scripts and compiled at load time into rendering state configurations.

## Virtual Machine System

Virtual machines represent approximately **30% of the codebase** and function as a mini-operating system providing system calls to three processes (cgame, game, q3_ui).

### Design Goals

The VM system combines:
- **QuakeC portability** from Quake 1 (bytecode runs anywhere)
- **Speed characteristics** of Quake 2 DLLs (JIT compilation)
- **Security isolation** through sandboxed execution

### Implementation

- The **Little C Compiler (LCC)**, an open-source ANSI C compiler, generates QVM bytecode
- `q3asm` assembles the bytecode into loadable `.qvm` files
- The interpreter supports JIT compilation for native speed
- System calls use negative indices to distinguish from bytecode functions

### Communication Model

Each VM exports:
- **`vmMain()`**: The only entry point, acts as a message dispatcher
- **`dllEntry()`**: Provides the system call function pointer

The engine communicates with VMs through `VM_Call()` (up to 11 parameters). VMs request engine services through system calls (trap functions).

## Networking Model

### Snapshot-Based

Unlike Quake 1's delta compression of individual entity updates, Quake 3 uses a **snapshot model**:

1. Server captures complete game state snapshots at fixed intervals
2. Snapshots are delta-compressed against previous acknowledged snapshots
3. Client receives snapshots and interpolates between them
4. Client prediction runs locally for immediate movement response

### Key Innovation

The snapshot model simplified the networking code and made it more robust against packet loss compared to the stream-based approach of previous engines.

## Bot AI System (botlib)

The bot system has an interesting development history:

- Initially problematic due to inadequate supervision during development
- Eventually completed by **Jan Paul van Waveren** (known as "Mr. Elusive"), a prominent Dutch mod maker
- This separated development justified the distinct `botlib` project
- The AI uses area awareness system (AAS) files generated from BSP data

## File System

The engine uses a **virtual filesystem** that:

1. Searches pak files (`.pk3`, which are renamed `.zip` files) in reverse alphabetical order
2. Falls back to loose files in the game directory
3. Supports multiple game directories (base game + mod)
4. Handles case-insensitive lookups

This allows mods to override base game assets simply by including replacement files in a higher-priority pak file.

## Key Takeaways for Engine Modification

### Extension Points

1. **Console commands**: `Cmd_AddCommand` for engine-level, `trap_AddCommand` for VM-level
2. **Renderer hooks**: The pluggable renderer architecture allows custom rendering paths
3. **VM system calls**: New trap functions can be added to extend VM capabilities
4. **Event system**: Custom events can be injected into the unified event queue

### Modification Strategies

- **Engine modification** (Q3IDE approach): Direct access to all systems, maximum control
- **VM/Mod approach**: Sandboxed, portable, but limited to existing system calls
- **Hybrid**: Extend the engine with new system calls, then use them from VMs

### Architecture Lessons

- The strict module separation makes the engine extensible
- The VM system provides a clean plugin interface
- The renderer's pluggable design means custom rendering can be added without touching game logic
- The unified event queue means new input sources can be added cleanly
