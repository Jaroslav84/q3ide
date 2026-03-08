# Quake 3 HUD & 2D Rendering Reference

For Q3IDE Batch 9: Ornament and Widget window styles (2D elements attached to/overlaid on windows).

## 1. CG_Draw2D() — HUD Rendering Entry Point

The cgame module calls `CG_Draw2D()` after rendering the 3D scene. This is the exclusive point for drawing all 2D HUD elements (crosshair, ammo counter, health, score, etc.). The engine guarantees that:
- The 3D world is fully rendered
- The 2D projection matrix is set via `RB_SetGL2D()`
- Coordinates use **virtual 640x480 space** (scaled to actual display resolution)
- Z-ordering is handled by draw order (last drawn = front)

Entry: `cl_cgame.c:CL_KeyEventCallback()` → calls exported cgame hook `CG_DrawActiveFrame`

## 2. Core 2D Primitive: CG_DrawPic()

```c
void CG_DrawPic(float x, float y, float w, float h, qhandle_t hShader)
```

Draws a textured quad at 640x480 virtual coordinates:
- **x, y**: top-left corner in virtual screen space
- **w, h**: width/height in virtual pixels
- **hShader**: shader handle (registered via `trap_R_RegisterShader()`)

Internally calls `SCR_AdjustFrom640()` then `trap_R_DrawStretchPic()` with full texture coords (0,0)→(1,1).

**Q3IDE Usage**: Base for Window Bar, Widget backgrounds.

## 3. Text Rendering: CG_DrawStringExt & CG_DrawBigStringColor

```c
void CG_DrawStringExt(float x, float y, const char *text, const float *color,
                      qboolean shadow, qboolean forceColor, int limit, int style)
```

Draws monospace characters at 640x480 virtual coordinates:
- Small chars: 16 chars/line (40px wide)
- Color: 4-component RGBA (0.0–1.0); NULL = white
- Shadow: adds dark outline for readability
- Style: `UI_LEFT`, `UI_CENTER`, `UI_RIGHT` for alignment

**CG_DrawBigStringColor()**: Larger font version, ~8 chars/line.

Both use pre-registered `cls.charSetShader` (monospace bitmap font, 16×16 grid of ASCII).

**Q3IDE Usage**: Clock, status text, build info on Widgets.

## 4. Underlying Renderer Call: trap_R_DrawStretchPic()

```c
void trap_R_DrawStretchPic(float x, float y, float w, float h,
                           float s1, float t1, float s2, float t2, qhandle_t hShader)
```

Direct renderer call (from tr_public.h `refexport_t.DrawStretchPic`):
- **x, y, w, h**: screen pixel coordinates (NOT virtual 640x480)
- **s1, t1, s2, t2**: texture UV range (0.0–1.0)
- **hShader**: shader handle

Called after `CG_DrawPic()` or direct calls perform `SCR_AdjustFrom640()`.

**Note**: `trap_R_SetColor()` modulates the texture; set before drawing.

## 5. Coordinate System: 640×480 Virtual → Pixel Conversion

Quake 3 uses fixed virtual resolution for HUD layout independence.

**Conversion formula** (in `SCR_AdjustFrom640`):
```c
float xscale = cls.glconfig.vidWidth / 640.0f;
float yscale = cls.glconfig.vidHeight / 480.0f;
pixel_x = virtual_x * xscale;
pixel_y = virtual_y * yscale;
```

Example: 1080p display (1920×1080):
- xscale = 3.0, yscale = 2.25
- Virtual (100, 100) → Pixel (300, 225)

**For Ornaments/Widgets**: Always work in virtual 640×480, call `SCR_AdjustFrom640()` before `trap_R_DrawStretchPic()`.

## 6. Projecting 3D Window Position → 2D Screen Coordinates

To draw a 2D element at a 3D window's on-screen position:

1. **Get 3D window center** (from window manager, e.g., `q3ide_wm.c`):
   ```c
   vec3_t window_pos;  // world-space 3D position
   ```

2. **Project to screen space** via renderer (if available):
   ```c
   // Hypothetical: trap_R_ProjectDecal(vec3_t pos, float *out_x, float *out_y)
   // Not directly exposed in cgame; use manual math instead:
   ```

3. **Manual projection** (view-space → screen):
   ```c
   // Assuming you have view/proj matrices from refdef_t:
   vec3_t view_pos;
   Matrix_Transform(window_pos, view_matrix, view_pos);
   float depth = view_pos[2];

   float screen_x = (view_pos[0] / depth) * (fov / aspect);
   float screen_y = (view_pos[1] / depth) * fov;

   // Convert from NDC [-1,1] to virtual [0,640] / [0,480]
   screen_x = (screen_x + 1.0) * 320.0;
   screen_y = (1.0 - screen_y) * 240.0;
   ```

   Or use engine's existing viewport transforms (if hooked).

**Q3IDE Practice**: Store 2D screen position in window manager state after each 3D render pass, reuse for Ornament drawing.

## 7. Projection Math Notes: trap_R_ProjectDecal vs Manual

**trap_R_ProjectDecal()** (if exposed):
- Renderer-internal; projects decals to world surfaces.
- Not available in cgame API; would require engine hook.

**Manual projection** (Q3IDE approach):
- Use window's stored 3D center + current view/proj matrices.
- Called during `CG_Draw2D()` to map `refdef_t` into virtual 2D coords.
- Store result in window state for subsequent Ornament/Widget draws.

**Alternative**: Hook after `re.RenderScene()` in `q3ide_render.c`:
- Each monitor pass stores viewport + projection matrix.
- Window manager captures that matrix for projection.
- cgame queries it during `CG_Draw2D()`.

## 8. Window Bar (Ornament): 8% Height Strip at Bottom

A visual status bar anchored to the bottom of a 3D window's on-screen position.

**Implementation** (pseudo-code in cgame):
```c
void CG_DrawWindowBar(int window_id, const char *title, float screen_x, float screen_y, float screen_w)
{
	float bar_h = screen_h * 0.08f;  // 8% of window height
	float bar_y = screen_y + screen_h - bar_h;  // bottom edge

	// Convert to virtual 640x480 space
	float virt_x = screen_x / (1920.0 / 640.0);  // scale-down example
	float virt_y = bar_y / (1080.0 / 480.0);
	float virt_w = screen_w / (1920.0 / 640.0);
	float virt_h = bar_h / (1080.0 / 480.0);

	// Draw background
	trap_R_SetColor((float[]){0.1, 0.1, 0.2, 0.8});
	CG_DrawPic(virt_x, virt_y, virt_w, virt_h, cls.whiteShader);

	// Draw title text
	trap_R_SetColor(NULL);  // white
	CG_DrawStringExt(virt_x + 4, virt_y + 4, title, NULL, qfalse, qfalse, -1, 0);
}
```

**Key points**:
- Bar position depends on window's projected 2D screen coordinates.
- Height is 8% of window's on-screen height.
- Background color: semi-transparent dark (alpha for visibility).
- Text: monospace, left-aligned, with padding.

## 9. Widget (Compact Overlay): 2D Quad with Text

Widgets are small status displays (clock, build counter, memory) overlaid as fixed-position HUD elements or anchored to windows.

**Implementation**:
```c
void CG_DrawWidget(float virt_x, float virt_y, float virt_w, float virt_h,
                   const char *label, const char *value)
{
	// Draw background quad
	trap_R_SetColor((float[]){0.05, 0.1, 0.15, 0.9});
	CG_DrawPic(virt_x, virt_y, virt_w, virt_h, cls.whiteShader);

	// Border
	trap_R_SetColor((float[]){0.4, 0.6, 1.0, 1.0});
	CG_DrawPic(virt_x, virt_y, virt_w, 2, cls.whiteShader);  // top
	CG_DrawPic(virt_x, virt_y + virt_h - 2, virt_w, 2, cls.whiteShader);  // bottom

	// Text: label on left, value on right
	trap_R_SetColor(NULL);
	CG_DrawStringExt(virt_x + 4, virt_y + 4, label, NULL, qfalse, qfalse, -1, UI_LEFT);
	CG_DrawStringExt(virt_x + virt_w - 4, virt_y + 4, value, NULL, qfalse, qfalse, -1, UI_RIGHT);
}
```

**Key points**:
- Uses `CG_DrawPic()` for backgrounds and borders.
- Text drawn with `CG_DrawStringExt()` for alignment control.
- Colors set via `trap_R_SetColor()` before each draw call.
- Typically fixed position in virtual space (e.g., top-right at (500, 10)).

## 10. Z-Ordering: Draw Order & Shader Sort Keys

**In 2D HUD (cgame's CG_Draw2D)**:
- No depth test; **order of calls determines stacking**.
- Last call = frontmost element.
- Example: draw backgrounds first, then borders, then text.

**In 3D poly rendering (q3ide_wm.c polyVerts)**:
- Shader's `sort` key controls order:
  - `SORT_BAD` = backmost
  - `SORT_PORTAL` / `SORT_SKY` = middle
  - `SORT_DECAL` / `SORT_SEETHROUGH` = frontmost
  - Higher sort = rendered later = frontmost

**For Q3IDE**:
- Window quads: use `SORT_SEETHROUGH` (sort key ~8) to appear over world.
- Ornament/Widget borders: drawn in cgame after window, inheriting HUD layering.
- If mixing 3D polys + 2D HUD, ensure window polys are `SORT_SEETHROUGH` and HUD is rendered after `re.RenderScene()`.

## Key Integration Points for Batch 9

1. **Query window's 2D position**: Store in window state during `Q3IDE_MultiMonitorRender()` or hook after each monitor pass.
2. **In cgame CG_Draw2D()**: Call ornament/widget draw functions with window's stored 2D coords.
3. **Use `SCR_AdjustFrom640()`**: All virtual 640×480 → pixel conversion.
4. **Set color before drawing**: `trap_R_SetColor()` modulates all subsequent `trap_R_DrawStretchPic()` calls.
5. **Text rendering**: Prefer `CG_DrawStringExt()` for aligned, monospace text.

---

**File references**: `quake3e/code/client/cl_scrn.c`, `quake3e/code/renderer/tr_backend.c`, `quake3e/code/cgame/cg_public.h`, `quake3e/code/q3ide/q3ide_render.c`.
