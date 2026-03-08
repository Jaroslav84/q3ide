#!/bin/sh
# lint.sh — Q3IDE code linter
#
# Checks:
#   q3ide/   — clang-format, cppcheck, file length, prefix, USE_Q3IDE guards
#   quake3e/ — modified files: USE_Q3IDE guard coverage only (no style checks)
#   capture/ — Rust: no unsafe outside lib.rs, limit .unwrap()
#
# Works in Docker (auto-installs tools via apt if available) and macOS.

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q3IDE_DIR="$ROOT/quake3e/code/q3ide"

ERRORS=0
WARNINGS=0

err()  { printf 'ERROR  %s\n' "$*"; ERRORS=$((ERRORS+1)); }
warn() { printf 'WARN   %s\n' "$*"; WARNINGS=$((WARNINGS+1)); }
ok()   { printf 'ok     %s\n' "$*"; }
info() { printf '       %s\n' "$*"; }

rel() { echo "${1#$ROOT/}"; }

# ─── tool bootstrap ───────────────────────────────────────────────────────────

ensure_tool() {
    local tool="$1"
    local pkg="${2:-$1}"
    command -v "$tool" >/dev/null 2>&1 && return 0

    # Try apt (Docker/Debian/Ubuntu with network)
    if command -v apt-get >/dev/null 2>&1; then
        apt-get install -y -q "$pkg" >/dev/null 2>&1 && return 0
    fi

    # Give platform-appropriate install hint
    if [ "$(uname)" = "Darwin" ]; then
        info "SKIP $tool — install: brew install $pkg"
    else
        info "SKIP $tool — install: apt-get install $pkg"
    fi
    return 1
}

# ─── clang-format (q3ide/ only) ───────────────────────────────────────────────

run_clang_format() {
    echo ""
    echo "-- clang-format (q3ide/ only) --"

    if ! ensure_tool clang-format clang-format; then
        info "skipped — clang-format not available"
        return
    fi

    local any=0
    for f in "$Q3IDE_DIR"/*.c "$Q3IDE_DIR"/*.h; do
        [ -f "$f" ] || continue
        any=1
        local r; r="$(rel "$f")"
        # --dry-run prints what would change, --Werror makes it exit non-zero
        local out
        out="$(clang-format --dry-run --Werror "$f" 2>&1)"
        if [ -n "$out" ]; then
            err "$r: clang-format violations (run: clang-format -i $r)"
            echo "$out" | head -5 | while IFS= read -r line; do info "  $line"; done
        else
            ok "$r"
        fi
    done
    [ "$any" -eq 0 ] && info "no C files found"
}

# ─── cppcheck (q3ide/ only) ───────────────────────────────────────────────────

run_cppcheck() {
    echo ""
    echo "-- cppcheck (q3ide/ only) --"

    if ! ensure_tool cppcheck cppcheck; then
        info "skipped — cppcheck not available"
        return
    fi

    # Include paths so cppcheck can resolve Quake3e headers
    local inc="-I$ROOT/quake3e/code/qcommon -I$ROOT/quake3e/code/client -I$ROOT/quake3e/code/renderercommon"

    local out
    out="$(cppcheck \
        --enable=warning,style,performance \
        --suppress=missingInclude \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        --error-exitcode=0 \
        --quiet \
        $inc \
        "$Q3IDE_DIR" 2>&1)"

    if [ -z "$out" ]; then
        ok "q3ide/ — no issues"
        return
    fi

    # Count errors vs warnings
    local nerr nwarn
    nerr="$(echo "$out" | grep -c "^.*: error:" || true)"
    nwarn="$(echo "$out" | grep -c "^.*: \(warning\|style\|performance\):" || true)"

    echo "$out" | while IFS= read -r line; do
        case "$line" in
            *": error:"*)   err "cppcheck: $line" ;;
            *)              warn "cppcheck: $line" ;;
        esac
    done

    ERRORS=$((ERRORS + nerr))
    WARNINGS=$((WARNINGS + nwarn))
}

# ─── basic checks: file length + prefix (q3ide/ only) ────────────────────────

run_basic_checks() {
    echo ""
    echo "-- basic checks (q3ide/ only) --"

    local found=0
    for f in "$Q3IDE_DIR"/*.c "$Q3IDE_DIR"/*.h; do
        [ -f "$f" ] || continue
        found=1
        local r; r="$(rel "$f")"
        local lines; lines="$(wc -l < "$f")"

        # File length
        if [ "$lines" -gt 400 ]; then
            err "$r: $lines lines (max 400 — split this file)"
        elif [ "$lines" -gt 200 ]; then
            warn "$r: $lines lines (sweet-spot 200)"
        fi

        # Public symbols must use q3ide_/Q3IDE_ prefix
        grep -n "^[a-zA-Z_][a-zA-Z0-9_ *]*  *[a-zA-Z_][a-zA-Z0-9_]* *(" "$f" 2>/dev/null | \
            grep -v "static " | \
            grep -v "^[0-9]*:#" | \
            grep -v "q3ide_\|Q3IDE_\|FBO_\|GLimp_\|RE_\|RB_\|R_" | \
            while IFS=: read -r n line; do
                warn "$r:$n: public symbol may lack q3ide_/Q3IDE_ prefix"
            done
    done
    [ "$found" -eq 0 ] && info "no files"
}

# ─── USE_Q3IDE guard check (modified Quake3e files only) ─────────────────────

run_guard_checks() {
    echo ""
    echo "-- USE_Q3IDE guards (modified Quake3e files) --"

    local MODIFIED="
        quake3e/code/client/cl_cgame.c
        quake3e/code/client/cl_main.c
        quake3e/code/sdl/sdl_glimp.c
        quake3e/code/renderer/tr_backend.c
        quake3e/code/renderer/tr_arb.c
        quake3e/code/renderer/tr_local.h
        quake3e/code/renderer/tr_init.c
        quake3e/code/renderer/tr_scene.c
        quake3e/code/renderer2/tr_backend.c
        quake3e/code/renderer2/tr_init.c
        quake3e/code/renderer2/tr_scene.c
        quake3e/code/renderercommon/tr_public.h
    "

    local found=0
    for rel_path in $MODIFIED; do
        local f="$ROOT/$rel_path"
        [ -f "$f" ] || continue
        found=1

        grep -q "Q3IDE_\|GLimp_CopyToSideWindow\|GLimp_SideWindow\|GLimp_SideYaw\|copyToSideCommand_t\|RC_COPY_TO_SIDE\|FBO_ResolveToBackBuffer" "$f" 2>/dev/null || continue

        local unguarded
        unguarded="$(awk '
            /^#if(def|ndef)?[[:space:]]/ || /^#if[[:space:]]/ {
                top++
                is_q3ide[top] = ($0 ~ /USE_Q3IDE/) ? 1 : 0
                if (is_q3ide[top]) q3ide++
                next
            }
            /^#endif/ {
                if (top > 0) {
                    if (is_q3ide[top]) q3ide--
                    delete is_q3ide[top]
                    top--
                }
                next
            }
            /^(void|int|float|static)[[:space:]].*GLimp_/ { next }
            /^[[:space:]]*(\/\/|\*)/ { next }
            /Q3IDE_|GLimp_CopyToSideWindow|GLimp_SideWindow|GLimp_SideYaw|copyToSideCommand_t|RC_COPY_TO_SIDE|FBO_ResolveToBackBuffer/ {
                if (q3ide == 0) { count++; print NR": "$0 > "/tmp/q3ide_lint_guards" }
            }
            END { print count+0 }
        ' "$f")"

        if [ "${unguarded:-0}" -gt 0 ]; then
            err "$rel_path: ${unguarded} Q3IDE reference(s) outside #ifdef USE_Q3IDE"
            while IFS= read -r line; do info "  $line"; done < /tmp/q3ide_lint_guards
        fi
    done
    [ "$found" -eq 0 ] && info "no files"
}

# ─── Rust basic checks (capture/) ────────────────────────────────────────────

run_rust_checks() {
    echo ""
    echo "-- capture/ Rust (basic) --"

    local found=0
    for f in "$ROOT"/capture/src/*.rs; do
        [ -f "$f" ] || continue
        found=1
        local r; r="$(rel "$f")"
        local base; base="$(basename "$f")"

        # unsafe outside lib.rs
        if [ "$base" != "lib.rs" ]; then
            local nu
            nu="$(grep -c "\bunsafe\b" "$f" 2>/dev/null || true)"
            [ "${nu:-0}" -gt 0 ] && warn "$r: ${nu} unsafe block(s) outside C-ABI boundary (lib.rs)"
        fi

        # excess .unwrap()
        local u
        u="$(grep -c "\.unwrap()" "$f" 2>/dev/null || true)"
        [ "${u:-0}" -gt 3 ] && warn "$r: ${u} .unwrap() calls — prefer ? or .expect()"
    done
    [ "$found" -eq 0 ] && info "no files"
}

# ─── main ─────────────────────────────────────────────────────────────────────

echo "=== Q3IDE Linter ==="
echo "root: $ROOT"

run_clang_format
run_cppcheck
run_basic_checks
run_guard_checks
run_rust_checks

echo ""
echo "=== ${ERRORS} error(s), ${WARNINGS} warning(s) ==="
[ "$ERRORS" -eq 0 ]
