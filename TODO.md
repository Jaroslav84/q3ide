So u can place windows in air and on walls. Great. But our window placement algorythm sucks. GArbage it. Create a custom class because this will be heavily used through out the whole project.

Here is what I need: I need an algo that takes all the windows as input -> and arranges them super ncielly inside the room the player is. It must be arranged evenly on the walls ONLY! Pick window size so that it has a ratio of 66.% of the wall height. If shit sticks out of the wall then you need to take that into consideration. Windows should be placed like TV screens on the wall, u get it?

Also: if u know that a window is stuck to a wall: never render it's back. No need to ;)

- windows upgrade: if there is something in front of the window (parts of map, 3D object, other players, etc) then it should be in front of it. Right now I can see all my windows from any angle, from anywhere no matter what is in front of it. This needs to work pixel perfect. No cheats. A window is a normal 3D object like other things in the game world.
- window spawn and die animation. One exception: I'm happy if own weapon is in an exclusion for better readibility (right now it works like so).




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