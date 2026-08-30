#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// Global variable pointers in WINMINE.EXE address space
#define dword_1005194 (*(int*)0x01005194)
#define dword_100595C (*(void**)0x0100595C)
#define dword_1005A60 ((uint32_t*)0x01005A60)
#define hWnd_1005B24  (*(HWND*)0x01005B24)

#define ADDR_SUB_1002752 0x01002752
#define ADDR_SUB_1002785 0x01002785
#define ADDR_SUB_1002801 0x01002801
#define ADDR_SUB_100346A 0x0100346A

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

// -----------------------------------------------------------------------------
// 1. Recompiled sub_1002752: Render single digit glyph via SetDIBitsToDevice
// -----------------------------------------------------------------------------
int __stdcall sub_1002752(HDC hdc, int xDest, int a3) {
    log_msg("[sub_1002752] Drawing digit index %d at xDest=%d (hdc=%p)\n", a3, xDest, hdc);

    void* bmi = dword_100595C;
    if (!bmi) return 0;

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
        (char*)bmi + dword_1005A60[a3], // lpvBits
        (BITMAPINFO*)bmi,               // lpbmi
        0);                             // ColorUse: 0
}

// -----------------------------------------------------------------------------
// 2. Recompiled sub_1002785: Decompose mine count into 3 digits and draw
// -----------------------------------------------------------------------------
DWORD __stdcall sub_1002785(HDC hdc) {
    int v2, v3, v4;
    DWORD result;
    DWORD hdca = GetLayout(hdc);

    if ((hdca & 1) != 0) {
        SetLayout(hdc, 0);
    }

    if (dword_1005194 >= 0) {
        v3 = dword_1005194 % 100;
        v2 = dword_1005194 / 100;
    } else {
        v2 = 11; // 11 is the minus sign glyph
        v3 = -dword_1005194 % 100;
    }
    v4 = v3;

    log_msg("[sub_1002785] Rendering mine counter %d -> digits: [%d, %d, %d]\n",
            dword_1005194, v2, v4 / 10, v4 % 10);

    sub_1002752(hdc, 17, v2);
    sub_1002752(hdc, 30, v4 / 10);
    result = (DWORD)sub_1002752(hdc, 43, v4 % 10);

    if ((hdca & 1) != 0) {
        return SetLayout(hdc, hdca);
    }
    return result;
}

// -----------------------------------------------------------------------------
// 3. Recompiled sub_1002801: Acquire window DC, invoke draw, and release DC
// -----------------------------------------------------------------------------
int sub_1002801() {
    HWND hWnd = hWnd_1005B24;
    HDC DC = GetDC(hWnd);
    if (!DC) return 0;

    log_msg("[sub_1002801] Acquired DC=%p for hWnd=%p\n", DC, hWnd);
    sub_1002785(DC);
    return ReleaseDC(hWnd, DC);
}

// -----------------------------------------------------------------------------
// 4. Recompiled sub_100346A: Add delta to mine counter and refresh display
// -----------------------------------------------------------------------------
int __stdcall sub_100346A(int a1) {
    log_msg("[sub_100346A] Mine delta=%d (before=%d, after=%d)\n",
            a1, dword_1005194, dword_1005194 + a1);
    dword_1005194 += a1;
    return sub_1002801();
}

// -----------------------------------------------------------------------------
// Hook Installation
// -----------------------------------------------------------------------------
static void install_jmp_hook(void* target, void* hook, int size) {
    DWORD oldProtect;
    VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect);

    BYTE patch[16];
    patch[0] = 0xE9; // JMP rel32
    DWORD relOffset = (DWORD)hook - ((DWORD)target + 5);
    memcpy(&patch[1], &relOffset, 4);
    for (int i = 5; i < size; i++) {
        patch[i] = 0x90; // NOP
    }

    memcpy(target, patch, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    VirtualProtect(target, size, oldProtect, &oldProtect);
}

void install_all_hooks() {
    install_jmp_hook((void*)ADDR_SUB_1002752, (void*)&sub_1002752, 9);
    install_jmp_hook((void*)ADDR_SUB_1002785, (void*)&sub_1002785, 7);
    install_jmp_hook((void*)ADDR_SUB_1002801, (void*)&sub_1002801, 7);
    install_jmp_hook((void*)ADDR_SUB_100346A, (void*)&sub_100346A, 10);

    log_msg("[DLL] Successfully installed all 4 recompiled hooks (1002752, 1002785, 1002801, 100346A)!\n");
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        install_all_hooks();
    }
    return TRUE;
}
