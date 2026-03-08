# Q3IDE — Orchestration Guide

How to run Claude Code like a pro on this project. Custom agents, subagents, and autonomous workflows.

---

## Prerequisites

### Docker Container Setup

Your Claude Code runs in Docker with the project mounted at `/root/Projects/q3ide`. Add this to `~/.claude/settings.json` inside the container:

```json
{
  "defaultMode": "bypassPermissions",
  "skipDangerousModePermissionPrompt": true
}
```

No more permission prompts. Docker is your sandbox. Git is your rollback.

### CLAUDE.md

Your project root should have a `AGENTS.md` that points to `./plan/04-Q3IDE_SPECIFICATION.md`:

```markdown
# Q3IDE

Read ./plan/04-Q3IDE_SPECIFICATION.md for the full feature spec, architecture, and implementation rules.

## Rules
- 200 lines per file sweetspot. 400 max. Split if exceeded.
- Never modify core Quake3e engine files unless absolutely necessary. Create files around them instead, adapters.
- All Q3IDE code goes in spatial/, capture/, or daemon/.
- spatial/ is engine-agnostic. It talks to the engine through engine/adapter.h only.
- Commit after completing each feature with a descriptive message.
- Run the test checkpoint before moving to the next feature.
```

---

## Step 1 — Custom Agents

### What the LLM must do (first task of any session)

Before writing any feature code, the LLM must create the `.agents/agents/q3agents/` directory and populate it with four specialized agents matching the architecture boundaries. **This is the first task. Do this before touching any code.**


The agent files are fully implemented at `.agents/agents/q3agents/`. Do not recreate them — read the actual files instead.

### Agent files (v2 — read these, don't recreate)

| Agent | File | Role |
|-------|------|------|
| orchestrator | `.agents/agents/q3agents/orchestrator.md` | Routes tasks, manages chain, signs off batches |
| engine-adapter | `.agents/agents/q3agents/engine-adapter.md` | Quake3e hooks, renderer, textures, Q3 source expert |
| capture-rust | `.agents/agents/q3agents/capture-rust.md` | SCK capture, C-ABI, ring buffer, frame mixing fix |
| spatial-c | `.agents/agents/q3agents/spatial-c.md` | Window placement, 7 presentation styles, VisionOS design |
| daemon-rust | `.agents/agents/q3agents/daemon-rust.md` | Background services, UML cache, FSEvents, FFI patterns |
| reviewer | `.agents/agents/q3agents/reviewer.md` | Code review, batch sign-off, blocks on violations |

All agents embed their key domain knowledge inline (hybrid doc strategy) and reference `.agents/LOOP.md` for the autonomous feedback loop protocol.

### Feedback loop (`.agents/LOOP.md`)

The canonical agent loop — all specialists follow this:

```
ROUTE → READ SPEC → IMPLEMENT → LINT → REFACTOR? → BUILD → SPAWN WINDOWS (automated)
→ RUN + WS DEBUGGER → KILL → CHECK LOGS → REVIEW → REPEAT or SIGN-OFF
```

Key properties:
- **No human intervention** — agents spawn iTerm2 test windows via AppleScript, attach via RCON, watch via WebSocket
- **Who runs the game** — the top-most integration layer agent runs; upstream agents stop after build
- **Reviewer never runs the game** — review only, triggered automatically at end of every cycle
- **Max 3 fix iterations** before escalating to user

---

### 📋 Manual steps for YOU (Istvan)

1. **Verify agents exist** after the LLM creates them: `ls .agents/agents/q3agents/`
2. **Customize agents** if you want — add project-specific notes, known gotchas, API patterns
3. **Add new agents later** as the project grows (e.g., `quakeos-renderer.md` for Batch 15)
4. **Commit the agents to git** — they're part of the project, not personal config

---

## Step 2 — Subagents

Subagents are worker agents that Claude Code spawns mid-session for focused tasks. They run in the background, report back, and die.

### Model Configuration

Set these environment variables in your Docker container (add to Dockerfile or docker-compose):

```bash
# Main session: Sonnet (smart enough, fast, cheaper)
export ANTHROPIC_MODEL="claude-sonnet-4-6"

# Subagents: Haiku for light work (file reading, grep, simple edits)
export CLAUDE_CODE_SUBAGENT_MODEL="claude-haiku-4-5-20251001"
```

**Thinking models:** Don't use extended thinking for routine work. It burns tokens for minimal gain on well-scoped tasks. The custom agents in `.agents/agents/q3agents/` provide the context that thinking would otherwise figure out.

**When to use Sonnet directly:** For complex architectural decisions, multi-file refactors, or anything where the agent needs to reason about the whole system. When you see Claude Code struggling with Haiku on a subtask, tell it: "Use Sonnet for this one."

### How subagents work in practice

During a session, Claude Code automatically spawns subagents when it needs to:
- Research a file or pattern across the codebase
- Run a focused task that doesn't need the full conversation context
- Perform parallel work (e.g., "check all imports in spatial/" while continuing the main task)

**Background a subagent:** Press `Ctrl+B` while a subagent is running to send it to the background. You keep working. It keeps working. Check status with `/tasks`.

### Telling Claude Code how to delegate

Add this to your `CLAUDE.md`:

```markdown
## Delegation Rules

When implementing a batch:
1. Use the custom agents in .agents/agents/q3agents/ for work in their scope.
2. Spawn subagents for independent parallel tasks (e.g., "write tests" while "implementing feature").
3. Subagents should be scoped to a single file or small group of related files.
4. Never let a subagent modify files outside its agent's scope.
5. After subagent completes, review its output before integrating.
```

### Example workflow

You tell Claude Code:

```
Implement Batch 5 (Grapple Hook & Spatial Tools) from ./plan/04-Q3IDE_SPECIFICATION.md.
```

Claude Code (main session) reads ./plan/04-Q3IDE_SPECIFICATION.md, plans the batch, then:

1. Delegates `spatial/nav/grapple.h` + `grapple.c` to the **spatial-c** agent
2. Spawns a subagent to implement `spatial/ui/minimap.h` + `minimap.c` in parallel
3. Spawns another subagent for `spatial/project/browser.h` + `browser.c`
4. Waits for all to complete, reviews, integrates
5. Runs the test checkpoint
6. Commits: `git commit -m "Batch 5: Grapple Hook, Minimap, File Browser, Quick Open"`

### Cost expectations

| Model | Use for | Cost |
|-------|---------|------|
| Haiku | Grep, read, simple edits, test runs, file scaffolding | ~$0.25/M input, $1.25/M output |
| Sonnet | Feature implementation, refactoring, architectural decisions | ~$3/M input, $15/M output |

Haiku subagents for grunt work = 10-12x cheaper than running everything on Sonnet.

---

### 📋 Manual steps for YOU (Istvan)

1. **Set environment variables** in your Docker setup:
   ```bash
   # In Dockerfile or docker-compose.yml or .env
   ANTHROPIC_MODEL=claude-sonnet-4-6
   CLAUDE_CODE_SUBAGENT_MODEL=claude-haiku-4-5-20251001
   ```

2. **Monitor token usage** — check your Anthropic dashboard after each batch. If Haiku subagents are failing on tasks, bump specific tasks to Sonnet.

3. **`Ctrl+B` is your friend** — whenever Claude Code spawns a subagent that's going to take a while, background it and keep talking to the main session.

4. **`/tasks`** — check on background workers anytime.

5. **Kill runaway agents** — if a subagent is spinning, use `/tasks` to find it and cancel.

---

## Workflow: Running a Batch

Here's the full orchestrated workflow for implementing a batch:

```
1. Start Claude Code in Docker
   └── bypassPermissions mode (no interruptions)

2. Tell it: "Implement Batch N from ./plan/04-Q3IDE_SPECIFICATION.md"

3. Claude Code reads ./plan/04-Q3IDE_SPECIFICATION.md
   ├── Identifies all features in the batch
   ├── Plans implementation order (dependencies first)
   └── Identifies which custom agent handles which files

4. Implementation (parallel where possible)
   ├── spatial-c agent → Window/UI/navigation code
   ├── capture-rust agent → capture pipeline changes
   ├── engine-adapter agent → engine hooks (if needed)
   └── Haiku subagents → file scaffolding, test stubs, grep tasks

5. Integration
   ├── Main session reviews all agent output
   ├── reviewer agent checks boundary violations
   └── Fixes any issues

6. Test checkpoint
   ├── Verify against ./plan/04-Q3IDE_SPECIFICATION.md test checkpoint
   └── Run performance check (if this is a perf checkpoint batch)

7. Commit
   └── git commit -m "Batch N: [feature summary]"

8. You review
   └── git log, git diff, test on your Mac
```

---

## Tips

- **One batch per session.** Don't try to do multiple batches in one Claude Code session. Context fills up. Start fresh for each batch.
- **Commit before starting.** Always have a clean git state before telling Claude Code to start a batch. Easy rollback with `git reset --hard HEAD` if things go sideways.
- **./plan/04-Q3IDE_SPECIFICATION.md is the source of truth.** If Claude Code asks a question that's answered in the vision doc, tell it to re-read the doc.
- **Logs matter.** Your Docker container is sandboxed — logs in `.q3ide/` are your only debugging window. Make sure Claude Code writes to them.
- **Watch for scope creep.** If Claude Code starts "improving" things outside the current batch, stop it. Batch discipline is everything.

---

## Future: Levels 3-4 (Not Now)

When the project grows and you need more parallelism:

- **Level 3 — Agent Teams:** Multiple Claude Code instances with a team lead coordinating. Experimental feature. Enable with `CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1`.
- **Level 4 — Git worktree parallel workers:** Separate Docker containers per branch, each running autonomous Claude Code. Merge when done.
- **Level 5 — Orchestrators (Gas Town / Multiclaude):** Full multi-agent orchestration frameworks. Kubernetes for AI agents.

These make sense when you're running 3+ agents on different batches simultaneously. Not needed yet.