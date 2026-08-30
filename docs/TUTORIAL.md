# Comprehensive Guide: Binary Reverse Engineering, Recompilation & Hooking Workflow

This guide provides a complete, repeatable end-to-end workflow for reverse engineering functions in Windows binaries (PE32/PE32+), rewriting/recompiling them in native C/C++, and injecting them as live inline hooks under Linux using MinGW-w64 and Wine.

---

## 1. Environment & Toolchain Setup

### Required Linux Packages
- **64-bit Windows Cross-Compiler:** `mingw64-cross-gcc-c++` (`x86_64-w64-mingw32-g++`)
- **32-bit Windows Cross-Compiler:** `mingw32-cross-gcc-c++` (`i686-w64-mingw32-g++`)
- **Windows Compatibility Layer:** `wine` (`wine`, `wineserver`)
- **Headless Display for GUI Testing:** `Xvfb`
- **Binary Inspection Tools:** `objdump` (`x86_64-w64-mingw32-objdump`, `objdump -d`)

---

## 2. Step-by-Step Reverse Engineering & Recompilation Workflow

```
[Target Binary] ──> [Disassemble / Decompile] ──> [Map Variables & APIs]
                                                          │
                                                          ▼
[Live Process] <── [Inject DLL (LoadLibraryA)] <── [Recompiled C Hook DLL]
```

---

### Step 1: Disassemble the Target Function

Use `objdump` to inspect the target function's disassembly, calling convention, and length:

```bash
# Disassemble specific address range
objdump -d --start-address=0x01002752 --stop-address=0x01002785 target.exe
```

#### Key Elements to Extract:
1. **Calling Convention**:
   - `ret $0xC` → `__stdcall` with 3 arguments (12 bytes on stack).
   - `ret` (with caller cleaning stack) → `__cdecl`.
2. **Arguments**:
   - Stack offsets: `0x4(%esp)` = arg1, `0x8(%esp)` = arg2, `0xC(%esp)` = arg3.
3. **Global Variables & Pointers**:
   - Memory addresses in instructions like `mov 0x1005194, %eax` refer to global state.
4. **IAT API Calls**:
   - `call *0x100104c` refers to imported DLL functions (check IAT table via `objdump -p`).

---

### Step 2: Write Recompiled C/C++ Code

Map the extracted disassembly into standard C/C++:

```cpp
#include <windows.h>
#include <stdint.h>

// 1. Map global variables to their fixed virtual addresses in the target process
#define g_mineCount    (*(int*)0x01005194)
#define g_pBitmapInfo  (*(void**)0x0100595C)
#define g_offsetTable  ((uint32_t*)0x01005A60)
#define g_hMainWindow  (*(HWND*)0x01005B24)

// 2. Recompile the function logic in standard C
int __stdcall sub_1002752(HDC hdc, int xDest, int digitIndex) {
    void* bmi = g_pBitmapInfo;
    if (!bmi) return 0;

    return SetDIBitsToDevice(
        hdc,
        xDest,
        16,     // yDest
        13,     // width
        23,     // height
        0, 0, 0, 23,
        (char*)bmi + g_offsetTable[digitIndex], // pixel buffer
        (BITMAPINFO*)bmi,
        0);
}
```

---

### Step 3: Sizing and Installing Inline Hooks

Hooks redirect execution from the target's original entry point to the recompiled C function.

#### A. 32-bit (x86) Inline Hook: 5-byte Relative JMP (`0xE9`)
- Minimum hook size: **5 bytes** (`0xE9 [4-byte rel32 offset]`).
- Calculate relative offset: `DWORD relOffset = (DWORD)hookFunc - (targetAddr + 5);`
- Pad remaining instruction bytes with `0x90` (`NOP`) to preserve instruction boundaries.

```cpp
void install_32bit_hook(void* targetAddr, void* hookFunc, int hookLength) {
    DWORD oldProtect;
    VirtualProtect(targetAddr, hookLength, PAGE_EXECUTE_READWRITE, &oldProtect);

    BYTE patch[16];
    patch[0] = 0xE9; // JMP rel32
    DWORD relOffset = (DWORD)hookFunc - ((DWORD)targetAddr + 5);
    memcpy(&patch[1], &relOffset, 4);

    for (int i = 5; i < hookLength; i++) {
        patch[i] = 0x90; // NOP padding
    }

    memcpy(targetAddr, patch, hookLength);
    FlushInstructionCache(GetCurrentProcess(), targetAddr, hookLength);
    VirtualProtect(targetAddr, hookLength, oldProtect, &oldProtect);
}
```

#### B. 64-bit (x86_64) Inline Hook: 14-byte Absolute JMP
- Uses 14-byte RIP-relative jump: `\xFF\x25\x00\x00\x00\x00 [8-byte 64-bit address]`.

```cpp
void install_64bit_hook(void* targetAddr, void* hookFunc) {
    DWORD oldProtect;
    VirtualProtect(targetAddr, 14, PAGE_EXECUTE_READWRITE, &oldProtect);

    BYTE patch[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    uint64_t addr = (uint64_t)hookFunc;
    memcpy(&patch[6], &addr, 8);

    memcpy(targetAddr, patch, 14);
    FlushInstructionCache(GetCurrentProcess(), targetAddr, 14);
    VirtualProtect(targetAddr, 14, oldProtect, &oldProtect);
}
```

#### C. Thread Safety: Freezing -> Hooking -> Unfreezing Lifecycle

> **CRITICAL RULE**: In multithreaded processes, you **MUST** suspend all other threads in the target process before modifying code bytes. Otherwise, other threads executing the target function concurrently will hit partially-written opcodes or invalid RIP/EIP locations and crash.

```cpp
#include <tlhelp32.h>

#define MAX_SUSPENDED_THREADS 128
static HANDLE g_suspendedThreads[MAX_SUSPENDED_THREADS];
static int g_suspendedCount = 0;

void freeze_other_threads() {
    g_suspendedCount = 0;
    DWORD currentPid = GetCurrentProcessId();
    DWORD currentTid = GetCurrentThreadId();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te = { sizeof(te) };
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == currentPid && te.th32ThreadID != currentTid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    SuspendThread(hThread);
                    if (g_suspendedCount < MAX_SUSPENDED_THREADS) {
                        g_suspendedThreads[g_suspendedCount++] = hThread;
                    } else {
                        CloseHandle(hThread);
                    }
                }
            }
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

void unfreeze_other_threads() {
    for (int i = 0; i < g_suspendedCount; i++) {
        ResumeThread(g_suspendedThreads[i]);
        CloseHandle(g_suspendedThreads[i]);
    }
    g_suspendedCount = 0;
}

// Full Hooking Sequence:
void install_hooks_safely() {
    freeze_other_threads();    // 1. Freeze
    install_jmp_hook(...);     // 2. Patch memory
    unfreeze_other_threads();  // 3. Unfreeze
}
```

---

### Step 4: Process DLL Injector

Create a universal injector using standard Win32 APIs:

```cpp
DWORD pid = findProcess("target.exe"); // or numeric PID via atoi
HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

char fullPath[MAX_PATH];
GetFullPathNameA("hook.dll", MAX_PATH, fullPath, NULL);

void* mem = VirtualAllocEx(proc, NULL, strlen(fullPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
WriteProcessMemory(proc, mem, fullPath, strlen(fullPath) + 1, NULL);

LPTHREAD_START_ROUTINE pfnLoadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
HANDLE thread = CreateRemoteThread(proc, NULL, 0, pfnLoadLib, mem, 0, NULL);

if (thread) {
    WaitForSingleObject(thread, 5000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    if (exitCode != 0) {
        printf("Injected successfully! HMODULE: 0x%p\n", (void*)exitCode);
    }
}
VirtualFreeEx(proc, mem, 0, MEM_RELEASE);
CloseHandle(proc);
```

---

### Step 5: Makefile Cross-Compilation Rules

Always compile with `-static` to bundle C/C++ runtime dependencies:

```makefile
CXX64 = x86_64-w64-mingw32-g++
CXX32 = i686-w64-mingw32-g++
CXXFLAGS = -static -O2

# 32-bit hook DLL
hook32.dll: hook32.cpp
	$(CXX32) -shared $(CXXFLAGS) -o $@ $< -lgdi32

# 32-bit injector
inject32.exe: inject32.cpp
	$(CXX32) $(CXXFLAGS) -o $@ $< -ladvapi32
```

---

### Step 6: Automated Headless Testing (`test.sh`)

Test GUI and CLI hooks without opening desktop windows using `Xvfb`:

```bash
#!/usr/bin/env bash
set -uo pipefail

# 1. Start headless virtual framebuffer
Xvfb :99 -screen 0 1024x768x16 >/dev/null 2>&1 &
XVFB_PID=$!
export DISPLAY=:99

# 2. Launch target Windows process
wine target.exe >/dev/null 2>&1 &
TARGET_PID=$!
sleep 2

# 3. Inject hook DLL
wine inject32.exe target.exe hook32.dll

# 4. Assert hook log output
if grep -q "Hook successfully installed" hook.log; then
    echo "PASS"
else
    echo "FAIL"
fi

# Cleanup
kill -9 $TARGET_PID $XVFB_PID 2>/dev/null || true
wineserver -k 2>/dev/null || true
```

---

## 3. Best Practices & Troubleshooting Checklist

| Problem | Cause | Solution |
|---|---|---|
| `libstdc++-6.dll missing` | Dynamic runtime linkage | Pass `-static` flag to MinGW compiler |
| Crash after hook jump | Split instruction in middle | Inspect disassembly bytes; pad full instruction length with NOPs (`0x90`) |
| `LoadLibraryA` returns NULL | DLL architecture mismatch | Compile 32-bit DLL for 32-bit PE, 64-bit DLL for 64-bit PE |
| `DllMain` not executed | C++ name mangling | Declare `extern "C" BOOL WINAPI DllMain(...)` |
| GUI app crashes in CI | No active X server | Run inside virtual framebuffer: `Xvfb :99 & export DISPLAY=:99` |
| Zombie Wine processes | Wine background server held open | Add `wineserver -k` to script exit traps |
