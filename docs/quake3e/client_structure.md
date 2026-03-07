# Quake3e Client Code - Directory Structure

> **Source:** https://github.com/ec-/Quake3e/tree/main/code/client
> **Fetched:** 2026-03-07
> **Purpose:** File listing for the client subsystem (`code/client/`), which manages rendering, input, audio, and game VM interaction.

---

## Directory: `code/client/`

### C Source Files (22 files)

#### Core Client
| File | Purpose |
|---|---|
| `cl_main.c` | **KEY FILE** - Client initialization (`CL_Init`), per-frame update (`CL_Frame`), shutdown, console command registration |
| `cl_cgame.c` | Client game VM interface - loads and communicates with cgame QVM/DLL |
| `cl_ui.c` | UI VM interface - loads and communicates with UI QVM/DLL |
| `cl_input.c` | Keyboard/mouse input processing, user command creation |
| `cl_keys.c` | Key binding system, key event handling |
| `cl_console.c` | In-game console rendering and input |
| `cl_scrn.c` | Screen update coordination, 2D drawing, screenshot capture |

#### Networking
| File | Purpose |
|---|---|
| `cl_net_chan.c` | Network channel management, reliable packet delivery |
| `cl_parse.c` | Server message parsing, gamestate/snapshot processing |
| `cl_curl.c` | HTTP download support via libcurl |

#### Media
| File | Purpose |
|---|---|
| `cl_cin.c` | Cinematic (RoQ video) playback |
| `cl_avi.c` | AVI video recording |
| `cl_jpeg.c` | JPEG screenshot encoding |

#### Audio System
| File | Purpose |
|---|---|
| `snd_main.c` | Audio system main entry, mixer coordination |
| `snd_dma.c` | DMA-based audio output |
| `snd_mix.c` | Audio sample mixing |
| `snd_mem.c` | Sound sample loading and memory management |
| `snd_adpcm.c` | ADPCM audio codec |
| `snd_wavelet.c` | Wavelet audio codec |
| `snd_codec.c` | Audio codec abstraction layer |
| `snd_codec_ogg.c` | OGG Vorbis codec implementation |
| `snd_codec_wav.c` | WAV file codec implementation |

### Header Files (7 files)

| File | Purpose |
|---|---|
| `client.h` | Main client header - `clientConnection_t`, `clientStatic_t`, `clientActive_t` structures |
| `cl_curl.h` | CURL download declarations |
| `keycodes.h` | Key code enumeration |
| `keys.h` | Key binding system declarations |
| `snd_codec.h` | Audio codec interface |
| `snd_local.h` | Internal audio system types |
| `snd_public.h` | Public audio API |

## Q3IDE Integration Notes

For Q3IDE, the most critical client files are:

- **`cl_main.c`** - Where `CL_Init()` registers console commands and `CL_Frame()` runs per-frame logic. Q3IDE console commands (`/q3ide_*`) would be registered here or in a new file called from here.
- **`cl_cgame.c`** - Interface to the client game VM. The cgame system call handler is here, which could be extended for Q3IDE-specific trap calls.
- **`cl_scrn.c`** - Screen update coordination. `SCR_UpdateScreen()` is called from `CL_Frame()` and triggers the full render pipeline.
- **`client.h`** - Contains the `refexport_t` reference used to call renderer functions. Q3IDE would use these function pointers to upload textures.
