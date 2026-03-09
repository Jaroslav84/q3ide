#!/bin/sh
# fmt.sh — Auto-format all q3ide C source files.
# Run this on macOS where clang-format is available (brew install clang-format).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q3IDE_DIR="$ROOT/quake3e/code/q3ide"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format not found — install: brew install clang-format"
    exit 1
fi

echo "Formatting $Q3IDE_DIR..."
for f in "$Q3IDE_DIR"/*.c "$Q3IDE_DIR"/*.h; do
    [ -f "$f" ] || continue
    clang-format -i "$f"
    echo "  formatted: $(basename "$f")"
done
echo "Done."
