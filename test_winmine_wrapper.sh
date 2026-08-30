#!/usr/bin/env bash
set -uo pipefail

MODE="${1:-headless}"
LOG_FILE="winmine_hook.log"

cleanup() {
    if [ "$MODE" = "headless" ]; then
        killall -9 Xvfb 2>/dev/null || true
        wineserver -k 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "=== Minesweeper 100346a Hook Testing Wrapper ==="
rm -f "$LOG_FILE"

if [ "$MODE" = "headless" ]; then
    echo "[1/4] Starting headless Xvfb on :99..."
    Xvfb :99 -screen 0 1024x768x16 >/dev/null 2>&1 &
    sleep 1
    export DISPLAY=:99
else
    echo "[1/4] Using active user display (${DISPLAY:-:1})..."
    export DISPLAY="${DISPLAY:-:1}"
fi

echo "[2/4] Launching WINMINE.EXE..."
wine WINMINE.EXE >/dev/null 2>&1 &
MINE_PID=$!
sleep 2

echo "[3/4] Injecting winmine_hook.dll..."
wine inject32.exe WINMINE.EXE winmine_hook.dll
sleep 1

echo "[4/4] Dispatching test triggers (flagging and unflagging)..."
cat << 'CLICK_EOF' > send_click.cpp
#include <windows.h>
#include <stdio.h>
int main() {
    HWND hwnd = FindWindowA("Minesweeper", NULL);
    if (!hwnd) {
        printf("Minesweeper window not found\n");
        return 1;
    }
    LPARAM pos = MAKELPARAM(30, 70);
    // Flag square (should decrement remaining mines)
    PostMessageA(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, pos);
    PostMessageA(hwnd, WM_RBUTTONUP, 0, pos);
    Sleep(200);
    // Unflag square (should increment remaining mines)
    PostMessageA(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, pos);
    PostMessageA(hwnd, WM_RBUTTONUP, 0, pos);
    Sleep(200);
    return 0;
}
CLICK_EOF
i686-w64-mingw32-g++ -static -o send_click.exe send_click.cpp
wine send_click.exe
rm -f send_click.cpp send_click.exe
sleep 2

echo ""
echo "=== Test Results Log ==="
cat "$LOG_FILE"

echo ""
echo "=== Verification Assertions ==="
if grep -q "Testing wrapper hook installed on 100346a" "$LOG_FILE" && \
   grep -q "delta=-1" "$LOG_FILE" && \
   grep -q "delta=1" "$LOG_FILE" && \
   grep -q "Original 100346a executed!" "$LOG_FILE"; then
    echo ">> ALL HOOK & ORIGINAL EXECUTION ASSERTIONS PASSED <<"
else
    echo ">> TEST FAILED <<"
    exit 1
fi
