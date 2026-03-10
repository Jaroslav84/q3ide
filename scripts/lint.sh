#!/bin/sh
# lint.sh — Q3IDE code linter
#
# Checks:
#   q3ide/   — clang-format, cppcheck, file length, prefix, USE_Q3IDE guards
#   quake3e/ — modified files: USE_Q3IDE guard coverage only
#   capture/ — Rust: no unsafe outside lib.rs, limit .unwrap()

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q3IDE_DIR="$ROOT/quake3e/code/q3ide"

RUN_CPPCHECK=0
for arg in "$@"; do
    [ "$arg" = "--cppcheck" ] && RUN_CPPCHECK=1
done

ERRORS=0
WARNINGS=0
ERROR_FILES=""

err()  { printf '[ERR]  %s\n' "$*"; ERRORS=$((ERRORS+1)); }
warn() { printf '[WRN]  %s\n' "$*"; WARNINGS=$((WARNINGS+1)); }
ok()   { printf '[ OK ] %s\n' "$*"; }
info() { printf '[   ]  %s\n' "$*"; }

rel()  { echo "${1#$ROOT/}"; }
trim() { echo "$1" | tr -d ' \t'; }

# ─── clang-format (q3ide/ only) ───────────────────────────────────────────────

run_clang_format() {
    echo ""
    echo "-- clang-format (q3ide/ only) --"

    if ! command -v clang-format >/dev/null 2>&1; then
        info "skipped — clang-format not available (brew install clang-format)"
        return
    fi

    for f in "$Q3IDE_DIR"/*.c "$Q3IDE_DIR"/*.h; do
        [ -f "$f" ] || continue
        local r; r="$(rel "$f")"
        local lines; lines=$(trim "$(wc -l < "$f")")
        local violations; violations="$(clang-format --dry-run "$f" 2>/dev/null)"
        if [ -n "$violations" ]; then
            warn "$r [${lines}L]: clang-format violations (fix: clang-format -i $r)"
            echo "$violations" | head -6 | while IFS= read -r line; do info "  $line"; done
        else
            ok "$r [${lines}L]"
        fi
    done
}

# ─── cppcheck (q3ide/ only) ───────────────────────────────────────────────────

run_cppcheck() {
    echo ""
    echo "-- cppcheck (q3ide/ only) --"

    if ! command -v cppcheck >/dev/null 2>&1; then
        info "skipped — cppcheck not available (brew install cppcheck)"
        return
    fi

    local any_issues=0
    for f in "$Q3IDE_DIR"/*.c; do
        [ -f "$f" ] || continue
        local r; r="$(rel "$f")"
        printf '[...] %s\r' "$r"

        local out
        out="$(cppcheck \
            --enable=warning \
            --suppress=missingInclude \
            --suppress=missingIncludeSystem \
            --suppress=unusedFunction \
            --error-exitcode=0 \
            --quiet \
            "$f" 2>&1)"

        if [ -n "$out" ]; then
            printf '%-60s\n' " "
            echo "$out" | while IFS= read -r line; do info "  $line"; done
            warn "$r: cppcheck issues"
            any_issues=1
        fi
    done
    printf '%-60s\r' " "
    [ "$any_issues" -eq 0 ] && ok "q3ide/ — no issues"
}

# ─── basic checks: file length + prefix (q3ide/ only) ────────────────────────

run_basic_checks() {
    echo ""
    echo "-- basic checks (q3ide/ only) --"

    local found=0 any_issues=0
    for f in "$Q3IDE_DIR"/*.c "$Q3IDE_DIR"/*.h; do
        [ -f "$f" ] || continue
        found=1
        local r; r="$(rel "$f")"
        local lines; lines=$(trim "$(wc -l < "$f")")

        if [ "$lines" -gt 400 ]; then
            err "$r: $lines lines (max 400 — split this file)"
            ERROR_FILES="${ERROR_FILES}  too-long: $r (${lines}L)\n"
            any_issues=1
        elif [ "$lines" -gt 200 ]; then
            warn "$r: $lines lines (sweet-spot 200)"
            any_issues=1
        fi

        grep -n "^[a-zA-Z_][a-zA-Z0-9_ *]*  *[a-zA-Z_][a-zA-Z0-9_]* *(" "$f" 2>/dev/null | \
            grep -v "static " | \
            grep -v "^[0-9]*:#" | \
            grep -v "q3ide_\|Q3IDE_\|FBO_\|GLimp_\|RE_\|RB_\|R_" | \
            while IFS=: read -r n line; do
                warn "$r:$n: public symbol may lack q3ide_/Q3IDE_ prefix"
            done
    done
    [ "$found" -eq 0 ] && info "no files"
    [ "$any_issues" -eq 0 ] && [ "$found" -gt 0 ] && ok "all files within size limits"
}

# ─── USE_Q3IDE guard check (modified Quake3e files only) ─────────────────────

run_guard_checks() {
    echo ""
    echo "-- USE_Q3IDE guards (modified Quake3e files) --"

    local MODIFIED="
        quake3e/code/client/cl_cgame.c
        quake3e/code/client/cl_main.c
        quake3e/code/client/cl_console.c
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

    local found=0 any_issues=0
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
            ERROR_FILES="${ERROR_FILES}  guard: $rel_path\n"
            any_issues=1
        fi
    done
    [ "$found" -eq 0 ] && info "no files"
    [ "$any_issues" -eq 0 ] && [ "$found" -gt 0 ] && ok "all guards in place"
}

# ─── Rust basic checks (capture/) ────────────────────────────────────────────

run_rust_checks() {
    echo ""
    echo "-- capture/ Rust (basic) --"

    local found=0 any_issues=0
    for f in "$ROOT"/capture/src/*.rs; do
        [ -f "$f" ] || continue
        found=1
        local r; r="$(rel "$f")"
        local base; base="$(basename "$f")"

        if [ "$base" != "lib.rs" ]; then
            local nu
            nu="$(grep -c "\bunsafe\b" "$f" 2>/dev/null || true)"
            if [ "${nu:-0}" -gt 0 ]; then
                err "$r: ${nu} unsafe block(s) outside C-ABI boundary (lib.rs)"
                ERROR_FILES="${ERROR_FILES}  unsafe: $r\n"
                any_issues=1
            fi
        fi

        local u
        u="$(grep -c "\.unwrap()" "$f" 2>/dev/null || true)"
        [ "${u:-0}" -gt 3 ] && warn "$r: ${u} .unwrap() calls — prefer ? or .expect()"
    done
    [ "$found" -eq 0 ] && info "no files"
    [ "$any_issues" -eq 0 ] && [ "$found" -gt 0 ] && ok "no unsafe outside lib.rs"

    # cargo check — catches dead_code, type errors, and all compiler warnings.
    # Only runs on macOS (crate requires ScreenCaptureKit framework).
    echo ""
    echo "-- capture/ Rust (cargo check) --"
    if [ "$(uname -s)" != "Darwin" ]; then
        info "skipped — cargo check requires macOS (ScreenCaptureKit)"
        return
    fi
    if ! command -v cargo >/dev/null 2>&1; then
        info "skipped — cargo not found"
        return
    fi

    local cargo_out
    cargo_out="$(cd "$ROOT/capture" && cargo check 2>&1)"
    local cargo_exit=$?

    # Report each warning/error line
    local had_warn=0 had_err=0
    while IFS= read -r line; do
        case "$line" in
            *"warning:"*) warn "cargo: $line"; had_warn=1 ;;
            *"error["*|*"error:"*) err "cargo: $line"; had_err=1; any_issues=1
                ERROR_FILES="${ERROR_FILES}  cargo-error\n" ;;
        esac
    done <<EOF
$cargo_out
EOF

    if [ "$cargo_exit" -ne 0 ] && [ "$had_err" -eq 0 ]; then
        err "cargo check failed (exit $cargo_exit)"
        ERROR_FILES="${ERROR_FILES}  cargo-check\n"
    elif [ "$had_warn" -eq 0 ] && [ "$had_err" -eq 0 ]; then
        ok "cargo check — clean"
    fi
}

# ─── main ─────────────────────────────────────────────────────────────────────

echo "=== Q3IDE Linter ==="
echo "root: $ROOT"

run_clang_format
[ "$RUN_CPPCHECK" -eq 1 ] && run_cppcheck
run_basic_checks
run_guard_checks
run_rust_checks

echo ""
if [ "$ERRORS" -gt 0 ]; then
    echo "=== ${ERRORS} error(s), ${WARNINGS} warning(s) — files to fix: ==="
    printf '%b' "$ERROR_FILES"
else
    echo "=== ${ERRORS} error(s), ${WARNINGS} warning(s) ==="
fi
[ "$ERRORS" -eq 0 ]
