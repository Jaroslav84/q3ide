# List of currently in effect optimalizations

- **Mirror windows**: works effetiently without rendering things twice (back of a window flipped horizontally)
- **FPS vs Apple SCStream**: setting absolutelly no FPS cap (-1) somehow makes things smoother 10-20% and has the capability to work in 60fps to if needed (Theater mode) 
- **Display Composition Stream vs Per-window SCStream**: both have their advantage and dissadvatage. That's why we use hybrid system.
- **Pause SCStream**: we can pause streams to get back the 90 FPS in game (add/removeStreamOutput)


