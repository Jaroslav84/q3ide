# SDL2 Multi-Display API Reference

For Q3IDE triple-monitor spanning implementation in sdl_glimp.c.

## Core Functions

### SDL_GetNumVideoDisplays()
```c
int SDL_GetNumVideoDisplays(void);
```
**Returns**: Number of connected displays (≥1), or negative error code.

Detect available monitors before building spanning window.

```c
int num_displays = SDL_GetNumVideoDisplays();
if (num_displays < 1) {
    Com_Error(ERR_FATAL, "No video displays found");
}
```

### SDL_GetDisplayBounds()
```c
int SDL_GetDisplayBounds(int displayIndex, SDL_Rect *rect);
```
**Parameters**:
- `displayIndex`: 0-based display index
- `rect`: Pointer to SDL_Rect (x, y, w, h) — populated on success

**Returns**: 0 on success, negative error code on failure.

Retrieves display origin (x, y) and dimensions (w, h). Primary display always at (0, 0).

```c
SDL_Rect display_bounds;
if (SDL_GetDisplayBounds(0, &display_bounds) < 0) {
    Com_Error(ERR_FATAL, "SDL_GetDisplayBounds failed: %s", SDL_GetError());
}
// display_bounds now contains: x, y, width, height
```

### SDL_GetWindowDisplayIndex()
```c
int SDL_GetWindowDisplayIndex(SDL_Window *window);
```
**Returns**: Display index of window's center, or negative error code.

Query which monitor a window is primarily on after creation.

```c
int display_idx = SDL_GetWindowDisplayIndex(window);
if (display_idx < 0) {
    Com_Printf("Warning: SDL_GetWindowDisplayIndex failed\n");
}
```

### SDL_CreateWindow()
```c
SDL_Window *SDL_CreateWindow(const char *title,
                             int x, int y,
                             int w, int h,
                             Uint32 flags);
```
**Parameters**:
- `title`: Window title (UTF-8)
- `x, y`: Position (use explicit coordinates for spanning)
- `w, h`: Width, height in screen coordinates
- `flags`: OR'd flag combination

**Returns**: SDL_Window pointer on success, NULL on failure.

**Key Flags for Multi-Monitor**:
- `SDL_WINDOW_BORDERLESS`: No window decoration (required for seamless spanning)
- `SDL_WINDOW_FULLSCREEN_DESKTOP`: Fullscreen at desktop resolution
- `SDL_WINDOW_OPENGL`: OpenGL context compatible

```c
SDL_Window *window = SDL_CreateWindow(
    "Q3IDE",
    union_x,        // leftmost display origin
    union_y,        // topmost display origin
    union_width,    // total spanning width
    union_height,   // total spanning height
    SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL
);
if (!window) {
    Com_Error(ERR_FATAL, "SDL_CreateWindow failed: %s", SDL_GetError());
}
```

## Triple-Monitor Spanning Strategy

### 1. Query Display Bounds
```c
int num_displays = SDL_GetNumVideoDisplays();
SDL_Rect *displays = malloc(num_displays * sizeof(SDL_Rect));

for (int i = 0; i < num_displays; i++) {
    if (SDL_GetDisplayBounds(i, &displays[i]) < 0) {
        Com_Error(ERR_FATAL, "Display %d query failed", i);
    }
}
```

### 2. Compute Union Rectangle
```c
// Find bounding box encompassing all displays
int union_x = displays[0].x;
int union_y = displays[0].y;
int union_right = displays[0].x + displays[0].w;
int union_bottom = displays[0].y + displays[0].h;

for (int i = 1; i < num_displays; i++) {
    union_x = (displays[i].x < union_x) ? displays[i].x : union_x;
    union_y = (displays[i].y < union_y) ? displays[i].y : union_y;
    union_right = (displays[i].x + displays[i].w > union_right) ?
                  displays[i].x + displays[i].w : union_right;
    union_bottom = (displays[i].y + displays[i].h > union_bottom) ?
                   displays[i].y + displays[i].h : union_bottom;
}

int union_width = union_right - union_x;
int union_height = union_bottom - union_y;
```

### 3. Create Spanning Window
```c
SDL_Window *window = SDL_CreateWindow(
    "Q3IDE - Triple Monitor",
    union_x,
    union_y,
    union_width,
    union_height,
    SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL
);

if (!window) {
    Com_Error(ERR_FATAL, "Spanning window creation failed: %s", SDL_GetError());
}
```

### 4. Create OpenGL Context
```c
SDL_GLContext glctx = SDL_GL_CreateContext(window);
if (!glctx) {
    Com_Error(ERR_FATAL, "GL context creation failed: %s", SDL_GetError());
}

// Enable vsync
SDL_GL_SetSwapInterval(1);
```

## Position and Size Adjustment

### SDL_SetWindowPosition()
```c
void SDL_SetWindowPosition(SDL_Window *window, int x, int y);
```
Reposition window after creation (useful for dynamic monitor changes).

### SDL_SetWindowSize()
```c
void SDL_SetWindowSize(SDL_Window *window, int w, int h);
```
Resize window (apply after computing new union rectangle).

## Error Handling Pattern

```c
const char *error = SDL_GetError();
if (error && error[0] != '\0') {
    Com_Printf("SDL2 error: %s\n", error);
}
```

Always clear error state before critical operations, check after.

## Notes for sdl_glimp.c Integration

- Compute union bounds **before** SDL_CreateWindow()
- Use explicit x, y coordinates (not SDL_WINDOWPOS_CENTERED)
- SDL_WINDOW_BORDERLESS eliminates gaps between monitors
- Multi-viewport rendering happens in tr_backend.c via r_mm* cvars
- Display enumeration order may vary by OS; rely on coordinates, not indices
