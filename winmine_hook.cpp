#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define TARGET_FUNC_ADDR   0x01002752
#define DWORD_100595C_ADDR 0x0100595C
#define DWORD_1005A60_ADDR 0x01005A60

static BYTE g_origBytes[9];
static BYTE g_patchBytes[9];
static int g_drawCallCount = 0;

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

// Hook replacing sub_1002752 in WINMINE.EXE
int __stdcall hooked_sub_1002752(HDC hdc, int xDest, int a3) {
    g_drawCallCount++;
    void* dword_100595C = *(void**)DWORD_100595C_ADDR;
    uint32_t* dword_1005A60 = (uint32_t*)DWORD_1005A60_ADDR;

    log_msg("[HOOK sub_1002752 #%d] Drawing digit index %d at xDest=%d (hdc=%p, bmi=%p)\n",
            g_drawCallCount, a3, xDest, hdc, dword_100595C);

    if (!dword_100595C) {
        return 0;
    }

    return SetDIBitsToDevice(
        hdc,
        xDest,
        16,     // yDest: 16
        0x0D,   // w: 0xDu (13)
        0x17,   // h: 0x17u (23)
        0,      // xSrc: 0
        0,      // ySrc: 0
        0,      // StartScan: 0
        0x17,   // cLines: 0x17u (23)
        (char*)dword_100595C + dword_1005A60[a3], // lpvBits
        (BITMAPINFO*)dword_100595C,               // lpbmi
        0);                                       // ColorUse: 0
}

void install_hook() {
    DWORD oldProtect;
    VirtualProtect((void*)TARGET_FUNC_ADDR, 9, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Save original 9 bytes
    memcpy(g_origBytes, (void*)TARGET_FUNC_ADDR, 9);

    // 5-byte JMP: 0xE9 [rel32 offset] + 4 NOPs (0x90)
    g_patchBytes[0] = 0xE9;
    DWORD relOffset = (DWORD)&hooked_sub_1002752 - (TARGET_FUNC_ADDR + 5);
    memcpy(&g_patchBytes[1], &relOffset, 4);
    for (int i = 5; i < 9; i++) {
        g_patchBytes[i] = 0x90;
    }

    // Apply patch
    memcpy((void*)TARGET_FUNC_ADDR, g_patchBytes, 9);
    FlushInstructionCache(GetCurrentProcess(), (void*)TARGET_FUNC_ADDR, 9);
    VirtualProtect((void*)TARGET_FUNC_ADDR, 9, oldProtect, &oldProtect);

    log_msg("[DLL] Hook successfully installed on sub_1002752 (0x%08X)!\n", TARGET_FUNC_ADDR);
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        install_hook();
    }
    return TRUE;
}
