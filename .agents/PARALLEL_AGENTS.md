# Parallel Agents & Orchestrator Rules

## Lint/Build Fail Triage

**4–6 agents run simultaneously. Each owns specific files. Never fix another agent's files.**

When lint or build fails:
1. Read the error. Which file?
2. Your scope when multiple claude sessions are running in parallel? → fix it.
3. Another agent's scope? → **stop. report to orchestrator. do not touch it, don't build.**

File ownership:
- `quake3e/code/q3ide/`, `quake3e/Makefile` → **engine-adapter**
- `capture/` → **capture-rust**
- `spatial/` → **spatial-c**
- `daemon/` → **daemon-rust**

If another agent's WIP breaks the shared build, that's their fix. Report the failing file+error to orchestrator and work on independent tasks or wait. **Never edit outside your scope to unblock a build** — it corrupts their in-progress work.

## Agent Autonomy — NON-NEGOTIABLE

**The agent does everything. Never ask the user to run, build, test, or check anything.**

- Lint → build → run → read logs → fix → repeat. All of it. Autonomous loop.
- Use the Remote API (`host.docker.internal:6666`) to build, run, stop, and tail logs.
- Use `dbg.cmd(...)` / `dbg.watch(...)` to read live output. Never ask the user to paste logs.
- If the API server is unreachable, say so once and wait — do NOT ask the user to start it.
- Fix compiler errors yourself. Read the struct definitions. Don't ship code that doesn't compile.
- When something breaks, diagnose from logs before touching code. Don't guess.

## Agent Infrastructure Location

**Agent definition files always go in `.agents/agents/`** (dot-prefixed, at project root) — NOT in `.claude/` or `agents/` (no dot). Commands go in `.agents/commands/`. Project manifest at `.agents/PROJECT_LOOP.md`. Claude-only settings in `.claude/`. The `.agents/` convention is the cross-IDE standard (Claude Code, OpenCode, etc.).
