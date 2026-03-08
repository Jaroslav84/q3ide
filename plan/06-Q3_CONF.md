# Quake 3 Graphics Configuration Presets

Three aggressiveness levels for 1080p. Pick based on your GPU.

---

## Quick Reference Table

| Setting | Conservative | Balanced | Aggressive |
|---------|:------------:|:--------:|:----------:|
| r_pbr | 0 | 1 | 1 |
| r_parallaxMapping | 0 | 1 | 1 |
| r_ssao | 0 | 1 | 1 |
| r_ext_multisample | 0 | 2 | 8 |
| r_shadowMapSize | 512 | 1024 | 2048 |
| r_cubemapSize | 64 | 128 | 512 |
| r_hdr | 0 | 1 | 1 |
| r_lodbias | 0 | -1 | -2 |
| r_ext_max_anisotropy | 8 | 16 | 16 |
| r_genNormalMaps | 0 | 0 | 1 |
| r_imageUpsample | 0 | 0 | 1 |

---

## Preset 1: CONSERVATIVE

**Target:** 60+ FPS on older GPUs (RX 580 / GTX 1060 class)

```cfg
// DISPLAY
r_mode -1
r_customwidth 1920
r_customheight 1080
r_fullscreen 1
cg_viewsize 100

// TEXTURE QUALITY
r_picmip 0
r_texturebits 32
r_textureMode GL_LINEAR_MIPMAP_LINEAR
r_ext_texture_filter_anisotropic 1
r_ext_max_anisotropy 8
r_roundImagesDown 1
r_detailtextures 1

// RENDERER
r_renderer 2
r_pbr 0
r_normalMapping 1
r_specularMapping 0
r_parallaxMapping 0
r_deluxeMapping 1

// LIGHTING
r_gamma 1.1
r_overBrightBits 1
r_intensity 1.1
r_mapOverBrightBits 2
r_dynamiclight 1
r_dlightMode 0

// ANTI-ALIASING
r_ext_framebuffer_multisample 0
r_ext_multisample 0

// POST-PROCESSING
r_hdr 0
r_toneMap 0
r_autoExposure 0
r_postProcess 0

// SHADOWS
r_sunShadows 0
r_shadowFilter 0
r_shadowBlur 0

// AMBIENT OCCLUSION
r_ssao 0

// GEOMETRY
r_lodbias 0
r_subdivisions 4
r_lodCurveError 250

// SKY
r_fastsky 0
r_flares 1
r_drawSun 0

// INPUT
m_pitch -0.022
sensitivity 8
```

---

## Preset 2: BALANCED

**Target:** 60 FPS on mid-range GPUs (RX 580 / GTX 1660 / RTX 2060)

```cfg
// DISPLAY
r_mode -1
r_customwidth 1920
r_customheight 1080
r_fullscreen 1
cg_viewsize 100

// TEXTURE QUALITY
r_picmip 0
r_texturebits 32
r_textureMode GL_LINEAR_MIPMAP_LINEAR
r_ext_texture_filter_anisotropic 1
r_ext_max_anisotropy 16
r_roundImagesDown 0
r_detailtextures 1

// RENDERER
r_renderer 2
r_pbr 1
r_normalMapping 1
r_specularMapping 1
r_parallaxMapping 1
r_deluxeMapping 1
r_baseNormalX 1.0
r_baseNormalY 1.0
r_baseParallax 0.03
r_deluxeSpecular 1.5

// LIGHTING
r_gamma 1.1
r_overBrightBits 1
r_intensity 1.2
r_mapOverBrightBits 2
r_dynamiclight 1
r_dlightBacks 1
r_dlightMode 1

// ANTI-ALIASING
r_ext_framebuffer_multisample 1
r_ext_multisample 2

// POST-PROCESSING
r_hdr 1
r_toneMap 1
r_autoExposure 1
r_postProcess 1

// SHADOWS
r_sunShadows 1
r_shadowFilter 1
r_shadowBlur 1
r_shadowMapSize 1024

// AMBIENT OCCLUSION
r_ssao 1

// REFLECTIONS
r_cubeMapping 1
r_cubemapSize 128

// GEOMETRY
r_lodbias -1
r_subdivisions 4
r_lodCurveError 250

// SKY
r_fastsky 0
r_flares 1
r_drawSun 1

// INPUT
m_pitch -0.022
sensitivity 8
```

---

## Preset 3: AGGRESSIVE

**Target:** Max quality on powerful GPUs (RX 6800 / RTX 3070+)

```cfg
// DISPLAY
r_mode -1
r_customwidth 1920
r_customheight 1080
r_fullscreen 1
cg_viewsize 100

// TEXTURE QUALITY
r_picmip 0
r_texturebits 32
r_textureMode GL_LINEAR_MIPMAP_LINEAR
r_ext_texture_filter_anisotropic 1
r_ext_max_anisotropy 16
r_roundImagesDown 0
r_detailtextures 1

// AUTO-UPSCALE OLD TEXTURES
r_imageUpsample 1
r_imageUpsampleMaxSize 1024
r_imageUpsampleType 1
r_genNormalMaps 1

// RENDERER
r_renderer 2
r_pbr 1
r_normalMapping 1
r_specularMapping 1
r_parallaxMapping 1
r_deluxeMapping 1
r_baseNormalX 1.0
r_baseNormalY 1.0
r_baseParallax 0.05
r_deluxeSpecular 2.0

// LIGHTING
r_gamma 1.1
r_overBrightBits 1
r_intensity 1.2
r_mapOverBrightBits 2
r_dynamiclight 1
r_dlightBacks 1
r_dlightMode 1

// ANTI-ALIASING
r_ext_framebuffer_multisample 1
r_ext_multisample 8

// POST-PROCESSING
r_hdr 1
r_floatLightmap 1
r_toneMap 1
r_autoExposure 1
r_postProcess 1

// SHADOWS
r_sunShadows 1
r_shadowFilter 1
r_shadowBlur 1
r_shadowMapSize 2048

// AMBIENT OCCLUSION
r_ssao 1

// REFLECTIONS
r_cubeMapping 1
r_cubemapSize 512

// GEOMETRY
r_lodbias -2
r_subdivisions 4
r_lodCurveError 250

// SKY
r_fastsky 0
r_flares 1
r_drawSun 1
r_drawSunRays 1

// INPUT
m_pitch -0.022
sensitivity 8
```

---

## How to Apply

1. Copy preset to `baseq3/autoexec.cfg`
2. Delete `baseq3/q3config.cfg`
3. Restart game

Or in console:
```
exec autoexec.cfg
vid_restart
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Black textures | `r_pbr 0` |
| Too dark | `r_gamma 1.3` or `r_intensity 1.5` |
| Low FPS | Drop to lower preset |
| Texture flickering | `r_parallaxMapping 0` |
| MSAA not working | `r_ext_multisample 0` |
| Shadows blocky | `r_shadowMapSize 512` |

---

## Your Hardware: AMD RX 580 8GB

**Recommended: BALANCED preset**

If FPS drops below 60:
- Set `r_ext_multisample 0`
- Set `r_ssao 0`
- Or switch to CONSERVATIVE
