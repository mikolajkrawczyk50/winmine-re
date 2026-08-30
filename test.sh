#!/usr/bin/env bash
set -uo pipefail

PASS=0
FAIL=0

cleanup() {
    wineserver -k 2>/dev/null || true
    killall -9 Xvfb 2>/dev/null || true
    rm -f target_test.log winmine_hook.log send_click.exe
}
trap cleanup EXIT

echo "=== Windows Cross-Platform Hooking & Injection Test Suite ==="

# 1. CLI Usage
echo -n "Test 1 (Invalid CLI arguments): "
OUT=$(wine inject.exe 2>&1 || true)
if echo "$OUT" | grep -q "Usage: inject.exe"; then
    echo "PASS"
    ((PASS++))
else
    echo "FAIL"
    ((FAIL++))
fi

# 2. Missing target process
echo -n "Test 2 (Missing process detection): "
OUT=$(wine inject.exe non_existent_9999.exe mydll.dll 2>&1 || true)
if echo "$OUT" | grep -q "Process not found"; then
    echo "PASS"
    ((PASS++))
else
    echo "FAIL"
    ((FAIL++))
fi

# 3. Missing DLL
echo -n "Test 3 (Missing DLL failure): "
wine target.exe > target_test.log 2>&1 &
sleep 2
OUT=$(wine inject.exe target.exe nonexistent_dll_123.dll 2>&1 || true)
if echo "$OUT" | grep -q "LoadLibraryA returned NULL"; then
    echo "PASS"
    ((PASS++))
else
    echo "FAIL"
    ((FAIL++))
fi
cleanup

# 4. 64-bit Multi-Hook by Name
echo -n "Test 4 (64-bit Multi-Hook by Name): "
wine target.exe > target_test.log 2>&1 &
sleep 2
wine inject.exe target.exe mydll.dll > /dev/null 2>&1 || true
sleep 3
if grep -q "Hook installed: calculate_price" target_test.log && \
   grep -q "calculate_price INTERCEPTED!" target_test.log && \
   grep -q "execute_job return value altered:" target_test.log; then
    echo "PASS"
    ((PASS++))
else
    echo "FAIL"
    ((FAIL++))
fi
cleanup

# 5. 64-bit Multi-Hook by PID
echo -n "Test 5 (64-bit Multi-Hook by PID): "
wine target.exe > target_test.log 2>&1 &
sleep 2
WIN_PID=$(grep "PID:" target_test.log | awk '{print $4}')
wine inject.exe "$WIN_PID" mydll.dll > /dev/null 2>&1 || true
sleep 3
if grep -q "Hook installed: calculate_price" target_test.log && \
   grep -q "calculate_price completed in" target_test.log; then
    echo "PASS"
    ((PASS++))
else
    echo "FAIL"
    ((FAIL++))
fi
cleanup

# 6. 32-bit WINMINE.EXE 6-Function Recompiled Pipeline
echo -n "Test 6 (32-bit WINMINE.EXE 6-Function Pipeline): "
rm -f winmine_hook.log
Xvfb :99 -screen 0 1024x768x16 >/dev/null 2>&1 &
sleep 1
DISPLAY=:99 wine WINMINE.EXE >/dev/null 2>&1 &
sleep 2
DISPLAY=:99 wine inject32.exe WINMINE.EXE winmine_hook.dll >/dev/null 2>&1 || true
sleep 1

cat << "CLICK_EOF" > send_click.cpp
#include <windows.h>
int main() {
    HWND hwnd = FindWindowA("Minesweeper", NULL);
    if (hwnd) {
        LPARAM pos = MAKELPARAM(30, 70);
        PostMessageA(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, pos);
        PostMessageA(hwnd, WM_RBUTTONUP, 0, pos);
        Sleep(100);
        PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, pos);
        Sleep(100);
        PostMessageA(hwnd, WM_LBUTTONUP, 0, pos);
    }
    return 0;
}
CLICK_EOF
i686-w64-mingw32-g++ -static -o send_click.exe send_click.cpp
DISPLAY=:99 wine send_click.exe >/dev/null 2>&1 || true
rm -f send_click.cpp send_click.exe
sleep 2

if grep -q "Successfully installed all 6 recompiled hooks" winmine_hook.log && \
   grep -q "sub_10028D9" winmine_hook.log && \
   grep -q "sub_1002913" winmine_hook.log; then
    echo "PASS"
    ((PASS++))
else
    echo "FAIL"
    ((FAIL++))
fi
cleanup

echo "============================================================="
echo "Results: $PASS passed, $FAIL failed."
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
