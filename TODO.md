So u can place windows in air and on walls. Great. But our window placement on wall algorythm sucks. GArbage it. Create a custom class because this will be heavily used through out the whole project.


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

- file naming is awfull :D

- write and use tests!!!!
- llm should have move/shoot/look/positions/etc actions. pff I guess queke has them implemented for easy testing. but llm should use that to test. And not ask me to press letter "K" to see if feature works. We also have a thing called 'events' which can be used to replay shit! bam. llm should use that instead :D


- opengl2? vilkan? unreal4?
- window spawn and die animation. One exception: I'm happy if own weapon is in an exclusion for better readibility (right now it works like so).

- wow quake supports flying. toggle that with "-"
- wtf: https://www.youtube.com/watch?v=N-veMFHqDVo - unreal
- wtf2 quake 3 elite - https://mus1n.github.io/# https://www.youtube.com/watch?v=Jd4nMJoHB7k&t=2s




- allow to switch MAPS like projects



Maps
=====

# Start off point
sh ./scripts/build.sh --run --level 0 --execute 'q3ide attach all' --music 1

# wow. what i eneed
sh ./scripts/build.sh --run --level lun3dm5 --execute 'q3ide attach all'

 # Tron-like
sh ./scripts/build.sh --run --level acid3dm12 --execute 'q3ide attach all'

# Officew
sh ./scripts/build.sh --run --level ori_apt --execute 'q3ide attach all'

# Porcelain CTF
sh ./scripts/build.sh --run --level q3ctfchnu01 --execute 'q3ide attach all'

# QuadCTF (BSP name might be quadctf or QuadCTF — try both)
sh ./scripts/build.sh --run --level QuadCTF --execute 'q3ide attach all'

# Quatrix
sh ./scripts/build.sh --run --level quatrix --execute 'q3ide attach all'

# Minecraft
sh ./scripts/build.sh --run --level r7-blockworld1 --execute 'q3ide attach all' 


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