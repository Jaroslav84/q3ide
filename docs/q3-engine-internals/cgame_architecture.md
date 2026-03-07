# Quake 3 CGGame (Client Game) Module Architecture

> **Sources:**
> - [Fabien Sanglard - Quake 3 Source Code Review: Architecture](https://fabiensanglard.net/quake3/)
> - [Fabien Sanglard - Quake 3 Source Code Review: Virtual Machine](https://fabiensanglard.net/quake3/qvm.php)
> - [ioquake3 Forum - Overall Architecture Discussion](https://discourse.ioquake.org/t/overall-architecture-coupling-between-game-cgame-etc/426)
> - [Quake III Arena Source Code - cl_cgame.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/client/cl_cgame.c)
> - [Quake III Arena Source Code - cg_main.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/cgame/cg_main.c)
> - [Quake 3 Virtual Machine Specifications](https://www.icculus.org/~phaethon/q3mc/q3vm_specs.html)

## Overview

Quake 3 has six modules that interact through a centralized event processing system:

| Module | Type | Purpose |
|--------|------|---------|
| `quake3.exe` | Executable | Main engine (input, networking, filesystem, sound) |
| `renderer` | Static Library | OpenGL rendering |
| `botlib` | Static Library | AI/bot system |
| `game` | VM (Dynamic Lib / QVM) | Server-side game logic |
| **`cgame`** | VM (Dynamic Lib / QVM) | **Client-side game logic** |
| `q3_ui` | VM (Dynamic Lib / QVM) | User interface |

The **cgame** (client game) module is the client-side game logic that runs as one of three virtual machines. It handles client prediction, entity interpolation, rendering setup, HUD drawing, and local effects.

## Module Boundaries

### Strict Separation

The architecture enforces strict separation between modules:

- **game** runs on the server, managing authoritative game state
- **cgame** runs on the client, presenting the game state visually
- **q3_ui** runs on the client, handling menus and UI
- **quake3.exe** provides services to all three through system calls

### Communication Channels

```
quake3.exe <--syscalls/vmcalls--> cgame (Client VM)
quake3.exe <--syscalls/vmcalls--> game  (Server VM)
quake3.exe <--syscalls/vmcalls--> q3_ui (UI VM)

game --> (snapshots via network) --> cgame
```

The game and cgame modules **never communicate directly**. All communication goes through the network protocol (snapshots) even in single-player, maintaining the client-server architecture.

## Virtual Machine System

### QVM (Quake Virtual Machine)

The three game modules (cgame, game, q3_ui) run inside virtual machines that provide:

- **Portability**: QVM bytecode runs on any platform
- **Security**: Sandboxed execution prevents malicious mods from accessing the system
- **Modularity**: Modules can be replaced without modifying the engine

In debug builds, the VMs are compiled as native DLLs (`.dll` / `.so` / `.dylib`) for easier debugging. In release builds, they are compiled to QVM bytecode.

### LCC Compiler

The Little C Compiler (LCC), an open-source ANSI C compiler, is used to compile C source code into QVM bytecode. The toolchain:

1. LCC compiles `.c` files to `.asm` (QVM assembly)
2. `q3asm` assembles `.asm` files into `.qvm` bytecode
3. The engine loads and executes `.qvm` files

## CGGame Entry Points (vmMain)

The `vmMain` function is the **only entry point** into the cgame module. It must be the very first function compiled into the `.qvm` file and is located at offset `0x2D` in the bytecode text segment.

```c
int vmMain(int command, int arg0, int arg1, ..., int arg11) {
    switch (command) {
        case CG_INIT:
            CG_Init(arg0, arg1, arg2);
            return 0;
        case CG_SHUTDOWN:
            CG_Shutdown();
            return 0;
        case CG_CONSOLE_COMMAND:
            return CG_ConsoleCommand();
        case CG_DRAW_ACTIVE_FRAME:
            CG_DrawActiveFrame(arg0, arg1, arg2);
            return 0;
        case CG_CROSSHAIR_PLAYER:
            return CG_CrosshairPlayer();
        case CG_LAST_ATTACKER:
            return CG_LastAttacker();
        case CG_KEY_EVENT:
            CG_KeyEvent(arg0, arg1);
            return 0;
        case CG_MOUSE_EVENT:
            CG_MouseEvent(arg0, arg1);
            return 0;
        case CG_EVENT_HANDLING:
            CG_EventHandling(arg0);
            return 0;
    }
    return -1;
}
```

### Key Messages

| Message | Purpose |
|---------|---------|
| `CG_INIT` | Initialize the cgame module (load media, set up state) |
| `CG_SHUTDOWN` | Clean up before module is unloaded |
| `CG_CONSOLE_COMMAND` | A registered cgame command was typed in console |
| `CG_DRAW_ACTIVE_FRAME` | Render the current frame (main render entry point) |
| `CG_CROSSHAIR_PLAYER` | Return the player entity the crosshair is over |
| `CG_LAST_ATTACKER` | Return the last player who attacked us |
| `CG_KEY_EVENT` | Key press/release event |
| `CG_MOUSE_EVENT` | Mouse movement event |
| `CG_EVENT_HANDLING` | UI event handling mode change |

### VM_Call Mechanism

The engine sends messages to the VM through `VM_Call`, which takes up to 11 parameters. Parameters are written as 4-byte values into the VM bytecode memory starting from offset `0x00` up to `0x26`, with the message ID written at `0x2A`.

## CGGame System Calls (Trap Functions)

The cgame module requests services from the engine through **system calls** (called "trap functions" on the cgame side). When the VM interpreter encounters a negative function index, it dispatches to the corresponding engine function.

### How Trap Calls Work

```
cgame code: trap_R_RegisterShader("myshader")
    |
    v
cg_syscalls.c: syscall(CG_R_REGISTERSHADER, name)
    |
    v
VM interpreter: detects negative index, calls systemCall()
    |
    v
cl_cgame.c: CL_CgameSystemCalls() dispatches to engine function
    |
    v
Engine: re.RegisterShader("myshader")
```

### System Call Categories

System call indices are assigned **negative integer values** to distinguish them from bytecode function addresses.

#### Print / Error
```c
void trap_Print(const char *fmt);              // CG_PRINT
void trap_Error(const char *fmt);              // CG_ERROR
```

#### Timing
```c
int  trap_Milliseconds(void);                  // CG_MILLISECONDS
```

#### Console / Cvar
```c
void trap_Cvar_Register(vmCvar_t *cvar, const char *name, const char *value, int flags);
void trap_Cvar_Update(vmCvar_t *cvar);
void trap_Cvar_Set(const char *name, const char *value);
void trap_SendConsoleCommand(const char *text);
void trap_AddCommand(const char *cmdName);
void trap_RemoveCommand(const char *cmdName);
void trap_SendClientCommand(const char *s);
```

#### Config Strings
```c
void trap_GetConfigstring(int index, char *buf, int bufsize);
```

#### Rendering
```c
void    trap_R_LoadWorldMap(const char *mapname);
qhandle_t trap_R_RegisterModel(const char *name);
qhandle_t trap_R_RegisterSkin(const char *name);
qhandle_t trap_R_RegisterShader(const char *name);          // CG_R_REGISTERSHADER (-40)
void    trap_R_ClearScene(void);
void    trap_R_AddRefEntityToScene(const refEntity_t *re);
void    trap_R_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts);
void    trap_R_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void    trap_R_RenderScene(const refdef_t *fd);
void    trap_R_SetColor(const float *rgba);
void    trap_R_DrawStretchPic(float x, float y, float w, float h,      // CG_R_DRAWSTRETCHPIC (-47)
                              float s1, float t1, float s2, float t2,
                              qhandle_t hShader);
int     trap_R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
```

#### Sound
```c
sfxHandle_t trap_S_RegisterSound(const char *sample, qboolean compressed);
void    trap_S_StartLocalSound(sfxHandle_t sfx, int channelNum);
void    trap_S_StartSound(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
void    trap_S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void    trap_S_UpdateEntityPosition(int entityNum, const vec3_t origin);
```

#### Collision / Traces
```c
void    trap_CM_BoxTrace(trace_t *results, const vec3_t start, const vec3_t end,
                         const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask);
void    trap_CM_TransformedBoxTrace(trace_t *results, const vec3_t start, const vec3_t end,
                                    const vec3_t mins, const vec3_t maxs, clipHandle_t model,
                                    int brushmask, const vec3_t origin, const vec3_t angles);
int     trap_CM_PointContents(const vec3_t point, clipHandle_t model);
```

#### Snapshot
```c
qboolean trap_GetSnapshot(int snapshotNumber, snapshot_t *snapshot);
qboolean trap_GetServerCommand(int serverCommandNumber);
int      trap_GetCurrentSnapshotNumber(int *snapshotNumber, int *serverTime);
```

## CG_DrawActiveFrame: The Main Render Loop

This is the most important cgame function, called every frame:

```c
void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback) {
    cg.time = serverTime;

    // Update cvars
    CG_UpdateCvars();

    // Predict player state (client prediction)
    CG_PredictPlayerState();

    // Calculate view position/angles
    CG_CalcViewValues();

    // Set up the refdef (rendering parameters)
    // Add entities to the scene
    CG_AddPacketEntities();
    CG_AddMarks();
    CG_AddLocalEntities();

    // Issue the render command
    trap_R_RenderScene(&cg.refdef);

    // Draw 2D overlays (HUD, crosshair, etc.)
    CG_Draw2D();
}
```

## CGGame State Management

### cg_t (Main State)

The primary state structure containing:
- Current server time
- Player prediction state
- View angles and position
- Crosshair entity tracking
- Local entity lists

### cgs_t (Static State)

Configuration that does not change during a game:
- Map name
- Game type
- Max clients
- Model/shader/sound handles

### centity_t (Client Entity)

Per-entity state including:
- Current and previous entity states (for interpolation)
- Entity type
- Animation state
- Effects and event history

## Key CGGame Source Files

| File | Purpose |
|------|---------|
| `cg_main.c` | Initialization, vmMain dispatch, cvar registration |
| `cg_draw.c` | 2D HUD rendering (health, armor, scores) |
| `cg_view.c` | View calculation, camera positioning |
| `cg_ents.c` | Entity processing and interpolation |
| `cg_event.c` | Game event handling (sounds, effects) |
| `cg_predict.c` | Client-side movement prediction |
| `cg_consolecmds.c` | Console command registration and dispatch |
| `cg_syscalls.c` | Trap function wrappers (system call interface) |
| `cg_weapons.c` | Weapon model rendering and effects |
| `cg_players.c` | Player model rendering and animation |
| `cg_local.h` | Local definitions, structures, function prototypes |
| `cg_public.h` | Public interface (vmMain message IDs) |

## Relevance to Q3IDE

The cgame architecture is critical for Q3IDE because:

1. **CG_DRAW_ACTIVE_FRAME** is where per-frame rendering happens -- Q3IDE needs to hook into this flow to update captured window textures each frame
2. **Trap functions** like `trap_R_RegisterShader` and `trap_R_AddPolyToScene` are how geometry is added to the scene -- Q3IDE will use similar calls to render window quads on walls
3. **trap_CM_BoxTrace** enables ray casting against world geometry to find walls for window placement
4. **Console commands** (`trap_AddCommand`) provide the mechanism for `/q3ide_*` commands

Since Q3IDE modifies the engine directly (not a pure mod), it has the option to work at either the engine level or the cgame level, or a combination of both.
