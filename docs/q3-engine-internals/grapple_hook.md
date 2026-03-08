# Quake 3 Grapple Hook Implementation Guide

> **Sources:**
> - [Quake III Arena Source Code - bg_public.h](https://github.com/id-Software/Quake-III-Arena/blob/master/code/game/bg_public.h) (grapple entity/damage types)
> - [Quake III Arena Source Code - q_shared.h](https://github.com/id-Software/Quake-III-Arena/blob/master/code/qcommon/q_shared.h) (playerState_t, movement flags)
> - [Quake3e - Team Arena grapple hook AI](https://github.com/ec-/Quake3e/blob/master/code/botlib/) (botlib grapple pathfinding)
> - [Quake3World - Grapple Hook Implementation](https://www.quake3world.com/)

## Overview

Quake 3 Team Arena included a grapple hook weapon, and **Quake3e retains the infrastructure for it** in its movement prediction system. The grapple is not a traditional projectile with physics simulation; instead, it uses a **pull-based approach** where a flag is set on the player state and a target point is specified. The pmove function then accelerates the player toward that point.

## Existing Grapple Infrastructure in Quake3e

### Player Movement Flags

**Location**: `code/game/bg_public.h:155`

```c
#define PMF_GRAPPLE_PULL  2048  // pull towards grapple location
```

This flag, when set in `playerState_t->pm_flags`, activates per-frame pulling logic in the movement prediction.

### Player State Grapple Point

**Location**: `code/qcommon/q_shared.h:1210` (in `playerState_t`)

```c
vec3_t grapplePoint;  // location of grapple to pull towards if PMF_GRAPPLE_PULL
```

This stores the world-space 3D position that the grapple hook is attached to. The pmove system uses this to calculate the pull velocity.

### Entity Type

**Location**: `code/game/bg_public.h:689`

```c
ET_GRAPPLE,  // grapple hooked on wall
```

This entity type is used to render the visual hook point (the cable endpoint).

### Means of Death

**Location**: `code/game/bg_public.h:606`

```c
MOD_GRAPPLE  // Grapple weapon/movement enum value
```

Used for logging deaths caused by grapple-related damage.

### Bot Grapple Support

**Location**: `code/server/sv_bot.c`

```c
Cvar_Get("bot_grapple", "0", 0);  // enable grapple for AI bots
```

Botlib includes pathfinding for grapple reachability (see `code/botlib/be_aas_reach.c`).

## Movement Prediction (Pmove) System

The grapple pull is implemented in the shared prediction code (used by both server and client). The key insight: **grapple is not a missile, it's a movement modifier**.

### How Pmove Handles Grapple Pull

When `ps->pm_flags & PMF_GRAPPLE_PULL`:

1. **Calculate direction to target**:
   ```c
   VectorSubtract(ps->grapplePoint, ps->origin, direction);
   VectorNormalize(direction);
   ```

2. **Apply acceleration toward target**:
   ```c
   float pullSpeed = 800;  // units/sec (tunable)
   VectorScale(direction, pullSpeed, accelVec);
   ps->velocity = accelVec;  // or blend with existing velocity
   ```

3. **Per-frame update** in `Pmove()`: the pull is applied each client frame (~16ms at 60 FPS)

4. **When to stop**: The game/cgame must detect when player reaches the hook point (distance < threshold) and clear `PMF_GRAPPLE_PULL`

## Firing the Grapple Hook

Since grapple is not a projectile in the traditional sense, **there is no "missile" to fire**. Instead:

### Approach: Player-Direct (Instant Cast)

When the player fires a grapple command:

1. **Raytrace from player viewpoint**:
   ```c
   trace_t trace;
   trap_CM_BoxTrace(&trace, playerOrigin, endPoint,
                    vec3_origin, vec3_origin,
                    0, MASK_SOLID);
   ```

2. **If hit solid surface within range (e.g., 1000 units)**:
   ```c
   ps->pm_flags |= PMF_GRAPPLE_PULL;
   VectorCopy(trace.endpos, ps->grapplePoint);
   ```

3. **If miss or out of range**: Don't set the flag; grapple fails silently

### Alternative: Visible Projectile (Optional Visual)

To show a cable from player to hook point, spawn an `ET_GRAPPLE` entity:

```c
// Server side (game module)
gentity_t *hook = G_Spawn();
hook->s.eType = ET_GRAPPLE;
VectorCopy(hookPoint, hook->s.origin);
hook->owner = playerEntity;  // Link to owner
```

This entity is purely visual; it doesn't affect gameplay. The server sends it via snapshot, and cgame renders it.

## Reeling In: Per-Frame Velocity Adjustment

The pull is applied automatically by `Pmove()` when the flag is set. However, you may want to tune the behavior:

### Soft Pull (Gravity-Aware)

```c
// Blend player's current velocity with pull direction
float blendFactor = 0.5;  // 50% pull, 50% current motion
vec3_t pullVel;
VectorScale(direction, pullSpeed, pullVel);
ps->velocity[0] = ps->velocity[0] * (1 - blendFactor) + pullVel[0] * blendFactor;
ps->velocity[1] = ps->velocity[1] * (1 - blendFactor) + pullVel[1] * blendFactor;
ps->velocity[2] = ps->velocity[2] * (1 - blendFactor) + pullVel[2] * blendFactor;
```

### Hard Pull (Direct Velocity Override)

```c
// Ignore current velocity, pull directly toward target
VectorScale(direction, pullSpeed, ps->velocity);
```

### Stop Condition

Check per-frame (in cgame's `CG_PredictPlayerState` or game's `PlayerThink`):

```c
if (Distance(ps->origin, ps->grapplePoint) < 20) {
    ps->pm_flags &= ~PMF_GRAPPLE_PULL;  // Clear flag
    // Optional: trigger a landing event
}
```

## Q3IDE: Navigate to Bookmark

For Q3IDE's VisionOS-style grapple, **use instant teleport** (more appropriate for windowed navigation):

### Teleport Alternative to Pull

```c
// Instant movement to bookmark location
G_SetOrigin(player->ent, bookmarkPos);
// Or: trap_SetOrigin(player->ps->origin, bookmarkPos) in cgame
```

For a smoother effect, **lerp over N frames**:

```c
// Store target and lerp start time in a separate struct
q3ide_navState.targetPos = bookmarkPos;
q3ide_navState.lerpStart = cg.time;
q3ide_navState.lerpDuration = 500;  // 500ms transition
q3ide_navState.lerpActive = qtrue;

// Each frame, update player origin
if (q3ide_navState.lerpActive) {
    float frac = (cg.time - q3ide_navState.lerpStart) / q3ide_navState.lerpDuration;
    frac = frac > 1.0 ? 1.0 : frac;

    vec3_t lerpPos;
    VectorLerp(q3ide_navState.startPos, q3ide_navState.targetPos, frac, lerpPos);
    VectorCopy(lerpPos, ps->origin);

    if (frac >= 1.0) {
        q3ide_navState.lerpActive = qfalse;
    }
}
```

This avoids the jarring teleport while keeping the code simple (no missile/projectile).

## Beam Rendering: Cable Visual

To draw the grapple cable from player to hook point as a visual indicator:

### Method 1: Poly Strip (Via trap_R_AddPolyToScene)

In cgame (called during render setup):

```c
void CG_DrawGrappleCable(void) {
    if (!(cg_entities[cg.snap->ps.clientNum].currentState.pm_flags & PMF_GRAPPLE_PULL)) {
        return;
    }

    vec3_t playerPos, hookPos;
    VectorCopy(cg_entities[cg.snap->ps.clientNum].lerpOrigin, playerPos);
    VectorCopy(cg_entities[cg.snap->ps.clientNum].currentState.grapplePoint, hookPos);

    // Create a quad cable with thickness
    polyVert_t verts[4];
    vec3_t right, up;
    float cableWidth = 2.0;

    // Calculate perpendicular directions for cable thickness
    VectorSubtract(hookPos, playerPos, up);  // Cable direction
    // Cross with camera right to get perpendicular
    // (This is simplified; proper calculation needed)

    verts[0].xyz = {playerPos[0] - cableWidth, playerPos[1], playerPos[2]};
    verts[1].xyz = {playerPos[0] + cableWidth, playerPos[1], playerPos[2]};
    verts[2].xyz = {hookPos[0] + cableWidth, hookPos[1], hookPos[2]};
    verts[3].xyz = {hookPos[0] - cableWidth, hookPos[1], hookPos[2]};

    // Set texture coords (0,0) to (1,1)
    verts[0].st[0] = 0; verts[0].st[1] = 0;
    verts[1].st[0] = 1; verts[1].st[1] = 0;
    verts[2].st[0] = 1; verts[2].st[1] = 1;
    verts[3].st[0] = 0; verts[3].st[1] = 1;

    // Set color (white, semi-transparent)
    for (int i = 0; i < 4; i++) {
        verts[i].modulate[0] = 255;
        verts[i].modulate[1] = 255;
        verts[i].modulate[2] = 255;
        verts[i].modulate[3] = 200;
    }

    // Register cable shader (simple white quad with opacity)
    qhandle_t cableShader = trap_R_RegisterShader("q3ide/grapple_cable");
    trap_R_AddPolyToScene(cableShader, 4, verts);
}
```

### Method 2: Line (Via Debug Drawing)

Simpler, but requires debug mode:

```c
trap_R_DebugLine(playerPos, hookPos, colorWhite);
```

Not available in release builds; use method 1 for shipping.

## Integration Hooks for Q3IDE

### 1. Fire Hook Command

Add a console command to Q3IDE:

```c
void Q3IDE_NavigateToBookmark(vec3_t bookmarkPos) {
    playerState_t *ps = &cg_entities[cg.snap->ps.clientNum].currentState;

    // Option A: Instant teleport
    VectorCopy(bookmarkPos, ps->origin);

    // Option B: Enable grapple pull
    ps->pm_flags |= PMF_GRAPPLE_PULL;
    VectorCopy(bookmarkPos, ps->grapplePoint);
}
```

Register in `Q3IDE_Init()`:

```c
trap_AddCommand("q3ide navigate");
```

### 2. Bookmark Storage

Store bookmarks as `vec3_t` array:

```c
#define MAX_Q3IDE_BOOKMARKS 32

typedef struct {
    vec3_t pos;
    char name[32];
} q3ide_bookmark_t;

q3ide_bookmark_t q3ide_bookmarks[MAX_Q3IDE_BOOKMARKS];
int q3ide_numBookmarks = 0;
```

### 3. Hook Window Position as Bookmark

When a window is clicked in-game:

```c
// User clicks a window quad
vec3_t windowWorldPos = /* from wall trace */
Q3IDE_SaveBookmark(windowWorldPos, "Window_iTerm");
```

## Summary: Implementation Steps

1. **Detect grapple fire** (raycast from player viewpoint)
2. **Set flags**: `ps->pm_flags |= PMF_GRAPPLE_PULL`; copy target to `ps->grapplePoint`
3. **Pmove handles rest**: Movement prediction automatically pulls player toward target each frame
4. **Stop condition**: When distance to grapplePoint < threshold, clear the flag
5. **Render cable** (optional): Use `trap_R_AddPolyToScene` with a cable shader
6. **For Q3IDE**: Leverage instant teleport or lerp, not traditional pull mechanics
