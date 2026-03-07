# Q3 HD Upgrade Procedure

## What You Already Have (renderer2)

Quake3e's `renderer2` is surprisingly modern:

```
r_pbr 1              // Physically-based rendering
r_normalMapping 1    // Loads _n or _nh textures
r_specularMapping 1  // Loads _s textures
r_parallaxMapping 1  // Relief mapping from _nh alpha
```

The shader (`lightall_fp.glsl`) already does:
- Disney diffuse BRDF
- Specular with roughness
- Metallic workflow
- Cubemap reflections with parallax correction
- Shadow mapping

**So you're not starting from zero.**

---

## Your Real Options

### Option 1: Just Add Textures (Easiest)
Create textures with the right suffixes and drop them in:
- `textures/gothic_block/block10.jpg` → diffuse
- `textures/gothic_block/block10_n.tga` → normal map
- `textures/gothic_block/block10_s.tga` → specular (R=roughness, G=metallic in PBR mode)
- `textures/gothic_block/block10_nh.tga` → normal+height for parallax

**Effort:** Just make/buy textures. No code changes.

---

### Option 2: Add Material Override System (Medium)
Like Q2RTX's approach - add an `overrides/` folder where HD textures live:
```
baseq3/
  overrides/
    textures/gothic_block/block10.png  // 4K version
    textures/gothic_block/block10_n.png
```

The engine checks `overrides/` first, falls back to original. This lets you ship HD packs separately.

**Effort:** ~50 lines in `tr_image.c`, modify `R_FindImageFile()`

---

### Option 3: Add .mat Files (More Work)
External material definitions like Q2RTX:
```
// textures/gothic_block/block10.mat
textures/gothic_block/block10:
    roughness 0.4
    metallic 0.0
    normal_scale 1.0 1.0
    emissive_threshold 0.8
```

**Effort:** New parser, material registry, hook into shader loading. Maybe 200-300 lines.

---

### Option 4: Auto-Generate Missing Maps (Fancy)
When a normal map doesn't exist, generate one from the diffuse using:
- Simple Sobel filter (fast, okay results)
- Or load a pre-baked normal from a community pack

Same for emissive - detect bright pixels in diffuse and extract.

**Effort:** Image processing code in `tr_image.c`. 100-200 lines.

---

## Honest Take

You already have **90% of what you need**. The shader is there. The cvars are there.

The gap is:
1. **No HD textures exist for Q3** (unlike Q2's community packs)
2. **No automatic override loading** (have to replace original files)
3. **Material properties are shader-only** (no external .mat files)

---

## Key Files

| File | Purpose |
|------|---------|
| `renderer2/tr_image.c` | Texture loading, suffix handling |
| `renderer2/tr_shader.c` | Shader parsing, PBR properties |
| `renderer2/glsl/lightall_fp.glsl` | PBR shader implementation |
| `renderer2/tr_common.h` | `image_t` struct, texture types |

---

## Reference: Q2RTX Material System

From NVIDIA's Q2RTX (GPL-2.0, same license as Q3):

- Materials defined in `*.mat` files
- Auto-search for `_n`, `_light` suffixes
- Properties: `roughness`, `metallic`, `emissive`, `is_light`
- Override folder priority

GitHub: https://github.com/NVIDIA/Q2RTX

---

## Current Texture Suffix Conventions

The engine already auto-loads these when `r_normalMapping` and `r_specularMapping` are enabled:

| Suffix | Type | Usage |
|--------|------|-------|
| `_n` | Normal map | XY normals in RG channels |
| `_nh` | Normal + Height | XY normals in RG, height in A (for parallax) |
| `_s` | Specular | RGB=specular color, A=gloss (or R=roughness, G=metallic with `r_pbr 1`) |

---

## Shader Keywords (in .shader files)

```
diffusemap    textures/example/base
normalmap     textures/example/base_n
specularmap   textures/example/base_s

// PBR properties (renderer2)
roughness     0.4
metallic      0.0
gloss         0.6
normalScale   1.0 1.0 0.5    // X Y height
specularScale 0.5 0.8 1.0 0.6 // R G B gloss (or metallic smoothness _ _ with r_pbr)
parallaxDepth 0.05
```

---

## Quick Start: Enable PBR

Add to `baseq3/autoexec.cfg`:

```
// PBR Rendering
r_renderer 2           // Use renderer2 (GLSL)
r_pbr 1                // Enable PBR mode
r_normalMapping 1      // Load normal maps
r_specularMapping 1    // Load specular maps
r_parallaxMapping 1    // Enable parallax/relief mapping
r_deluxeMapping 1      // Use deluxemaps if available
r_baseNormalX 1.0
r_baseNormalY 1.0
r_baseParallax 0.05
r_deluxeSpecular 1.5
```

Then restart the game.
