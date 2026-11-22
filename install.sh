#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: $0 <path-to-wine-or-proton-prefix>"
    echo "Example: $0 /path/to/your/prefix"
    exit 1
fi

PREFIX="$1"
# Support both Steam compatdata dirs (which contain pfx/) and direct Wine prefixes
if [ -d "$PREFIX/pfx/drive_c/windows/system32" ] && [ ! -L "$PREFIX/pfx" ]; then
    PFX="$PREFIX/pfx"
else
    PFX="$PREFIX"
fi

SYS32="$PFX/drive_c/windows/system32"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -d "$SYS32" ]; then
    echo "Error: $SYS32 not found. Launch the prefix once first so Proton creates drive_c."
    exit 1
fi

# Back up original symlink/file once
if [ ! -e "$SYS32/wintrust.dll.bak" ] && [ ! -L "$SYS32/wintrust.dll.bak" ]; then
    if [ -e "$SYS32/wintrust.dll" ] || [ -L "$SYS32/wintrust.dll" ]; then
        mv "$SYS32/wintrust.dll" "$SYS32/wintrust.dll.bak"
    fi
else
    rm -f "$SYS32/wintrust.dll"
fi

cp "$SCRIPT_DIR/wintrust.dll" "$SYS32/wintrust.dll"

# Prevent Proton prefix updates from overwriting system32/wintrust.dll
if [ -f "$PFX/tracked_files" ]; then
    grep -vx "drive_c/windows/system32/wintrust.dll" "$PFX/tracked_files" > "$PFX/tracked_files.tmp" || true
    mv "$PFX/tracked_files.tmp" "$PFX/tracked_files"
fi

echo "Installed wintrust.dll into $SYS32"
echo ""
echo "Now set this DLL override when launching the game:"
echo "  Steam launch options:  WINEDLLOVERRIDES=\"wintrust=n\" %command%"
echo "  Lutris:                Configure -> Runner options -> DLL overrides -> wintrust = n"
