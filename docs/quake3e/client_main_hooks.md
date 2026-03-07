# Quake3e Client Main - Initialization and Per-Frame Hooks

> **Source:** https://raw.githubusercontent.com/ec-/Quake3e/main/code/client/cl_main.c
> **Fetched:** 2026-03-07
> **Purpose:** Detailed analysis of client initialization, per-frame hooks, console command registration, renderer initialization, and DLL loading. Critical for understanding where Q3IDE hooks into the engine.

---

## 1. Client Initialization: `CL_Init()`

```c
void CL_Init( void )
```

### What it does:
- Initializes the console system
- Clears client state (`cls`, `clc`)
- Registers approximately 50+ console variables (cvars)
- Registers console commands via `Cmd_AddCommand()`
- Sets up client state machine

### Key Cvars Registered:

**Network:**
- `cl_timeout` - Connection timeout
- `cl_timeNudge` - Time nudge for prediction
- `cl_shownet` - Network debugging display

**Demo/Recording:**
- `cl_autoRecordDemo` - Auto-record demos
- `cl_aviFrameRate` - Video capture frame rate
- `cl_aviMotionJpeg` - Use motion JPEG for video

**Download:**
- `cl_allowDownload` - Allow file downloads
- `cl_mapAutoDownload` - Auto-download maps

**Display:**
- `cl_conXOffset` - Console X offset
- `r_mode` - Video mode
- `r_fullscreen` - Fullscreen toggle

**Input:**
- Various keyboard/mouse settings

### Console Commands Registered:

**Network/Server:**
| Command | Description |
|---|---|
| `connect` | Connect to a server (supports `-4`/`-6` for address family) |
| `disconnect` | Disconnect from server |
| `rcon` | Remote console (with tab completion) |
| `ping` | Ping a server |
| `servers` | List servers |
| `localservers` | List LAN servers |
| `globalservers` | List global servers |
| `getinfo` | Get server info |
| `getstatus` | Get server status |

**Demo:**
| Command | Description |
|---|---|
| `record` | Start demo recording (with tab completion) |
| `stoprecord` | Stop demo recording |
| `demo` | Play a demo (with tab completion) |

**Video:**
| Command | Description |
|---|---|
| `video` | Start video recording |
| `video-pipe` | Start video recording via ffmpeg pipe |
| `stopvideo` | Stop video recording |

**Information/Debug:**
| Command | Description |
|---|---|
| `configstrings` | Dump config strings |
| `clientinfo` | Show client info |
| `serverinfo` | Show server info |
| `systeminfo` | Show system info |
| `showip` | Show IP address |
| `modelist` | List video modes |
| `pk3list` | List loaded PK3 files |
| `pureList` | List pure PK3 files |

**Rendering/System:**
| Command | Description |
|---|---|
| `vid_restart` | Full video restart (reloads renderer) |
| `vid_restart_fast` | Fast video restart (preserves context) |
| `snd_restart` | Sound system restart |
| `setModel` | Set player model |

**Q3IDE would add commands here:**
```c
// In CL_Init() or a new Q3IDE_Init() called from CL_Init():
Cmd_AddCommand( "q3ide_list", Q3IDE_List_f );
Cmd_AddCommand( "q3ide_attach", Q3IDE_Attach_f );
Cmd_AddCommand( "q3ide_detach", Q3IDE_Detach_f );
Cmd_AddCommand( "q3ide_status", Q3IDE_Status_f );
```

---

## 2. Per-Frame Update: `CL_Frame()`

```c
void CL_Frame( int msec, int realMsec )
```

### What it does each frame:

```c
void CL_Frame( int msec, int realMsec ) {
    // 1. Early exit if client not running
    if ( !com_cl_running->integer ) {
        return;
    }

    // 2. Store real frame time
    cls.realFrametime = realMsec;

    // 3. Handle CD dialog (legacy)
    if ( cls.cddialog ) {
        cls.cddialog = qfalse;
        VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_NEED_CD );
    }

    // 4. Video recording frame locking
    if ( CL_VideoRecording() && msec ) {
        float fps = MIN( cl_aviFrameRate->value / com_timescale->value, 1000.0f );
        frameDuration = MAX( 1000.0f / fps, 1.0f ) + clc.aviVideoFrameRemainder;
        CL_TakeVideoFrame();
        msec = (int)frameDuration;
    }

    // 5. Auto-record demos
    if ( cl_autoRecordDemo->integer && !clc.demoplaying ) {
        if ( cls.state == CA_ACTIVE && !clc.demorecording ) {
            Cbuf_ExecuteText( EXEC_NOW, va( "record %s-%s-%s",
                nowString, serverName, mapName ) );
        }
    }

    // 6. Update timing
    cls.frametime = msec;
    cls.realtime += msec;

    // 7. Core per-frame operations
    CL_CheckUserinfo();     // Check if userinfo changed
    CL_CheckTimeout();      // Check for network timeout
    CL_SendCmd();           // Send user commands to server
    CL_CheckForResend();    // Check if connection needs resend
    CL_SetCGameTime();      // Synchronize cgame time

    // 8. Increment frame counter
    cls.framecount++;

    // 9. Render
    SCR_UpdateScreen();     // ** THIS TRIGGERS THE FULL RENDER PIPELINE **

    // 10. Audio
    S_Update( realMsec );

    // 11. Cinematics
    SCR_RunCinematic();

    // 12. Console
    Con_RunConsole();
}
```

### Q3IDE Hook Point in CL_Frame:

The ideal place to update Q3IDE capture textures is **before `SCR_UpdateScreen()`** (step 9):

```c
    // ... existing code ...
    cls.framecount++;

    // Q3IDE: Update capture textures from ring buffer
    Q3IDE_UpdateCaptures();  // <-- INSERT HERE

    SCR_UpdateScreen();      // Render with updated textures
    // ... existing code ...
```

### `SCR_UpdateScreen()` call chain:

```
CL_Frame()
  -> SCR_UpdateScreen()
    -> re.BeginFrame()           // Begin render frame
    -> CL_DrawActiveGame()       // Draw 3D world + HUD
      -> VM_Call(cgvm, CG_DRAW_ACTIVE_FRAME)  // cgame draws the scene
        -> CG_DrawActiveFrame()
          -> CG_DrawWorld()      // BSP world + entities
          -> CG_Draw2D()         // HUD overlay
    -> SCR_DrawConsole()         // Console overlay
    -> re.EndFrame()             // Swap buffers / present
```

---

## 3. Renderer Initialization: `CL_InitRenderer()` and `CL_InitRef()`

### `CL_InitRenderer()`

```c
static void CL_InitRenderer( void )
```

**What it does:**
1. If renderer not yet loaded, calls `CL_InitRef()` to load it
2. Calls `re.BeginRegistration( &cls.glconfig )` to initialize renderer
3. Loads essential shaders:
   - `cls.charSetShader = re.RegisterShader( "gfx/2d/bigchars" )`
   - `cls.whiteShader = re.RegisterShader( "white" )`
   - `cls.consoleShader = re.RegisterShader( "console" )`
4. Calculates screen scaling for 640x480 virtual resolution
5. Calls `SCR_Init()` for screen system initialization

### `CL_InitRef()` - Renderer Loading

```c
static void CL_InitRef( void )
```

**Dynamic renderer loading (`USE_RENDERER_DLOPEN`):**
```c
#ifdef USE_RENDERER_DLOPEN
    GetRefAPI_t GetRefAPI;
    char dllName[ MAX_OSPATH ];

    // Construct renderer DLL name based on cl_renderer cvar
    // e.g., "renderer_opengl_aarch64.dylib" or "renderer_vulkan_x86_64.so"
    Com_sprintf( dllName, sizeof( dllName ),
        RENDERER_PREFIX "_%s_" REND_ARCH_STRING DLL_EXT,
        cl_renderer->string );

    rendererLib = Sys_LoadLibrary( ospath );
    GetRefAPI = Sys_LoadFunction( rendererLib, "GetRefAPI" );
#endif
```

**Note:** On macOS, `USE_RENDERER_DLOPEN` is **disabled** by default. The renderer is statically linked.

**Engine-to-renderer interface (`refimport_t`):**
The engine constructs a `refimport_t` table with function pointers:
```c
refimport_t ri;
ri.Cmd_AddCommand = Cmd_AddCommand;
ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
ri.Cmd_Argc = Cmd_Argc;
ri.Cmd_Argv = Cmd_Argv;
ri.Cvar_Get = Cvar_Get;
ri.Cvar_Set = Cvar_Set;
ri.FS_ReadFile = FS_ReadFile;
ri.FS_FreeFile = FS_FreeFile;
ri.Hunk_Alloc = Hunk_Alloc;
ri.Printf = CL_RefPrintf;
ri.Error = Com_Error;
// ... more function pointers
```

**Renderer-to-engine interface (`refexport_t`):**
```c
refexport_t re;
re = GetRefAPI( REF_API_VERSION, &ri );
// Now the engine can call:
//   re.RegisterShader(), re.AddRefEntityToScene(),
//   re.AddPolyToScene(), re.RenderScene(), etc.
```

---

## 4. CGame/UI VM Loading

### `CL_InitCGame()`

Called when entering a game (after connecting to server):
- Loads the cgame QVM or native DLL
- Sets up system call handler (`CL_CgameSystemCalls`)
- The cgame module handles all in-game rendering decisions

### `CL_InitUI()`

Called during initialization:
- Loads the UI QVM or native DLL
- Sets up system call handler
- Handles menus and pre-game UI

---

## 5. Shutdown: `CL_ShutdownAll()`

```c
void CL_ShutdownAll( void )
```

**What it does:**
1. Shuts down CURL (HTTP downloads)
2. Disables audio
3. Shuts down VMs (cgame, UI)
4. Shuts down renderer:
   ```c
   if ( re.Shutdown ) {
       if ( CL_GameSwitch() ) {
           CL_ShutdownRef( REF_DESTROY_WINDOW );
       } else {
           re.Shutdown( REF_KEEP_CONTEXT );
       }
   }
   ```
5. Clears state flags

**Q3IDE would add shutdown here:**
```c
Q3IDE_Shutdown();  // Free capture textures, disconnect from dylib
```

---

## 6. Extension Points and Hooks

### `activeAction` Cvar

```c
cl_activeAction = Cvar_Get( "activeAction", "", CVAR_TEMP );
// "Contents of this variable will be executed upon first frame of play"
```

This is executed when the client first enters an active game state. Q3IDE could use this
to auto-attach windows on spawn.

### Virtual Machine Callbacks

The cgame VM communicates with the engine through a system call interface:
- `CG_DRAW_ACTIVE_FRAME` - Called each frame to draw the game
- `CG_INIT` - Called when cgame initializes
- `CG_SHUTDOWN` - Called when cgame shuts down

### Network Packet Handlers

- `CL_ConnectionlessPacket()` - Out-of-band protocol handling
- `CL_PacketEvent()` - Sequenced packet processing
- `CL_ParseServerMessage()` - Game message parsing

### `vid_restart` / `vid_restart_fast`

These commands trigger a full or fast renderer restart. Q3IDE must handle these
by recreating capture textures after the renderer is reinitialized.

---

## 7. Q3IDE Integration Summary

### Initialization Flow

```
CL_Init()
  +-- Register Q3IDE cvars (q3ide_enabled, etc.)
  +-- Register Q3IDE commands (q3ide_list, q3ide_attach, etc.)
  +-- Q3IDE_Init()  -- Load capture dylib, enumerate windows

CL_InitRenderer()
  +-- Q3IDE_InitTextures()  -- Create GPU textures for captures
```

### Per-Frame Flow

```
CL_Frame()
  +-- Q3IDE_UpdateCaptures()  -- Pull frames from ring buffer, upload to GPU
  +-- SCR_UpdateScreen()
      +-- re.BeginFrame()
      +-- CL_DrawActiveGame()
          +-- cgame draws world
          +-- Q3IDE_DrawPanels()  -- Draw capture quads on walls
      +-- re.EndFrame()
```

### Shutdown Flow

```
CL_ShutdownAll()
  +-- Q3IDE_Shutdown()  -- Free textures, unload dylib
```

### Console Commands

```
/q3ide_list     -- List available macOS windows
/q3ide_attach   -- Attach window capture to nearest wall
/q3ide_detach   -- Detach current capture
/q3ide_status   -- Show capture status and performance
```
