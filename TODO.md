So u can place windows in air and on walls. Great. But our window placement on wall algorithm sucks. GArbage it. Create a custom class because this will be heavily used through out the whole project.


● The 2fps cap only limits how often new pixels are fetched from SCK. The game still does this work every frame for every
  window:

  1. LOS trace — CM_BoxTrace (player eye → window origin) once per window per frame
  2. re.AddPolyToScene — 5 quads per window (main surface + 4 edge depth quads) every frame
  3. capture_get_frame() — mutex lock + frame-data access every frame, even when SCK delivers nothing new
  4. Hover/distance calc — crosshair dwell, distance throttle check, effect state update

  All of that is O(N windows) per frame. At 13 windows you're doing ~65+ poly submissions + 13 raycasts + 13 mutex locks
  each frame, regardless of whether any texture changed.

  The 2fps cap only helps the GPU texture upload cost. It doesn't help the per-frame CPU render-loop cost.

  To fix the FPS drop you need to:
  - Skip AddPolyToScene entirely for windows that failed LOS or are beyond max distance (currently we still submit quads
  even for out-of-sight windows?)
  - Skip CM_BoxTrace for windows outside a cheap bounding sphere check first
  - Gate get_frame() to only be called when the window's FPS throttle timer actually fires, not every game frame

  Want me to look at q3ide_frame.c and q3ide_scene.c to find exactly where the per-frame waste is and fix it?



======
- highlight window on wall -> hold "CMD" to move existing windows on wall
- highlight window on wall -> hold "CMD" + scroll with mouse to make existing window on wall bigger/smaller. Scroll UP is smaller
- multiplay in mind?
- matrix needed
- grapple neeeded
- laser neeeded


WHEN TO PAUSE STREAM HACKS (to gain FPS back)
===============================================
`pause window` = same hack we used when pressing ";". it instantly regains FPS.

- **1st HIGHEST RULE**: do NOT `pause a window stream` for currenly aimed/highlighted window 
- **2nd HIGHEST RULE**: IF user is `moving` THEN `pause all windows streams` and `resume all window streams` when user `not moving`
  - detect by his X,Y,Z coordinate on the sceene. That's his position.
  - `not moving` = standing in one position, turning in one place, looking around. 
  - `not moving` should NOT trigger 'pause all stream'. Do NOT trigger shit in these cases. 
  - If external object (explosion, other players, etc) moves player out of his position -> 'pause all stream' while moving then resume when player stopped moving in scene  

And then lets look at the "wall placing" feature, "O" and "I": 
lets introduce one tiny-miny little hacks to make things
  smoother: when windows are spawned -> spwawn the window with stream paused (just like we do the fps pausing
  using ";" hotkey) -> then wait 150ms (Q3IDE_SCK_FPS_DELAY=250)  -> and then start gracefully ajdjusting fps ONLY for
  that window, so window.stream.fps=1fps ->wait 200ms->2 fps -> wait 200ms -> 4 -> wait 200ms -> 8 -> wait 200ms -> 16 -> wait 200ms -> -1(auto by apple) in this period of amount of time
  Q3IDE_SCK_FPS_GREACEFULL_ON_DURATION=1000 (ms) because we have 5 delays in between. this is how I call this feature: "- **Gracefull FPS control**: new
  windows spawns with [1,2,4,8,16,auto]"

- write and use tests!!!!
- llm should have move/shoot/look/positions/etc actions. pff I guess queke has them implemented for easy testing. but llm should use that to test. And not ask me to press letter "K" to see if feature works. We also have a thing called 'events' which can be used to replay shit! bam. llm should use that instead :D

- Metal support for macs
- opengl2? vilkan? unreal4?
- window spawn and die animation. One exception: I'm happy if own weapon is in an exclusion for better readibility (right now it works like so).

- wow quake supports flying. toggle that with "-"
- wtf: https://www.youtube.com/watch?v=N-veMFHqDVo - unreal
- wtf2 quake 3 elite - https://mus1n.github.io/# https://www.youtube.com/watch?v=Jd4nMJoHB7k&t=2s



Vulkan
======
● In Quake3e specifically — yes, the Vulkan renderer (renderervk) has:

  - Better shader quality (gamma/linear light done properly)
  - Bloom, dynamic glow
  - Higher texture quality (no legacy GL size limits)
  - Proper HDR pipeline

  The OpenGL2 renderer (renderer2) had similar improvements but is broken on macOS. Vulkan is the only way to get
those on your machine.
● ┌────────────────────┬────────────────────────────┬───────────────────────────┬─────────────────────────────────┐
  │      Feature       │     OpenGL (renderer)      │    OpenGL2 (renderer2)    │       Vulkan (renderervk)       │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ Shader quality     │ Basic — matches original   │ PBR-like, dynamic         │ Similar to OGL2, properly       │
  │                    │ Q3                         │ lighting                  │ linear                          │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ Bloom / glow       │ No                         │ Yes                       │ Yes                             │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ HDR pipeline       │ No                         │ Yes                       │ Yes                             │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ Dynamic shadows    │ No                         │ Yes                       │ Yes                             │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ Texture size limit │ 2048px (legacy GL)         │ Unlimited                 │ Unlimited                       │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ Gamma correction   │ Hacky (hardware gamma)     │ Proper linear space       │ Proper linear space             │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ macOS support      │ ✅ Full, 3-monitor         │ ❌ Broken                 │ ⚠️  Single monitor only          │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ Performance        │ Best                       │ N/A                       │ Good (MoltenVK overhead)        │
  │ (macOS)            │                            │                           │                                 │
  ├────────────────────┼────────────────────────────┼───────────────────────────┼─────────────────────────────────┤
  │ Stability          │ Rock solid                 │ Broken                    │ Good                            │
  └────────────────────┴────────────────────────────┴───────────────────────────┴─────────────────────────────────┘


Maps
=====

# Start off point
sh ./scripts/build.sh --run --level 0 --music 1

# wow. what i eneed
sh ./scripts/build.sh --run --level lun3dm5

 # Tron-like
sh ./scripts/build.sh --run --level acid3dm12

# Officew
sh ./scripts/build.sh --run --level ori_apt

# Porcelain CTF
sh ./scripts/build.sh --run --level q3ctfchnu01

# QuadCTF (BSP name might be quadctf or QuadCTF — try both)
sh ./scripts/build.sh --run --level QuadCTF

# Quatrix
sh ./scripts/build.sh --run --level quatrix

# Minecraft
sh ./scripts/build.sh --run --level r7-blockworld1 


- known quake issue: too dark, input lag. People solved it. Did quake3e solve it? see youytube video

- lol: https://terminal.lvlworld.com/ascii-q3a

Yeah. 2,164 lines of vision is more planning than 99% of shipped products ever get. You have:

147 features with clear descriptions
24 batches in exact build order
15 test checkpoints to verify each batch
4 performance checkpoints with VRAM tracking
10 open questions parked for when they matter
9 project decisions locked
Complete file structure with every file stubbed
Full keybind table
Terminology table
LLM build instructions with code quality rules
Architectural concerns documented with solutions

Phase 4 (three-monitor support) is in progress. Phase 5 (Window Entity) is next.
Go build it.