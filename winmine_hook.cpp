#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define TARGET_FUNC_ADDR 0x0100346a
#define MINE_COUNT_ADDR  0x01005194

typedef void (__stdcall *UpdateMinesFunc_t)(int);

static BYTE g_origBytes[10];
static BYTE g_patchBytes[10];
static int g_callCount = 0;

static void log_msg(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    FILE* f = fopen("winmine_hook.log", "a");
    if (f) {
        fputs(buf, f);
        fclose(f);
    }

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        DWORD written;
        WriteFile(hOut, buf, len, &written, NULL);
    }
}

void __stdcall hooked_100346a(int delta) {
    g_callCount++;
    int beforeCount = *(int*)MINE_COUNT_ADDR;

    log_msg("[HOOK WRAPPER #%d] 100346a called with delta=%d (Mines before: %d)\n",
            g_callCount, delta, beforeCount);

    // Unhook temporarily to call original function
    DWORD oldProtect;
    VirtualProtect((void*)TARGET_FUNC_ADDR, 10, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)TARGET_FUNC_ADDR, g_origBytes, 10);
    FlushInstructionCache(GetCurrentProcess(), (void*)TARGET_FUNC_ADDR, 10);

    // Call original function cleanly
    ((UpdateMinesFunc_t)TARGET_FUNC_ADDR)(delta);

    // Re-hook
    memcpy((void*)TARGET_FUNC_ADDR, g_patchBytes, 10);
    FlushInstructionCache(GetCurrentProcess(), (void*)TARGET_FUNC_ADDR, 10);
    VirtualProtect((void*)TARGET_FUNC_ADDR, 10, oldProtect, &oldProtect);

    int afterCount = *(int*)MINE_COUNT_ADDR;
    log_msg("[HOOK WRAPPER #%d] Original 100346a executed! (Mines after: %d, expected: %d)\n",
            g_callCount, afterCount, beforeCount + delta);
}

void install_hook() {
    DWORD oldProtect;
    VirtualProtect((void*)TARGET_FUNC_ADDR, 10, PAGE_EXECUTE_READWRITE, &oldProtect);

    memcpy(g_origBytes, (void*)TARGET_FUNC_ADDR, 10);

    // 5-byte JMP: 0xE9 [rel32 offset] + 5 NOPs
    g_patchBytes[0] = 0xE9;
    DWORD relOffset = (DWORD)&hooked_100346a - (TARGET_FUNC_ADDR + 5);
    memcpy(&g_patchBytes[1], &relOffset, 4);
    for (int i = 5; i < 10; i++) {
        g_patchBytes[i] = 0x90;
    }

    memcpy((void*)TARGET_FUNC_ADDR, g_patchBytes, 10);
    FlushInstructionCache(GetCurrentProcess(), (void*)TARGET_FUNC_ADDR, 10);
    VirtualProtect((void*)TARGET_FUNC_ADDR, 10, oldProtect, &oldProtect);

    log_msg("[DLL] Testing wrapper hook installed on 100346a!\n");
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        install_hook();
    }
    return TRUE;
}
