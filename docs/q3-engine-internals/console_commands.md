# Quake 3 Console Command Registration

> **Sources:**
> - [Quake III Arena Source Code - cmd.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/qcommon/cmd.c)
> - [Quake III Arena Source Code - cg_consolecmds.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/cgame/cg_consolecmds.c)
> - [Quake Command Processor Documentation](https://www.icculus.org/~phaethon/q3/cmdproc/qcmdproc.html)
> - [Quake Wiki - Console Commands (Q3)](https://quake.fandom.com/wiki/Console_Commands_(Q3))

## Overview

Quake 3's console command system allows the engine, game modules (game, cgame, ui), and mods to register named commands that can be typed in the console or bound to keys. The system is based on a linked list of command structures, each binding a command name string to a C function pointer.

## Engine-Level Command Registration (cmd.c)

### cmd_function_t Structure

Commands are stored as a linked list of `cmd_function_t` structures:

```c
typedef struct cmd_function_s {
    struct cmd_function_s *next;
    char                  *name;
    xcommand_t            function;     // function pointer: void (*)(void)
} cmd_function_t;
```

The list head is `cmd_functions`, a global pointer to the first command in the chain.

### Cmd_AddCommand

The primary function for registering engine-level commands:

```c
void Cmd_AddCommand(const char *cmd_name, xcommand_t function);
```

This function:

1. Checks if the command name already exists (warns if duplicate)
2. Allocates a new `cmd_function_t` from the zone allocator
3. Sets the `name` field to the command name string
4. Sets the `function` field to the provided function pointer
5. Links the new command at the head of the `cmd_functions` linked list:
   ```c
   cmd->next = cmd_functions;
   cmd_functions = cmd;
   ```

### Cmd_RemoveCommand

Removes a command from the linked list:

```c
void Cmd_RemoveCommand(const char *cmd_name);
```

Walks the linked list, finds the matching command, and unlinks it.

### Cmd_ExecuteString

The dispatcher that processes a command string:

```c
void Cmd_ExecuteString(const char *text);
```

The execution order is:

1. **Tokenize** the input string into `cmd_argc` / `cmd_argv`
2. **Search registered commands**: Walk the `cmd_functions` linked list for a name match
3. **Search aliases**: If no command found, check the alias list
4. **Search cvars**: If no alias found, try to set/get a cvar
5. If nothing matches, print "Unknown command" error

### Cmd_Init

During engine startup, `Cmd_Init` registers the built-in commands:

```c
void Cmd_Init(void) {
    Cmd_AddCommand("cmdlist", Cmd_List_f);
    Cmd_AddCommand("exec", Cmd_Exec_f);
    Cmd_AddCommand("vstr", Cmd_Vstr_f);
    Cmd_AddCommand("echo", Cmd_Echo_f);
    Cmd_AddCommand("wait", Cmd_Wait_f);
    // ... more built-in commands
}
```

### Command Arguments

Inside a command handler, arguments are accessed via:

```c
int    Cmd_Argc(void);           // Number of arguments (including command name)
char  *Cmd_Argv(int arg);        // Get argument by index
char  *Cmd_Args(void);           // Get all arguments as a single string
```

## CGGame-Level Command Registration

The cgame module registers its own commands through the VM trap system, since it runs inside the virtual machine and cannot directly call `Cmd_AddCommand`.

### trap_AddCommand

The cgame-side wrapper for command registration:

```c
void trap_AddCommand(const char *cmdName);
```

This calls through the VM syscall interface to `CG_ADDCOMMAND` in the engine, which registers the command name. When the command is executed, the engine sends a `CG_CONSOLE_COMMAND` message back to the cgame VM.

### CG_InitConsoleCommands

The cgame module registers all its commands during initialization:

```c
// From cg_consolecmds.c
typedef struct {
    char    *cmd;
    void    (*function)(void);
} consoleCommand_t;

static consoleCommand_t commands[] = {
    { "testgun",        CG_TestGun_f },
    { "testmodel",      CG_TestModel_f },
    { "nextframe",      CG_TestModelNextFrame_f },
    { "prevframe",      CG_TestModelPrevFrame_f },
    { "nextskin",       CG_TestModelNextSkin_f },
    { "prevskin",       CG_TestModelPrevSkin_f },
    { "viewpos",        CG_Viewpos_f },
    { "+scores",        CG_ScoresDown_f },
    { "-scores",        CG_ScoresUp_f },
    { "+zoom",          CG_ZoomDown_f },
    { "-zoom",          CG_ZoomUp_f },
    { "sizeup",         CG_SizeUp_f },
    { "sizedown",       CG_SizeDown_f },
    { "weapnext",       CG_NextWeapon_f },
    { "weapprev",       CG_PrevWeapon_f },
    { "weapon",         CG_Weapon_f },
    { "tell_target",    CG_TellTarget_f },
    { "tell_attacker",  CG_TellAttacker_f },
    { "tcmd",           CG_TargetCommand_f },
};

void CG_InitConsoleCommands(void) {
    int i;
    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        trap_AddCommand(commands[i].cmd);
    }
    // Also register server commands for tab completion
    trap_AddCommand("kill");
    trap_AddCommand("say");
    trap_AddCommand("give");
    // ... more server commands
}
```

### CG_ConsoleCommand (Dispatch)

When a registered cgame command is executed, the engine sends a `CG_CONSOLE_COMMAND` message to the VM. The dispatch function:

```c
qboolean CG_ConsoleCommand(void) {
    const char *cmd;
    int i;

    cmd = CG_Argv(0);

    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (!Q_stricmp(cmd, commands[i].cmd)) {
            commands[i].function();
            return qtrue;
        }
    }

    return qfalse;
}
```

## Server-Side Command Registration (game module)

The server game module (`game/`) has its own parallel system:

```c
// g_cmds.c - Server commands sent by clients
// g_svcmds.c - Server console commands (admin commands)
```

Server commands use `trap_SendConsoleCommand` to execute commands on the server.

## Q3IDE Command Registration Strategy

For Q3IDE's console commands (`/q3ide_list`, `/q3ide_attach`, etc.), there are two approaches:

### Approach 1: Engine-Level (Recommended for MVP)

Register commands directly in the engine using `Cmd_AddCommand`:

```c
// In q3ide initialization
Cmd_AddCommand("q3ide_list",   Q3IDE_Cmd_List);
Cmd_AddCommand("q3ide_attach", Q3IDE_Cmd_Attach);
Cmd_AddCommand("q3ide_detach", Q3IDE_Cmd_Detach);
Cmd_AddCommand("q3ide_status", Q3IDE_Cmd_Status);
```

This approach works because Q3IDE modifies the engine directly (Quake3e fork), so it has access to engine internals.

### Approach 2: CGGame-Level

If Q3IDE were implemented as a mod rather than an engine modification, commands would go through the VM trap system. This is less direct but more portable.

## Cvar System (Related)

Cvars (console variables) are the complementary system to commands:

```c
cvar_t *Cvar_Get(const char *var_name, const char *var_value, int flags);
void    Cvar_Set(const char *var_name, const char *value);
```

Q3IDE will likely need cvars for configuration (e.g., `q3ide_fps`, `q3ide_debug`).
