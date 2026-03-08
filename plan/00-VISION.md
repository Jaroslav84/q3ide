# Quake III IDE

### The first post-AI workplace.

---

## What Is This?

Your AI agent is coding. You're fragging your coworkers. Your claude terminals are on the walls, in different rooms. When the build's done, you see it.

Quake III IDE is not just an IDE. It's not a game. It's not a co-working space. It's not a chat app. It's all of these things because none of them are sufficient anymore.

The entire history of developer tools assumed the human is the bottleneck. Now the AI is the bottleneck. The human waits. And what do humans do when they wait together? They socialize. They compete. They look over each other's shoulders.

**This is a developer environment where work happens on the walls and life happens in the room.**

- It's not an IDE — the IDE is a texture on the wall.
- It's not a game — the game is the downtime.
- It's not a co-working space — it's a co-*existing* space.
- It's not a chat app — you're literally running past each other with rocket launchers.

It's what happens when the office becomes a game because the computer doesn't need you at the keyboard anymore.

---

## The Name

**Quake III IDE.**

- IDE as in Integrated Development Environment — but the environment is a Quake III map.
- IDE as in the place where everything comes together — code, agents, teammates, gameplay.
- IDE as in the thing you stare at all day — except now it stares back, and it has a railgun.

It follows the original id Software naming: Quake III Arena. Quake III Team Arena. **Quake III IDE.**

---

## The Core Insight: Method of Loci for Code

The Method of Loci is a 2000-year-old memory technique — the memory palace. You associate information with physical locations in an imagined building. The human brain is wired for spatial memory, not text memory.

In Quake III IDE:

- The `auth/` module is the red room with the lava pit.
- The `api/routes/` is the long corridor with the jump pads.
- The `database/` layer is the basement with the dark lighting.
- The `config/` is the small room near spawn with the shotgun pickup.

You *remember* where things are because you've been physically there — you've strafed through it, fought there, seen your teammate's screen on the wall while chasing them with a lightning gun.

Six months later someone says "where's the rate limiter?" and your brain doesn't think `src/middleware/ratelimit.ts` — it thinks "second floor, the room with the railgun pickup near the window that always shows the Redis dashboard."

**IDEs flatten everything into a file tree. A text list. Tabs. More tabs. It's a spreadsheet pretending to be a creative tool.** Quake III IDE gives code a sense of place. Architecture you can walk through. Code you can visit. A codebase that has geography.

And the kicker — in multiplayer, your whole team builds a *shared spatial memory* of the codebase. "Meet me in the database room" becomes a real sentence with a real location.

Research backs this up: VR environments using the Method of Loci produce ~20% better recall of non-spatial information compared to traditional techniques. And that's with passive walking. Add adrenaline, competition, and social interaction? The spatial encoding goes through the roof.

---

## Feature Vision

### 1. Live Desktop Windows in the Game World

Stream any macOS window into the game as a live-updating texture. Terminals, browsers, VSCode, YouTube — anything ScreenCaptureKit can see.

**Two display modes:**

**Floating Panels (VisionOS Design Language)**
- Frosted glass background with subtle blur
- Rounded corners, thin title bar with window name
- Drop shadow on ground plane
- Hover glow when crosshair aims at panel
- Smooth position interpolation
- Grabbable with +use key — follows your view until released
- Apple VisionOS naming conventions and design tokens

**Wall-Mounted Screens**
- Flush with BSP surface, slight bevel/frame
- Configurable aesthetic: clean modern, CRT retro, monitor bezel
- Emits faint dynamic light matching dominant screen color
- No title bar — clean embedded look
- Feels like the room was built for this screen

### 2. Rooms as Code Modules

Each room or area on the game map corresponds to a section of the codebase.

- **Room = Module/Package** — the `auth/` directory is a physical room
- **Walls = Files** — screens on walls show files from that module
- **Corridors = Imports/Dependencies** — the hallway connecting two rooms represents the dependency between two modules
- **Teleporters = Deep Links** — jump pads and teleporters take you to distant parts of the codebase instantly
- **Ninja Rope / Grapple Hook** — swing to any room, any file, any part of the code
- **Room Aesthetics Match Code Character** — database layer feels industrial/basement, frontend feels bright/open, security code feels locked-down/bunker

The map IS the architecture diagram. Walking through it IS understanding the system.

### 3. AI Agent Integration

The whole point: your AI agent is working while you play.

- **Build/Task Status on Walls** — live terminal output showing AI agent progress
- **Notification System** — in-game audio + visual cue when AI completes a task (distinct from game sounds, configurable)
- **Diff Viewer** — AI-generated code changes appear on a dedicated screen, highlighted, scrollable
- **Approve/Reject from In-Game** — interact with the screen to accept or request changes
- **Queue Visibility** — see what's in the AI's task queue on a dashboard screen
- **Multiple Agents** — different AI agents working on different parts of the code, their progress visible in their respective rooms

### 4. Multiplayer Co-Working

This is an office. A weird, violent, rocket-jumping office.

- **Shared Game Server** — your team plays on the same map
- **Per-Window Visibility** — configurable per desktop window:
  - **Private** — only you see your screens
  - **Public** — everyone sees your screens (low-res thumbnails over network for bandwidth)
  - **Team-only** — only your team sees them
- **Presence** — see where your teammates are. "Oh, Alex is in the database room again"
- **Screen Sharing by Proximity** — walk up to someone's screen and it gets sharper/higher resolution
- **Voice Chat** — spatialized, distance-based. Louder when close, fades when far. Just like a real office
- **Pair Programming** — two people standing at the same wall-mounted screen, both looking at the same code

### 5. Spatial Navigation

Beyond standard Quake movement:

- **Teleporters Between Modules** — instant travel mapped to codebase structure
- **Grapple Hook / Ninja Rope** — swing to distant rooms, because sometimes you need to get to that one file NOW
- **Jump Pads** — launch between floors/layers of the architecture
- **Map Overview** — pull up a minimap that shows the codebase architecture as a floorplan, with dots showing where your teammates are
- **Bookmarks** — save locations in the map ("my debugging spot in the auth room") as personal teleport points
- **Portal Windows** — see into another room through a window without physically going there. Peek at the API layer from the frontend room

### 6. Notifications & Awareness

The game world itself communicates work status:

- **Room Lighting** — a room glows red if tests are failing in that module. Green if passing. Amber if building
- **Screen Flash** — your terminal screen flashes/pulses when a task completes
- **In-Game Announcer** — repurpose Q3's announcer voice: "BUILD COMPLETE" / "TESTS FAILING" / "DEPLOY SUCCESSFUL" (in the Q3 announcer voice)
- **Kill Feed Style Notifications** — "Claude finished refactoring auth/middleware.ts" scrolls like a frag notification
- **Ambient Sound** — each room has subtle audio that reflects its status. Healthy module = calm hum. Failing tests = ominous drone

### 7. The Waiting Game

The core gameplay loop:

1. You give your AI agent a task
2. You walk (or rocket-jump) into the game
3. You frag your coworkers / explore the codebase / check on other agents
4. You see the AI's progress on walls as you play
5. The announcer says "TASK COMPLETE"
6. You walk to the relevant room, review the diff on the wall
7. You approve, give next task
8. Back to fragging

The game is the idle screen. The idle screen is the game. There is no alt-tab. There is no waiting. There is only the arena.

### 8. Map Generation from Codebase

**Future vision:** auto-generate Q3 BSP maps from project structure.

- Directory depth = floor level
- File count = room size
- Import relationships = corridors/teleporters
- Test coverage = room lighting quality
- Git blame hotspots = battle-scarred walls
- Recently changed files = fresh paint / construction signs

A new developer joining the project literally walks through the codebase on their first day. Onboarding becomes exploration.

### 9. Configurable Styles

Not everyone wants the same aesthetic:

- **VisionOS Clean** — frosted glass, minimal, Apple-inspired floating panels
- **CRT Retro** — green phosphor monitors, flickering, scanlines
- **Cyberpunk** — neon-lit screens, dark rooms, holographic projections
- **War Room** — military aesthetic, big screens, situation room feel
- **Cozy Office** — warm lighting, wooden frames around screens, plants (yes, in Quake)
- **Custom** — shader-configurable per user

### 10. Three Monitor Setup

Three physical monitors. Quake 3 spans all three. The game world wraps around you — 180° of arena.

- **Left monitor** — peripheral vision. You see movement, you see a room's walls lit red (failing tests), you catch a teammate rocket-jumping past
- **Center monitor** — where you aim, where you frag, where you read code on the wall in front of you
- **Right monitor** — your AI agent's terminal output, your build dashboard, the diff viewer. Always visible in your peripheral

No alt-tab. No window management. No "let me check my other screen." You're IN the codebase. The codebase is around you. Your code is on the left wall, your game is in front of you, your AI's work is on the right wall.

The three-monitor Q3 experience already existed — competitive players ran it for years. We just give every extra pixel of wall space a purpose.

### 11. The Sounds

Audio design matters:

- **Quake III sounds stay** — rockets, railgun, announcer, jump pads. Non-negotiable
- **Work sounds are distinct** — notification chimes, build sounds, deploy whooshes are clearly different from game sounds. You should never confuse a railgun hit for a deploy notification
- **Spatial audio** — hear a coworker's build completing from two rooms away
- **Ambient coding sounds** — optional subtle keyboard/terminal ambience in rooms with active screens

---

## Technical Foundation

- **Engine:** Quake3e (ec-/Quake3e) — modern, actively maintained, OpenGL + Vulkan, macOS compatible
- **Capture:** ScreenCaptureKit (macOS 12.3+) via screencapturekit-rs (Rust bindings)
- **Bridge:** Rust dylib with C-ABI interface, loaded by the engine at startup
- **Frame Pipeline:** SCK GPU callback → IOSurface → ring buffer → glTexSubImage2D / Vulkan staging buffer
- **Design Language:** Apple VisionOS for floating panels, configurable for wall-mounted
- **Multiplayer:** Q3 netcode + per-window visibility flags + low-res thumbnail sync for shared screens
- **Platform:** macOS first (ScreenCaptureKit), Linux later (PipeWire/wlroots), Windows eventually (DXGI)

---

## Prior Art & Why This Is Different

| Project | What It Does | Why We're Different |
|---------|-------------|-------------------|
| SimulaVR | Linux VR desktop compositor using Godot + wlroots | VR-only, no game, no fun, no multiplayer fragging |
| Immersed | VR multi-monitor workspace | Closed source, no game element, passive experience |
| xrdesktop | Linux desktop windows as OpenVR overlays | Research-grade, no game, no spatial code mapping |
| RiftSketch | VR live coding environment for Three.js | Proof of concept toy, single user, no desktop capture |
| CodeCity | 3D city visualization of codebases | Static visualization, not interactive, not a game, no live code |
| Virtual Desktop | Stream desktop to VR headset | Pure utility, no game, no multiplayer, no spatial meaning |

**Nobody has combined:** live desktop streaming + multiplayer FPS + spatial code mapping + AI agent integration. This is genuinely new.

---

## The Tagline

*Your AI is coding. You're fragging. Your terminal is on the wall.*

---

## Who Is This For?

- Developers who are visual/spatial thinkers and hate tab-based IDEs
- Teams using AI agents (Cursor, Claude Code, Copilot, Devin, OpenClaw) who spend increasing time waiting
- Remote teams who miss the serendipity of a physical office
- Anyone who's ever said "I wish I could see the whole codebase at once"
- People who've been envisioning this for the last 10 years

---

*"Meet me in the database room."*
*— An actual sentence you'll say to your coworker, and both of you will know exactly where that is.*
