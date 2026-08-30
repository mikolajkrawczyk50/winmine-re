#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Building latest binaries ==="
make all

export DISPLAY="${DISPLAY:-:1}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"

LOG_FILE="winmine_hook.log"
rm -f "$LOG_FILE"
touch "$LOG_FILE"

echo "=== Launching WINMINE.EXE on $DISPLAY ==="
wine WINMINE.EXE &
MINE_PID=$!

cleanup() {
    echo ""
    echo "=== Shutting down ==="
    kill -9 "$MINE_PID" 2>/dev/null || true
    kill -9 "$TAIL_PID" 2>/dev/null || true
    wineserver -k 2>/dev/null || true
}
trap cleanup EXIT INT TERM

sleep 1.5

echo "=== Injecting winmine_hook.dll ==="
wine inject32.exe WINMINE.EXE winmine_hook.dll

echo ""
echo "=========================================================="
echo " WINMINE.EXE is running on your screen!"
echo " Interacting with the game (timer ticks, flag clicks, etc.)"
echo " will trigger sub_1002752 digit drawing hook."
echo " Live hook output:"
echo " (Press Ctrl+C or close the window to exit)"
echo "=========================================================="
echo ""

tail -n +1 -f "$LOG_FILE" &
TAIL_PID=$!

# Wait for Minesweeper window to close
wait "$MINE_PID" 2>/dev/null || true
