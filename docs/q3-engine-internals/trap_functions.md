# Quake 3 Trap Functions (VM System Calls)

> **Sources:**
> - [Quake III Arena Source Code - cg_syscalls.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/cgame/cg_syscalls.c)
> - [Quake III Arena Source Code - cg_syscalls.asm](https://github.com/id-Software/Quake-III-Arena/blob/master/code/cgame/cg_syscalls.asm)
> - [Quake III Arena Source Code - cl_cgame.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/client/cl_cgame.c)
> - [Quake3World Forum - Add a trap function](https://www.quake3world.com/forum/viewtopic.php?t=32037)
> - [ioquake3 Forum - Trap_* prefix on functions](https://discourse.ioquake.org/t/trap-prefix-on-functions/1007)
> - [Fabien Sanglard - Quake 3 Virtual Machine](https://fabiensanglard.net/quake3/qvm.php)
> - [GuidedHacking - Understanding Quake VM and Syscalls](https://guidedhacking.com/threads/understanding-quake-vm-and-syscalls.14357/)

## Overview

Trap functions (also called system calls or syscalls) are the mechanism by which Quake 3's virtual machine modules (cgame, game, q3_ui) request services from the main engine. The name "trap" comes from the concept of a system trap -- the VM "traps" out of sandboxed bytecode execution into native engine code.

## Architecture

### Communication Model

```
VM Code (cgame)                     Engine (quake3.exe)
===============                     ===================
trap_R_RegisterShader("tex")  --->  CL_CgameSystemCalls()
                                         |
                                         v
                                    re.RegisterShader("tex")
                                         |
                                         v
                              <---  Returns qhandle_t
```

### Dispatch Mechanism

1. The VM calls a wrapper function (e.g., `trap_R_RegisterShader`)
2. The wrapper calls `syscall()` with a **negative integer** identifier and parameters
3. The VM interpreter detects the negative function index (CALLI4 opcode)
4. The interpreter calls the `systemCall` function pointer (set during VM init via `dllEntry()`)
5. `CL_CgameSystemCalls()` in `cl_cgame.c` acts as a dispatcher, routing to the correct engine function
6. The result is returned to the VM

### Why Negative Indices?

System calls use negative integer values so the interpreter can distinguish them from bytecode function addresses (which are positive offsets into the code segment). When the interpreter encounters a call to a negative address, it knows to dispatch to the engine rather than jumping to bytecode.

## CGGame Trap Functions -- Complete Reference

### System / Print

| Syscall ID | Trap Function | Description |
|------------|---------------|-------------|
| CG_PRINT | `trap_Print(const char *fmt)` | Print message to console |
| CG_ERROR | `trap_Error(const char *fmt)` | Abort with error message |
| CG_MILLISECONDS | `trap_Milliseconds(void)` | Get engine time in milliseconds |
| CG_REAL_TIME | `trap_RealTime(qtime_t *qtime)` | Get real wall-clock time |
| CG_SNAPVECTOR | `trap_SnapVector(float *v)` | Snap float vector to integers |
| CG_MEMSET | `trap_Memset(void *dest, int c, int count)` | Memory set |
| CG_MEMCPY | `trap_Memcpy(void *dest, const void *src, int count)` | Memory copy |

### Console / Cvar

| Syscall ID | Trap Function | Description |
|------------|---------------|-------------|
| CG_CVAR_REGISTER | `trap_Cvar_Register(vmCvar_t*, name, value, flags)` | Register a cvar |
| CG_CVAR_UPDATE | `trap_Cvar_Update(vmCvar_t *cvar)` | Update cached cvar value |
| CG_CVAR_SET | `trap_Cvar_Set(const char *name, const char *value)` | Set a cvar value |
| CG_CVAR_VARIABLESTRINGBUFFER | `trap_Cvar_VariableStringBuffer(name, buf, bufsize)` | Get cvar string value |
| CG_ADDCOMMAND | `trap_AddCommand(const char *cmdName)` | Register a console command |
| CG_REMOVECOMMAND | `trap_RemoveCommand(const char *cmdName)` | Unregister a console command |
| CG_SENDCONSOLECOMMAND | `trap_SendConsoleCommand(const char *text)` | Execute a console command |
| CG_SENDCLIENTCOMMAND | `trap_SendClientCommand(const char *s)` | Send command to server |
| CG_ARGC | `trap_Argc(void)` | Get argument count |
| CG_ARGV | `trap_Argv(int n, char *buf, int bufLen)` | Get argument by index |
| CG_ARGS | `trap_Args(char *buf, int bufLen)` | Get all arguments |

### Config Strings

| Syscall ID | Trap Function | Description |
|------------|---------------|-------------|
| CG_GETGAMESTATE | `trap_GetGameState(gameState_t *gs)` | Get current game state |
| CG_GETSERVERCOMMAND | `trap_GetServerCommand(int serverCmdNum)` | Get a server command |
| CG_GETCURRENTSNAPSHOTNUMBER | `trap_GetCurrentSnapshotNumber(int*, int*)` | Get current snapshot info |
| CG_GETSNAPSHOT | `trap_GetSnapshot(int snapNum, snapshot_t *snap)` | Get a game snapshot |
| CG_GETCONFIGSTRING | `trap_GetConfigstring(int index, char *buf, int size)` | Get a config string |

### Rendering

| Syscall ID | Trap Function | Description |
|------------|---------------|-------------|
| CG_R_LOADWORLDMAP | `trap_R_LoadWorldMap(const char *mapname)` | Load BSP map |
| CG_R_REGISTERMODEL | `trap_R_RegisterModel(const char *name)` | Register MD3 model |
| CG_R_REGISTERSKIN | `trap_R_RegisterSkin(const char *name)` | Register skin |
| CG_R_REGISTERSHADER | `trap_R_RegisterShader(const char *name)` | Register shader (ID: -40) |
| CG_R_REGISTERSHADERNOMIP | `trap_R_RegisterShaderNoMip(const char *name)` | Register shader (no mipmaps) |
| CG_R_CLEARSCENE | `trap_R_ClearScene(void)` | Clear render scene |
| CG_R_ADDREFENTITYTOSCENE | `trap_R_AddRefEntityToScene(const refEntity_t *re)` | Add entity to scene |
| CG_R_ADDPOLYTOSCENE | `trap_R_AddPolyToScene(qhandle_t shader, int numVerts, const polyVert_t *verts)` | Add polygon to scene |
| CG_R_ADDLIGHTTOSCENE | `trap_R_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b)` | Add dynamic light |
| CG_R_RENDERSCENE | `trap_R_RenderScene(const refdef_t *fd)` | Render the 3D scene |
| CG_R_SETCOLOR | `trap_R_SetColor(const float *rgba)` | Set 2D drawing color |
| CG_R_DRAWSTRETCHPIC | `trap_R_DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader)` | Draw 2D image (ID: -47) |
| CG_R_MODELBOUND | `trap_R_ModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs)` | Get model bounds |
| CG_R_LERPTAG | `trap_R_LerpTag(orientation_t*, clipHandle_t, int, int, float, const char*)` | Interpolate model tag |
| CG_R_LIGHTFORPOINT | `trap_R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir)` | Get lighting at point |

### Sound

| Syscall ID | Trap Function | Description |
|------------|---------------|-------------|
| CG_S_REGISTERSOUND | `trap_S_RegisterSound(const char *sample, qboolean compressed)` | Register sound |
| CG_S_STARTLOCALSOUND | `trap_S_StartLocalSound(sfxHandle_t sfx, int channelNum)` | Play local sound |
| CG_S_STARTSOUND | `trap_S_StartSound(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx)` | Play 3D sound |
| CG_S_CLEARLOOPINGSOUNDS | `trap_S_ClearLoopingSounds(qboolean killall)` | Clear looping sounds |
| CG_S_ADDLOOPINGSOUND | `trap_S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx)` | Add looping sound |
| CG_S_ADDREALLOOPINGSOUND | `trap_S_AddRealLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx)` | Add real looping sound |
| CG_S_STOPLOOPINGSOUND | `trap_S_StopLoopingSound(int entityNum)` | Stop looping sound |
| CG_S_UPDATEENTITYPOSITION | `trap_S_UpdateEntityPosition(int entityNum, const vec3_t origin)` | Update sound source position |
| CG_S_RESPATIALIZE | `trap_S_Respatialize(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater)` | Update listener position |
| CG_S_STARTBACKGROUNDTRACK | `trap_S_StartBackgroundTrack(const char *intro, const char *loop)` | Start music track |
| CG_S_STOPBACKGROUNDTRACK | `trap_S_StopBackgroundTrack(void)` | Stop music track |

### Collision / Traces

| Syscall ID | Trap Function | Description |
|------------|---------------|-------------|
| CG_CM_LOADMAP | `trap_CM_LoadMap(const char *mapname)` | Load collision model |
| CG_CM_NUMINLINEMODELS | `trap_CM_NumInlineModels(void)` | Number of brush models |
| CG_CM_INLINEMODEL | `trap_CM_InlineModel(int index)` | Get brush model handle |
| CG_CM_TEMPBOXMODEL | `trap_CM_TempBoxModel(const vec3_t mins, const vec3_t maxs)` | Create temp box for collision |
| CG_CM_POINTCONTENTS | `trap_CM_PointContents(const vec3_t point, clipHandle_t model)` | Get contents at point |
| CG_CM_TRANSFORMEDPOINTCONTENTS | `trap_CM_TransformedPointContents(const vec3_t point, clipHandle_t model, const vec3_t origin, const vec3_t angles)` | Transformed point contents |
| CG_CM_BOXTRACE | `trap_CM_BoxTrace(trace_t *results, start, end, mins, maxs, model, brushmask)` | Box/ray trace |
| CG_CM_TRANSFORMEDBOXTRACE | `trap_CM_TransformedBoxTrace(trace_t*, start, end, mins, maxs, model, brushmask, origin, angles)` | Transformed box trace |
| CG_CM_MARKFRAGMENTS | `trap_CM_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)` | Project marks (decals) onto surfaces |

### Filesystem

| Syscall ID | Trap Function | Description |
|------------|---------------|-------------|
| CG_FS_FOPENFILE | `trap_FS_FOpenFile(const char *qpath, fileHandle_t *f, fsMode_t mode)` | Open file |
| CG_FS_READ | `trap_FS_Read(void *buffer, int len, fileHandle_t f)` | Read from file |
| CG_FS_WRITE | `trap_FS_Write(const void *buffer, int len, fileHandle_t f)` | Write to file |
| CG_FS_FCLOSEFILE | `trap_FS_FCloseFile(fileHandle_t f)` | Close file |

## Adding New Trap Functions

To add a new trap function (extending the VM-engine interface):

### Step 1: Define the Syscall ID

In `cg_public.h`, add a new negative enum value:

```c
typedef enum {
    // ... existing syscalls ...
    CG_Q3IDE_GET_FRAME = -200,   // New Q3IDE syscall
} cgameImport_t;
```

### Step 2: Create the Trap Wrapper (cgame side)

In `cg_syscalls.c`:

```c
void trap_Q3IDE_GetFrame(int windowId, byte *buffer, int *width, int *height) {
    syscall(CG_Q3IDE_GET_FRAME, windowId, buffer, width, height);
}
```

### Step 3: Handle in Engine Dispatcher (engine side)

In `cl_cgame.c`, inside `CL_CgameSystemCalls()`:

```c
case CG_Q3IDE_GET_FRAME:
    Q3IDE_GetFrame(args[1],
                   VMA(2),    // byte *buffer (VM address translation)
                   VMA(3),    // int *width
                   VMA(4));   // int *height
    return 0;
```

### Step 4: Add ASM Binding

In `cg_syscalls.asm`:

```asm
equ trap_Q3IDE_GetFrame -200
```

### Important Notes

- `VMA(n)` translates a VM memory address to a real pointer -- necessary because VM bytecode uses its own address space
- `VMF(n)` converts a VM argument to a float (arguments are always passed as ints in the VM ABI)
- The syscall number must be unique and negative
- Both the cgame QVM and the engine must be recompiled when adding syscalls

## Server-Side Game Trap Functions

The server game module (`game/`) has its own parallel set of trap functions defined in `g_syscalls.c` and dispatched in `sv_game.c`. Key server-side traps include:

- `trap_LocateGameData` -- Tell engine where game entity data lives
- `trap_SetBrushModel` -- Assign a BSP brush model to an entity
- `trap_Trace` -- Server-side trace
- `trap_LinkEntity` / `trap_UnlinkEntity` -- Link/unlink entities from collision world
- `trap_SetConfigstring` -- Set config strings (propagated to clients)

## Q3IDE Trap Function Strategy

Since Q3IDE modifies the engine directly (Quake3e fork), there are two approaches:

### Approach A: Direct Engine Calls (Recommended for MVP)

Skip the trap function layer entirely. Since Q3IDE code lives in the engine, it can call renderer and collision functions directly:

```c
// Direct call instead of going through trap
re.RegisterShader("q3ide/window");
CM_BoxTrace(&trace, start, end, mins, maxs, 0, MASK_SOLID);
```

### Approach B: New Trap Functions (For Mod Compatibility)

If Q3IDE features should be accessible from QVM mods, define new trap functions. This maintains the clean VM/engine boundary but adds complexity.

The recommended approach for MVP is Approach A, with Approach B considered for future extensibility.
