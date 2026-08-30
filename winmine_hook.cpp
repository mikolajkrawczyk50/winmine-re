#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// Global variable pointers in WINMINE.EXE address space
#define dword_1005194 (*(int*)0x01005194)
#define dword_100595C (*(void**)0x0100595C)
#define dword_1005A60 ((uint32_t*)0x01005A60)
#define dword_1005A00 (*(void**)0x01005A00)
#define dword_1005960 ((uint32_t*)0x01005960)
#define xRight_1005B2C (*(int*)0x01005B2C)
#define hWnd_1005B24  (*(HWND*)0x01005B24)

#define ADDR_SUB_1002752 0x01002752
#define ADDR_SUB_1002785 0x01002785
#define ADDR_SUB_1002801 0x01002801
#define ADDR_SUB_10028D9 0x010028D9
#define ADDR_SUB_1002913 0x01002913
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
// 4. Recompiled sub_10028D9: Render 24x24 Smiley Face Icon via SetDIBitsToDevice
// -----------------------------------------------------------------------------
int __stdcall sub_10028D9(HDC hdc, int a2) {
    void* bmi = dword_1005A00;
    int xDest = (xRight_1005B2C - 24) >> 1;

    log_msg("[sub_10028D9] Drawing face state %d at xDest=%d (hdc=%p)\n", a2, xDest, hdc);

    if (!bmi) return 0;

    return SetDIBitsToDevice(
        hdc,
        xDest,
        16,     // yDest: 16
        0x18,   // w: 0x18u (24)
        0x18,   // h: 0x18u (24)
        0,      // xSrc: 0
        0,      // ySrc: 0
        0,      // StartScan: 0
        0x18,   // cLines: 0x18u (24)
        (char*)bmi + dword_1005960[a2], // lpvBits
        (BITMAPINFO*)bmi,               // lpbmi
        0);                             // ColorUse: 0
}

// -----------------------------------------------------------------------------
// 5. Recompiled sub_1002913: Acquire DC, draw smiley face, and release DC
// -----------------------------------------------------------------------------
int __stdcall sub_1002913(int a1) {
    HWND hWnd = hWnd_1005B24;
    HDC DC = GetDC(hWnd);
    if (!DC) return 0;

    log_msg("[sub_1002913] Redraw face requested: state=%d, DC=%p\n", a1, DC);
    sub_10028D9(DC, a1);
    return ReleaseDC(hWnd, DC);
}

// -----------------------------------------------------------------------------
// 6. Recompiled draw_number_sub_1002825: Render 3-digit game timer on DC
// -----------------------------------------------------------------------------
#define dword_100579C (*(int*)0x0100579C)
#define dword_1005A90 (*(int*)0x01005A90)
#define ADDR_SUB_1002825 0x01002825

DWORD __stdcall draw_number_sub_1002825(HDC hdc) {
    int v1 = dword_100579C;
    DWORD Layout = GetLayout(hdc);

    if ((Layout & 1) != 0) {
        SetLayout(hdc, 0);
    }

    int v5 = v1 / 100;
    int v3 = v1 % 100;

    log_msg("[sub_1002825] Rendering timer %d -> digits: [%d, %d, %d] (hdc=%p)\n",
            v1, v5, v3 / 10, v3 % 10, hdc);

    sub_1002752(hdc, xRight_1005B2C - dword_1005A90 - 56, v5);
    sub_1002752(hdc, xRight_1005B2C - dword_1005A90 - 43, v3 / 10);
    DWORD result = (DWORD)sub_1002752(hdc, xRight_1005B2C - dword_1005A90 - 30, v3 % 10);

    if ((Layout & 1) != 0) {
        return SetLayout(hdc, Layout);
    }
    return result;
}

// -----------------------------------------------------------------------------
// 7. Recompiled redraw_number_sub_10028B5: Acquire DC, draw timer, and release DC
// -----------------------------------------------------------------------------
#define ADDR_SUB_10028B5 0x010028B5

int redraw_number_sub_10028B5() {
    HWND hWnd = hWnd_1005B24;
    HDC DC = GetDC(hWnd);
    if (!DC) return 0;

    log_msg("[sub_10028B5] Redraw timer requested (hWnd=%p, DC=%p)\n", hWnd, DC);
    draw_number_sub_1002825(DC);
    return ReleaseDC(hWnd, DC);
}

// -----------------------------------------------------------------------------
// 8. Recompiled set_draw_mode_sub_100293D: Set ROP2 draw mode / brush selection
// -----------------------------------------------------------------------------
#define dword_1005158 (*(HGDIOBJ*)0x01005158)
#define ADDR_SUB_100293D 0x0100293D

HGDIOBJ __stdcall set_draw_mode_sub_100293D(HDC hdc, char a2) {
    log_msg("[sub_100293D] Setting draw mode: hdc=%p, a2=%d\n", hdc, (int)a2);

    if ((a2 & 1) != 0) {
        return (HGDIOBJ)SetROP2(hdc, 16); // R2_WHITE (16)
    }

    SetROP2(hdc, 13); // R2_COPYPEN (13)
    return SelectObject(hdc, dword_1005158);
}

// -----------------------------------------------------------------------------
// 9. Recompiled sub_1002971: Draw 3D beveled rectangle borders
// -----------------------------------------------------------------------------
#define ADDR_SUB_1002971 0x01002971

int __stdcall sub_1002971(HDC hdc, int x, int a3, int a4, int y, int a6, int a7) {
    int result = 0;
    int v9 = 0;

    log_msg("[sub_1002971] DrawBevelRect: hdc=%p, rect=[%d, %d, %d, %d], border=%d, mode=%d\n",
            hdc, x, a3, a4, y, a6, a7);

    set_draw_mode_sub_100293D(hdc, (char)a7);

    if (a6 > 0) {
        int count = a6;
        v9 = a6;
        do {
            MoveToEx(hdc, x, --y, NULL);
            LineTo(hdc, x++, a3);
            LineTo(hdc, a4--, a3++);
            count--;
        } while (count != 0);
    }

    int v10 = v9 + 1;
    if (a7 < 2) {
        set_draw_mode_sub_100293D(hdc, (char)(a7 ^ 1));
    }

    result = v10 - 1;
    if (v10 != 1) {
        int v12 = v10 - 1;
        do {
            MoveToEx(hdc, x--, ++y, NULL);
            LineTo(hdc, ++a4, y);
            result = LineTo(hdc, a4, --a3);
            --v12;
        } while (v12 != 0);
    }

    return result;
}

// -----------------------------------------------------------------------------
// 10. Recompiled sub_1002A22: Draw all main window UI frames & containers
// -----------------------------------------------------------------------------
#define yBottom_1005B20 (*(int*)0x01005B20)
#define ADDR_SUB_1002A22 0x01002A22

int __stdcall sub_1002A22(HDC hdc) {
    log_msg("[sub_1002A22] Drawing all window frames (hdc=%p, xRight=%d, yBottom=%d)\n",
            hdc, xRight_1005B2C, yBottom_1005B20);

    int v1 = xRight_1005B2C - 1;
    int v2 = yBottom_1005B20 - 1;

    // 1. Outer raised border
    sub_1002971(hdc, 0, 0, xRight_1005B2C - 1, yBottom_1005B20 - 1, 3, 1);

    v1 -= 9;
    // 2. Sunken game board
    sub_1002971(hdc, 9, 52, v1, v2 - 9, 3, 0);

    // 3. Sunken header area
    sub_1002971(hdc, 9, 9, v1, 45, 2, 0);

    // 4. Mine counter inset frame
    sub_1002971(hdc, 16, 15, 56, 39, 1, 0);

    // 5. Timer counter inset frame
    int timerLeft = xRight_1005B2C - dword_1005A90 - 57;
    sub_1002971(hdc, timerLeft, 15, timerLeft + 40, 39, 1, 0);

    // 6. Smiley button frame
    int faceLeft = ((xRight_1005B2C - 24) >> 1) - 1;
    return sub_1002971(hdc, faceLeft, 15, faceLeft + 25, 40, 1, 2);
}

// -----------------------------------------------------------------------------
// 6. Recompiled sub_100346A: Add delta to mine counter and refresh display
// -----------------------------------------------------------------------------
int __stdcall sub_100346A(int a1) {
    log_msg("[sub_100346A] Mine delta=%d (before=%d, after=%d)\n",
            a1, dword_1005194, dword_1005194 + a1);
    dword_1005194 += a1;
    return sub_1002801();
}

// -----------------------------------------------------------------------------
// Thread Freezing / Unfreezing Lifecycle
// -----------------------------------------------------------------------------
#include <tlhelp32.h>

#define MAX_SUSPENDED_THREADS 128
static HANDLE g_suspendedThreads[MAX_SUSPENDED_THREADS];
static int g_suspendedCount = 0;

static void freeze_other_threads() {
    g_suspendedCount = 0;
    DWORD currentPid = GetCurrentProcessId();
    DWORD currentTid = GetCurrentThreadId();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te;
    te.dwSize = sizeof(te);

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

static void unfreeze_other_threads() {
    for (int i = 0; i < g_suspendedCount; i++) {
        ResumeThread(g_suspendedThreads[i]);
        CloseHandle(g_suspendedThreads[i]);
    }
    g_suspendedCount = 0;
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
    // 1. Freeze all other threads to prevent race conditions during patching
    freeze_other_threads();

    // 2. Apply hooks safely
    install_jmp_hook((void*)ADDR_SUB_1002752, (void*)&sub_1002752, 9);
    install_jmp_hook((void*)ADDR_SUB_1002785, (void*)&sub_1002785, 7);
    install_jmp_hook((void*)ADDR_SUB_1002801, (void*)&sub_1002801, 7);
    install_jmp_hook((void*)ADDR_SUB_1002825, (void*)&draw_number_sub_1002825, 7);
    install_jmp_hook((void*)ADDR_SUB_10028B5, (void*)&redraw_number_sub_10028B5, 7);
    install_jmp_hook((void*)ADDR_SUB_10028D9, (void*)&sub_10028D9, 9);
    install_jmp_hook((void*)ADDR_SUB_1002913, (void*)&sub_1002913, 7);
    install_jmp_hook((void*)ADDR_SUB_100293D, (void*)&set_draw_mode_sub_100293D, 7);
    install_jmp_hook((void*)ADDR_SUB_1002971, (void*)&sub_1002971, 8);
    install_jmp_hook((void*)ADDR_SUB_1002A22, (void*)&sub_1002A22, 7);
    install_jmp_hook((void*)ADDR_SUB_100346A, (void*)&sub_100346A, 10);

    // 3. Unfreeze (Resume) threads
    unfreeze_other_threads();

    log_msg("[DLL] Successfully installed all 11 recompiled hooks (freeze -> hook -> unfreeze)!\n");
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        install_all_hooks();
    }
    return TRUE;
}
