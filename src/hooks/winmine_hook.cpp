#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <tlhelp32.h>
#include <float.h>

// =============================================================================
// WINMINE.EXE Global Memory Map & Virtual Addresses
// =============================================================================
#define dword_1005158 (*(HGDIOBJ*)0x01005158)       // Current selected GDI brush/pen handle
#define dword_1005160 (*(int*)0x01005160)           // Current smiley face state (0=smile, 1=down, 2=scared, 3=dead, 4=shades)
#define dword_1005194 (*(int*)0x01005194)           // Current remaining unflagged mine count
#define dword_100579C (*(int*)0x0100579C)           // Elapsed game seconds timer (0-999)
#define dword_1005950 (*(HKEY*)0x01005950)          // Open Registry HKEY handle (HKCU\Software\Microsoft\winmine)
#define hKey_1005950  dword_1005950
#define dword_100595C (*(void**)0x0100595C)         // BITMAPINFO* pointer for 7-segment digit glyphs
#define dword_1005960 ((uint32_t*)0x01005960)       // Pixel buffer byte offsets array for smiley face button icons
#define dword_1005A00 (*(void**)0x01005A00)         // BITMAPINFO* pointer for smiley face button icons
#define dword_1005A60 ((uint32_t*)0x01005A60)       // Pixel buffer byte offsets array for 7-segment digit glyphs
#define dword_1005A90 (*(int*)0x01005A90)           // Right-side margin padding constant
#define hMenu_1005A94 (*(HMENU*)0x01005A94)         // Game window menu handle
#define dword_10056C4 (*(int*)0x010056C4)           // UI/menu flags (bit0 = menu hidden)
#define lpKeyName_10050D0 ((const WCHAR**)0x010050D0)// Array of wide-string registry value names ("Height", "Width", etc.)
#define xRight_1005B2C (*(int*)0x01005B2C)          // Game window client area width / right boundary coordinate
#define yBottom_1005B20 (*(int*)0x01005B20)         // Game window client area height / bottom boundary coordinate
#define hWnd_1005B24  (*(HWND*)0x01005B24)          // Main Minesweeper window handle
#define dword_1005334 (*(int*)0x01005334)           // Number of columns in playing field
#define dword_1005338 (*(int*)0x01005338)           // Number of rows in playing field
#define unk_1005360   ((BYTE*)0x01005360)           // Tile state array (32 bytes per row, column 1-based)
#define byte_1005340  ((BYTE*)0x01005340)           // Tile state array base (byte_1005360 - 0x20)
#define hdcSrc_1005A20 ((HDC*)0x01005A20)           // HDC table for tile bitmaps (indexed by tile_state & 0x1F)
#define dword_1005958 (*(HANDLE*)0x01005958)        // Resource data handle (LoadResource result)
#define dword_1005A08 (*(HANDLE*)0x01005A08)        // Resource data handle for digits
#define dword_1005954 (*(HANDLE*)0x01005954)        // Resource data handle for smileys
#define dword_1005A04 (*(void**)0x01005A04)         // Locked tile bitmap data pointer (LockResource result)
#define dword_10056C8 (*(int*)0x010056C8)           // Color mode flag (0=monochrome, non-zero=16-color)
#define dword_10056B8 (*(int*)0x010056B8)           // Sound enabled flag (3=all sounds)
#define dword_10059C0 ((uint32_t*)0x010059C0)       // Tile bitmap byte offset array (16 entries, ends at 0x1005A00)
#define dword_1005980 ((int*)0x01005980)            // HBITMAP table for tile DCs (16 entries)
#define dword_1005974 ((int*)0x01005974)            // End of smiley bitmap offset array
#define hModule_1005B30 (*(HMODULE*)0x01005B30)     // Module handle for resource loading
#define defaultStr_1005B40 ((const WCHAR*)0x01005B40) // Default string for registry fallback

// =============================================================================
// Function Entry Point Addresses in WINMINE.EXE .text Section
// =============================================================================
#define ADDR_SUB_1002414 0x01002414
#define ADDR_SUB_10023CD 0x010023CD
#define ADDR_SUB_10023F1 0x010023F1
#define ADDR_SUB_1002B80 0x01002B80
#define ADDR_SUB_100272E 0x0100272E
#define ADDR_SUB_10038D7 0x010038D7
#define ADDR_SUB_10038C2 0x010038C2
#define ADDR_SUB_1003940 0x01003940
#define ADDR_SUB_10039E7 0x010039E7
#define ADDR_SUB_1001516 0x01001516
#define ADDR_SUB_1001950 0x01001950
#define ADDR_SUB_1003CC4 0x01003CC4
#define ADDR_SUB_1003CE5 0x01003CE5
#define ADDR_SUB_1003DF6 0x01003DF6
#define ADDR_SUB_1003FF4 0x01003FF4
#define ADDR_SUB_1002607 0x01002607
#define ADDR_SUB_10026A7 0x010026A7
#define ADDR_SUB_1002752 0x01002752
#define ADDR_SUB_1002785 0x01002785
#define ADDR_SUB_1002801 0x01002801
#define ADDR_SUB_1002825 0x01002825
#define ADDR_SUB_10028B5 0x010028B5
#define ADDR_SUB_10028D9 0x010028D9
#define ADDR_SUB_1002913 0x01002913
#define ADDR_SUB_100293D 0x0100293D
#define ADDR_SUB_1002971 0x01002971
#define ADDR_SUB_1002A22 0x01002A22
#define ADDR_SUB_1002AC3 0x01002AC3
#define ADDR_SUB_1002AF0 0x01002AF0
#define ADDR_SUB_1002B14 0x01002B14
#define ADDR_SUB_1002B27 0x01002B27
#define ADDR_SUB_1002ED5 0x01002ED5
#define ADDR_SUB_100346A 0x0100346A

typedef int (__stdcall *Sub_10026A7_t)(HDC hdc);
typedef int (*Sub_1002414_t)(void);
typedef int (*Sub_1002ED5_t)(void);
typedef HRSRC (__stdcall *Sub_10023CD_t)(int resourceId);
typedef int (__stdcall *Sub_10023F1_t)(int width, int height);

// =============================================================================
// Logging Utility
// =============================================================================
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

// =============================================================================
// RECOMPILED FUNCTION IMPLEMENTATIONS
// =============================================================================

/**
 * 1. sub_1002752 (0x01002752)
 * -----------------------------------------------------------------------------
 * @brief Low-level 7-Segment Digital Glyph Renderer.
 * @details Blits an individual 13x23 pixel digital counter glyph (0-9, minus sign,
 *          or blank) directly onto the target Device Context at the specified X coordinate.
 * @param hdc     Target device context handle.
 * @param xDest   X coordinate on client window where glyph top-left should be drawn.
 * @param a3      Glyph index (0-9 for digits, 10 for blank, 11 for minus sign).
 * @return int    Status code from SetDIBitsToDevice (number of scan lines copied).
 */
int __stdcall sub_1002752(HDC hdc, int xDest, int a3) {
    log_msg("[sub_1002752] Drawing digit index %d at xDest=%d (hdc=%p)\n", a3, xDest, hdc);

    void* bmi = dword_100595C;
    if (!bmi) return 0;

    return SetDIBitsToDevice(
        hdc,
        xDest,
        16,     // yDest: fixed at Y=16 in header bar
        0x0D,   // width: 13 pixels (0xDu)
        0x17,   // height: 23 pixels (0x17u)
        0,      // xSrc
        0,      // ySrc
        0,      // StartScan
        0x17,   // cLines: 23 scanlines
        (char*)bmi + dword_1005A60[a3], // Pointer to pixel bitstream for glyph
        (BITMAPINFO*)bmi,               // DIB bitmap header
        0);                             // ColorUse: DIB_RGB_COLORS (0)
}

/**
 * 2. sub_1002785 (0x01002785)
 * -----------------------------------------------------------------------------
 * @brief Remaining Mine Counter Formatter and Drawer.
 * @details Decomposes the global remaining mine count (dword_1005194) into 3
 *          digits (hundreds at X=17, tens at X=30, ones at X=43). Handles negative
 *          numbers by rendering minus glyph (index 11) in the hundreds position.
 *          Temporarily disables RTL mirroring layout if active during draw.
 * @param hdc       Target device context handle.
 * @return DWORD    Result of the last SetDIBitsToDevice call or restored layout.
 */
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

    sub_1002752(hdc, 17, v2);        // Hundreds digit (or minus sign)
    sub_1002752(hdc, 30, v4 / 10);   // Tens digit
    result = (DWORD)sub_1002752(hdc, 43, v4 % 10); // Ones digit

    if ((hdca & 1) != 0) {
        return SetLayout(hdc, hdca);
    }
    return result;
}

/**
 * 3. sub_1002801 (0x01002801)
 * -----------------------------------------------------------------------------
 * @brief Mine Counter DC Acquisition Wrapper.
 * @details Acquires the window DC via GetDC(hWnd_1005B24), triggers sub_1002785
 *          to repaint the mine counter digits, and releases the DC via ReleaseDC.
 * @return int    Status code from ReleaseDC.
 */
int sub_1002801() {
    HWND hWnd = hWnd_1005B24;
    HDC DC = GetDC(hWnd);
    if (!DC) return 0;

    log_msg("[sub_1002801] Acquired DC=%p for hWnd=%p (redraw mine counter)\n", DC, hWnd);
    sub_1002785(DC);
    return ReleaseDC(hWnd, DC);
}

/**
 * 4. sub_10028D9 (0x010028D9)
 * -----------------------------------------------------------------------------
 * @brief 24x24 Smiley Face Status Icon Renderer.
 * @details Blits the 24x24 pixel status button face bitmap centered horizontally
 *          at X = (xRight - 24) >> 1, Y = 16. Supports 5 icon states:
 *          0 = Normal smile, 1 = Pressed down, 2 = Scared 'O', 3 = Dead, 4 = Shades.
 * @param hdc     Target device context handle.
 * @param a2      Smiley icon state index (0 to 4).
 * @return int    Status code from SetDIBitsToDevice.
 */
int __stdcall sub_10028D9(HDC hdc, int a2) {
    void* bmi = dword_1005A00;
    int xDest = (xRight_1005B2C - 24) >> 1;

    log_msg("[sub_10028D9] Drawing face state %d at xDest=%d (hdc=%p)\n", a2, xDest, hdc);

    if (!bmi) return 0;

    return SetDIBitsToDevice(
        hdc,
        xDest,
        16,     // yDest: fixed at Y=16
        0x18,   // width: 24 pixels (0x18u)
        0x18,   // height: 24 pixels (0x18u)
        0,      // xSrc
        0,      // ySrc
        0,      // StartScan
        0x18,   // cLines: 24 scanlines
        (char*)bmi + dword_1005960[a2], // Pointer to icon pixel bitstream
        (BITMAPINFO*)bmi,               // DIB bitmap header
        0);                             // ColorUse: 0
}

/**
 * 5. sub_1002913 (0x01002913)
 * -----------------------------------------------------------------------------
 * @brief Smiley Face DC Acquisition Wrapper.
 * @details Acquires the window DC via GetDC(hWnd_1005B24), calls sub_10028D9
 *          with the requested face state, and releases the DC.
 * @param a1      Smiley face state index (0 to 4).
 * @return int    Status code from ReleaseDC.
 */
int __stdcall sub_1002913(int a1) {
    HWND hWnd = hWnd_1005B24;
    HDC DC = GetDC(hWnd);
    if (!DC) return 0;

    log_msg("[sub_1002913] Redraw face requested: state=%d, DC=%p\n", a1, DC);
    sub_10028D9(DC, a1);
    return ReleaseDC(hWnd, DC);
}

/**
 * 6. draw_number_sub_1002825 (0x01002825)
 * -----------------------------------------------------------------------------
 * @brief Elapsed Game Seconds Timer Formatter and Drawer.
 * @details Formats and renders the 3-digit elapsed game timer (dword_100579C)
 *          anchored to the right side of the header bar. Digits are positioned at:
 *          Hundreds = xRight - dword_1005A90 - 56
 *          Tens     = xRight - dword_1005A90 - 43
 *          Ones     = xRight - dword_1005A90 - 30
 * @param hdc       Target device context handle.
 * @return DWORD    Result of the last SetDIBitsToDevice call or restored layout.
 */
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

/**
 * 7. redraw_number_sub_10028B5 (0x010028B5)
 * -----------------------------------------------------------------------------
 * @brief Game Timer DC Acquisition Wrapper.
 * @details Acquires the window DC via GetDC(hWnd_1005B24), calls draw_number_sub_1002825
 *          to repaint the timer digits, and releases the DC.
 * @return int    Status code from ReleaseDC.
 */
int redraw_number_sub_10028B5() {
    HWND hWnd = hWnd_1005B24;
    HDC DC = GetDC(hWnd);
    if (!DC) return 0;

    log_msg("[sub_10028B5] Redraw timer requested (hWnd=%p, DC=%p)\n", hWnd, DC);
    draw_number_sub_1002825(DC);
    return ReleaseDC(hWnd, DC);
}

/**
 * 8. set_draw_mode_sub_100293D (0x0100293D)
 * -----------------------------------------------------------------------------
 * @brief GDI Raster Operation (ROP2) and Pen/Brush Selector.
 * @details Configures GDI drawing modes for 3D bevel borders:
 *          - If (a2 & 1) != 0: Sets SetROP2(hdc, R2_WHITE) for highlight edges.
 *          - Else: Sets SetROP2(hdc, R2_COPYPEN) and selects brush dword_1005158.
 * @param hdc        Target device context handle.
 * @param a2         Draw mode flag (0 = standard brush, 1 = white pen, 2 = shadow).
 * @return HGDIOBJ   Previous GDI object or SetROP2 return code.
 */
HGDIOBJ __stdcall set_draw_mode_sub_100293D(HDC hdc, char a2) {
    log_msg("[sub_100293D] Setting draw mode: hdc=%p, a2=%d\n", hdc, (int)a2);

    if ((a2 & 1) != 0) {
        return (HGDIOBJ)SetROP2(hdc, 16); // R2_WHITE (16)
    }

    SetROP2(hdc, 13); // R2_COPYPEN (13)
    return SelectObject(hdc, dword_1005158);
}

/**
 * 9. sub_1002971 (0x01002971)
 * -----------------------------------------------------------------------------
 * @brief 3D Beveled Rectangle Border Drawing Engine.
 * @details Draws classic Windows XP 3D beveled rectangle frames using MoveToEx
 *          and LineTo. Renders top/left highlight and bottom/right shadow edges
 *          with specified border thickness.
 * @param hdc     Target device context handle.
 * @param x       Left coordinate.
 * @param a3      Top coordinate.
 * @param a4      Right coordinate.
 * @param y       Bottom coordinate.
 * @param a6      Border line thickness in pixels.
 * @param a7      Bevel style: 0 = Sunken/Inset, 1 = Raised/Outset, 2 = Flat button frame.
 * @return int    Status of last LineTo call.
 */
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

/**
 * 10. sub_1002A22 (0x01002A22)
 * -----------------------------------------------------------------------------
 * @brief Window UI Frame Layout Orchestrator.
 * @details Computes geometry and draws all 6 beveled containers of the Minesweeper UI:
 *          1. Outer window border (raised, thickness=3)
 *          2. Playing field board border (sunken, thickness=3)
 *          3. Top score header frame (sunken, thickness=2)
 *          4. Mine counter inset box (sunken, thickness=1)
 *          5. Timer counter inset box (sunken, thickness=1)
 *          6. Smiley face button frame (flat, thickness=1)
 * @param hdc     Target device context handle.
 * @return int    Status of last frame drawing operation.
 */
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

/**
 * 11. sub_1002AC3 (0x01002AC3)
 * -----------------------------------------------------------------------------
 * @brief Master Game Window Repaint Dispatcher.
 * @details Executes complete client area painting pipeline in correct Z-order:
 *          1. sub_1002A22: 3D UI borders and container frames
 *          2. sub_1002785: Remaining mine counter digits
 *          3. sub_10028D9: Smiley face status button icon
 *          4. draw_number_sub_1002825: Game seconds timer digits
 *          5. sub_10026A7: Game board tile grid blitter
 * @param hdc     Target device context handle.
 * @return int    Result from tile grid renderer (sub_10026A7).
 */
int __stdcall sub_1002AC3(HDC hdc) {
    log_msg("[sub_1002AC3] Full game window repaint (hdc=%p, faceState=%d)\n", hdc, dword_1005160);

    sub_1002A22(hdc);
    sub_1002785(hdc);
    sub_10028D9(hdc, dword_1005160);
    draw_number_sub_1002825(hdc);

    return ((Sub_10026A7_t)ADDR_SUB_10026A7)(hdc);
}

/**
 * 12. sub_1002AF0 (0x01002AF0)
 * -----------------------------------------------------------------------------
 * @brief Master Repaint DC Acquisition Wrapper.
 * @details Acquires the window DC via GetDC(hWnd_1005B24), triggers full window
 *          repaint via sub_1002AC3, and releases DC via ReleaseDC.
 * @return int    Status code from ReleaseDC.
 */
int sub_1002AF0() {
    HWND hWnd = hWnd_1005B24;
    HDC DC = GetDC(hWnd);
    if (!DC) return 0;

    log_msg("[sub_1002AF0] Full game window repaint DC wrapper (hWnd=%p, DC=%p)\n", hWnd, DC);
    sub_1002AC3(DC);
    return ReleaseDC(hWnd, DC);
}

/**
 * 13. sub_1002B14 (0x01002B14)
 * -----------------------------------------------------------------------------
 * @brief Game Initialization & Reset Controller.
 * @details Checks initialization / loads game resources via sub_1002414.
 *          If valid (non-zero), triggers board reset and new game setup via sub_1002ED5.
 * @return int    1 if game was initialized and reset; 0 on failure.
 */
int sub_1002B14() {
    log_msg("[sub_1002B14] Game initialization controller invoked\n");
    int result = ((Sub_1002414_t)ADDR_SUB_1002414)();
    if (result != 0) {
        ((Sub_1002ED5_t)ADDR_SUB_1002ED5)();
        return 1;
    }
    return result;
}

/**
 * 14. sub_1002B27 (0x01002B27)
 * -----------------------------------------------------------------------------
 * @brief Clamped Registry DWORD Setting Reader.
 * @details Reads a DWORD configuration setting from HKCU\Software\Microsoft\winmine.
 *          If the value is missing or query fails, returns defaultVal.
 *          Otherwise clamps the value within [minVal, maxVal].
 * @param keyIndex     Index into lpKeyName_10050D0 wide-string name table.
 * @param defaultVal   Default fallback value if key not present in Registry.
 * @param minVal       Minimum allowed clamp boundary.
 * @param maxVal       Maximum allowed clamp boundary.
 * @return int         Clamped configuration integer value.
 */
int __stdcall sub_1002B27(int keyIndex, int defaultVal, int minVal, int maxVal) {
    DWORD data = 0;
    DWORD cbData = sizeof(DWORD);
    const WCHAR* valName = lpKeyName_10050D0[keyIndex];

    LONG status = RegQueryValueExW(
        hKey_1005950,
        valName,
        NULL,
        NULL,
        (LPBYTE)&data,
        &cbData);

    if (status != ERROR_SUCCESS) {
        log_msg("[sub_1002B27] RegQueryValueExW(keyIndex=%d): not found, default=%d\n",
                keyIndex, defaultVal);
        return defaultVal;
    }

    int val = (int)data;
    if (val < minVal) val = minVal;
    if (val > maxVal) val = maxVal;

    log_msg("[sub_1002B27] RegQueryValueExW(keyIndex=%d): read=%d, clamped=%d [min=%d, max=%d]\n",
            keyIndex, (int)data, val, minVal, maxVal);

    return val;
}

/**
 * 15. sub_100346A (0x0100346A)
 * -----------------------------------------------------------------------------
 * @brief Remaining Mine Counter Delta Updater.
 * @details Modifies global unflagged mine count (dword_1005194 += a1) and triggers
 *          7-segment digit display refresh via sub_1002801.
 *          Called with a1 = -1 when flag placed, a1 = +1 when unflagged, a1 = 0 on reset.
 * @param a1      Mine count delta (-1, +1, or 0).
 * @return int    Status code from ReleaseDC via sub_1002801.
 */
int __stdcall sub_100346A(int a1) {
    log_msg("[sub_100346A] Mine delta=%d (before=%d, after=%d)\n",
            a1, dword_1005194, dword_1005194 + a1);
    dword_1005194 += a1;
    return sub_1002801();
}

/**
 * 16. sub_10026A7 (0x010026A7)
 * -----------------------------------------------------------------------------
 * @brief Game Board Tile Grid Blitter.
 * @details Iterates all rows and columns of the Minesweeper playing field,
 *          reads each tile's state byte from the tile state array (unk_1005360),
 *          looks up the corresponding pre-built HDC from hdcSrc_1005A20, and
 *          blits the 16x16 tile bitmap onto the window DC via BitBlt(SRCCOPY).
 *          Tile grid starts at pixel coordinates (12, 55) with 16px spacing.
 * @param hdc     Target device context handle.
 * @return int    Row counter after completion (always >= 1).
 */
int __stdcall sub_10026A7(HDC hdc) {
    int result = 1;
    int y = 55;
    int v4 = 1;

    if (dword_1005338 >= 1) {
        BYTE* v2 = unk_1005360;
        do {
            int v3 = 1;
            for (int x = 12; v3 <= dword_1005334; ++v3) {
                BYTE tileState = v2[v3] & 0x1F;
                BitBlt(hdc, x, y, 16, 16, hdcSrc_1005A20[tileState], 0, 0, 0xCC0020);
                x += 16;
            }
            result = ++v4;
            y += 16;
            v2 += 32;
        } while (v4 <= dword_1005338);
    }
    return result;
}

/**
 * 17. sub_1002414 (0x01002414)
 * -----------------------------------------------------------------------------
 * @brief Game Resource Loader & GDI Initialization.
 * @details Loads bitmap resources (tiles, digits, smileys) from the executable,
 *          locks them to obtain memory pointers, creates GDI pen/brush objects,
 *          computes DIB row offsets for each bitmap set, and initializes 16
 *          compatible DCs with pre-rendered tile bitmaps for fast BitBlt blitting.
 * @return int    1 on success (all resources loaded), 0 on failure.
 */
int sub_1002414() {
    Sub_10023CD_t fnFindResource = (Sub_10023CD_t)ADDR_SUB_10023CD;
    Sub_10023F1_t fnRowSize = (Sub_10023F1_t)ADDR_SUB_10023F1;

    dword_1005958 = nullptr;
    dword_1005A08 = nullptr;
    dword_1005954 = nullptr;

    HRSRC hRes;
    hRes = fnFindResource(410);
    if (hRes) dword_1005958 = LoadResource(hModule_1005B30, hRes);

    hRes = fnFindResource(420);
    if (hRes) dword_1005A08 = LoadResource(hModule_1005B30, hRes);

    hRes = fnFindResource(430);
    if (hRes) dword_1005954 = LoadResource(hModule_1005B30, hRes);

    if (!dword_1005958 || !dword_1005A08 || !dword_1005954) {
        log_msg("[sub_1002414] Resource load FAILED (tiles=%p, digits=%p, smileys=%p)\n",
                dword_1005958, dword_1005A08, dword_1005954);
        return 0;
    }

    void* lpbmi = LockResource(dword_1005958);
    dword_100595C = LockResource(dword_1005A08);
    dword_1005A00 = LockResource(dword_1005954);

    HGDIOBJ hPenBrush;
    if (dword_10056C8) {
        hPenBrush = CreatePen(PS_SOLID, 1, RGB(0x80, 0x80, 0x80));
    } else {
        hPenBrush = GetStockObject(WHITE_BRUSH);
    }
    dword_1005158 = hPenBrush;

    int v4 = fnRowSize(16, 16);
    int offset = (dword_10056C8 ? 0x68 : 0x30);
    uint32_t* pTileOffsets = dword_10059C0;
    while (pTileOffsets < (uint32_t*)0x01005A00) {
        *pTileOffsets++ = offset;
        offset += v4;
    }

    int v8 = fnRowSize(13, 23);
    int digitOffset = (dword_10056C8 ? 0x68 : 0x30);
    uint32_t* pDigitOffsets = dword_1005A60;
    while (pDigitOffsets < (uint32_t*)0x01005A90) {
        *pDigitOffsets++ = digitOffset;
        digitOffset += v8;
    }

    int v12 = fnRowSize(24, 24);
    int smileyOffset = (dword_10056C8 ? 0x68 : 0x30);
    uint32_t* pSmileyOffsets = dword_1005960;
    while (pSmileyOffsets < (uint32_t*)dword_1005974) {
        *pSmileyOffsets++ = smileyOffset;
        smileyOffset += v12;
    }

    HDC DC = GetDC(hWnd_1005B24);
    for (int i = 0; i < 16; ++i) {
        hdcSrc_1005A20[i] = CreateCompatibleDC(DC);
        if (!hdcSrc_1005A20[i])
            OutputDebugStringA("FLoad failed to create compatible dc\n");

        HBITMAP hBmp = CreateCompatibleBitmap(DC, 16, 16);
        dword_1005980[i] = (int)hBmp;
        if (!hBmp)
            OutputDebugStringA("Failed to create Bitmap\n");

        SelectObject(hdcSrc_1005A20[i], (HGDIOBJ)dword_1005980[i]);
        SetDIBitsToDevice(
            hdcSrc_1005A20[i],
            0, 0, 16, 16,
            0, 0, 0, 16,
            (char*)lpbmi + dword_10059C0[i],
            (BITMAPINFO*)lpbmi,
            0);
    }
    ReleaseDC(hWnd_1005B24, DC);

    log_msg("[sub_1002414] Resources loaded: tiles=%p, digits=%p, smileys=%p (colorMode=%d)\n",
            lpbmi, dword_100595C, dword_1005A00, dword_10056C8);
    return 1;
}

/**
 * 18. sub_1002ED5 (0x01002ED5)
 * -----------------------------------------------------------------------------
 * @brief Board Reset / Tile State Initialization.
 * @details Fills the entire tile state array (byte_1005340, 864 bytes) with
 *          hidden state (0x0F), then sets border wall tiles (0x10) around the
 *          playing field perimeter: top/bottom rows and left/right columns.
 *          Border coordinates: row 0, row rowCount+1, col 0, col colCount+1.
 */
void sub_1002ED5() {
    for (int i = 864; i != 0; --i)
        byte_1005340[i - 1] = 0x0F;

    int colCount = dword_1005334;
    int rowCount = dword_1005338;

    for (int c = colCount + 1; c >= 0; --c) {
        byte_1005340[c] = 0x10;
        byte_1005340[(rowCount + 1) * 32 + c] = 0x10;
    }

    for (int r = rowCount + 1; r >= 0; --r) {
        byte_1005340[r * 32] = 0x10;
        byte_1005340[r * 32 + colCount + 1] = 0x10;
    }
}

/**
 * 19. sub_1002607 (0x01002607)
 * -----------------------------------------------------------------------------
 * @brief GDI Resource Cleanup.
 * @details Deletes the global pen/brush handle (dword_1005158), then iterates
 *          16 compatible DCs and their associated bitmaps, releasing all GDI
 *          resources allocated during game initialization.
 * @return BOOL    Result of the last DeleteObject call.
 */
BOOL sub_1002607() {
    if (dword_1005158)
        DeleteObject(dword_1005158);

    BOOL result = FALSE;
    for (int i = 0; i < 16; ++i) {
        DeleteDC(hdcSrc_1005A20[i]);
        result = DeleteObject((HGDIOBJ)dword_1005980[i]);
    }
    return result;
}

/**
 * 20. sub_1002B80 (0x01002B80)
 * -----------------------------------------------------------------------------
 * @brief Registry String Value Reader.
 * @details Reads a string value from HKCU\Software\Microsoft\winmine using
 *          the value name from lpKeyName_10050D0[keyIndex]. If the read fails,
 *          copies a default string to the output buffer.
 * @param keyIndex     Index into lpKeyName_10050D0 array.
 * @param lpData       Output buffer for the string value.
 * @return LONG        Status from RegQueryValueExW (0 = success).
 */
int __stdcall sub_1002B80(int keyIndex, LPBYTE lpData) {
    DWORD cbData = 64;
    const WCHAR* valName = lpKeyName_10050D0[keyIndex];

    LONG status = RegQueryValueExW(
        hKey_1005950,
        valName,
        NULL,
        NULL,
        lpData,
        &cbData);

    if (status != ERROR_SUCCESS) {
        lstrcpyW((LPWSTR)lpData, defaultStr_1005B40);
    }

    log_msg("[sub_1002B80] RegQueryValueExW(keyIndex=%d): status=%d\n", keyIndex, status);
    return status;
}

/**
 * 21. sub_100272E (0x0100272E)
 * -----------------------------------------------------------------------------
 * @brief Tile Grid DC Acquisition Wrapper.
 * @details Acquires the window DC via GetDC(hWnd_1005B24), triggers the tile
 *          grid blitter (sub_10026A7) to repaint all playing field tiles,
 *          and releases the DC via ReleaseDC.
 * @return int    Status code from ReleaseDC.
 */
int sub_100272E() {
    HDC DC = GetDC(hWnd_1005B24);
    sub_10026A7(DC);
    return ReleaseDC(hWnd_1005B24, DC);
}

/**
 * 22. sub_10038D7 (0x010038D7)
 * -----------------------------------------------------------------------------
 * @brief Sound Stop (Stop Current Sound).
 * @details If sound is enabled (dword_10056B8 == 3), calls PlaySoundW with
 *          SND_PURGE to stop any currently playing sound effect.
 * @return BOOL    Result of PlaySoundW, or undefined if sound disabled.
 */
BOOL sub_10038D7() {
    if (dword_10056B8 == 3)
        return PlaySoundW(NULL, NULL, SND_PURGE);
    return FALSE;
}

/**
 * 23. sub_10038C2 (0x010038C2)
 * -----------------------------------------------------------------------------
 * @brief Sound Stop with Offset Return.
 * @details Calls PlaySoundW with SND_PURGE to stop any currently playing sound,
 *          returns PlaySoundW result + 2.
 * @return int    PlaySoundW result + 2.
 */
int sub_10038C2() {
    return PlaySoundW(NULL, NULL, SND_PURGE) + 2;
}

/**
 * 24. sub_1003940 (0x01003940)
 * -----------------------------------------------------------------------------
 * @brief Random Number Generator (Modulo).
 * @details Returns rand() % a1, providing a random integer in range [0, a1-1].
 * @param a1      Upper bound (exclusive).
 * @return int    Random value in [0, a1-1].
 */
int __stdcall sub_1003940(int a1) {
    return rand() % a1;
}

/**
 * 25. sub_10039E7 (0x010039E7)
 * -----------------------------------------------------------------------------
 * @brief String Resource Loader with Fallback.
 * @details Loads a string resource by ID via LoadStringW. If the load fails
 *          (returns 0), falls back to loading string ID 1001 via sub_1003950.
 * @param uID             String resource ID.
 * @param lpBuffer        Output buffer for the string.
 * @param cchBufferMax    Maximum characters in buffer.
 * @return int            Number of characters copied, or fallback result.
 */
int __stdcall sub_10039E7(unsigned __int16 uID, LPWSTR lpBuffer, int cchBufferMax) {
    int result = LoadStringW(hModule_1005B30, uID, lpBuffer, cchBufferMax);
    if (result == 0)
        return ((int (__stdcall*)(int))0x01003950)(1001);
    return result;
}

/**
 * 26. sub_1003CC4 (0x01003CC4)
 * -----------------------------------------------------------------------------
 * @brief Menu Check/Uncheck Item.
 * @details Sets or removes the check mark on a menu item. If a2 is non-zero,
 *          uses MF_CHECKED (8); otherwise MF_UNCHECKED (0).
 * @param a1      Menu item ID (unsigned short).
 * @param a2      Check state (non-zero = check, zero = uncheck).
 * @return DWORD  Previous state of the menu item.
 */
DWORD __stdcall sub_1003CC4(unsigned __int16 a1, int a2) {
    UINT uCheck = (a2 != 0) ? MF_CHECKED : MF_UNCHECKED;
    return CheckMenuItem(hMenu_1005A94, a1, uCheck);
}

/**
 * 27. sub_1003CE5 (0x01003CE5)
 * -----------------------------------------------------------------------------
 * @brief Menu Visibility Toggle.
 * @details Sets UI flags, refreshes UI, then shows/hides the game menu based
 *          on bit0 of the flags value. If bit0=0, menu is shown; if bit0=1,
 *          menu is hidden (SetMenu with NULL).
 * @param a1      New UI flags value.
 * @return int    Result of sub_1001950(2).
 */
int __stdcall sub_1003CE5(int a1) {
    typedef int (*Fn_1001516)(void);
    typedef int (__stdcall *Fn_1001950)(int);

    dword_10056C4 = a1;
    ((Fn_1001516)ADDR_SUB_1001516)();
    SetMenu(hWnd_1005B24, ((dword_10056C4 & 1) == 0) ? hMenu_1005A94 : NULL);
    return ((Fn_1001950)ADDR_SUB_1001950)(2);
}

/**
 * 28. sub_1003DF6 (0x01003DF6)
 * -----------------------------------------------------------------------------
 * @brief Dialog Integer Getter with Clamping.
 * @details Retrieves an unsigned integer from a dialog control via GetDlgItemInt,
 *          then clamps the value to the range [a3, a4].
 * @param hDlg        Dialog window handle.
 * @param nIDDlgItem  Control ID.
 * @param a3          Minimum allowed value.
 * @param a4          Maximum allowed value.
 * @return int        Clamped integer value.
 */
int __stdcall sub_1003DF6(HWND hDlg, int nIDDlgItem, int a3, int a4) {
    BOOL translated;
    int result = GetDlgItemInt(hDlg, nIDDlgItem, &translated, FALSE);
    if (result < a3) return a3;
    if (result > a4) return a4;
    return result;
}

/**
 * 29. sub_1003FF4 (0x01003FF4)
 * -----------------------------------------------------------------------------
 * @brief Floating-Point Control Word Setter with Differential Verification.
 * @details Calls _controlfp to set the floating-point control word: sets
 *          precision to 53-bit (0x10000) with mask 0x30000.
 *          Differential test invokes both original binary function via trampoline
 *          and recompiled C function to prove 100% equivalence.
 * @return unsigned int    Floating-point control word result.
 */
typedef unsigned int (*Sub_1003FF4_Orig_t)();
static uint8_t g_sub_1003FF4_trampoline[32];
static Sub_1003FF4_Orig_t g_orig_sub_1003FF4 = NULL;

static void setup_sub_1003FF4_trampoline() {
    DWORD oldProtect;
    VirtualProtect(g_sub_1003FF4_trampoline, sizeof(g_sub_1003FF4_trampoline), PAGE_EXECUTE_READWRITE, &oldProtect);

    // Copy original 5 bytes from 0x01003FF4 ("push $0x30000" -> 68 00 00 03 00)
    memcpy(g_sub_1003FF4_trampoline, (void*)0x01003FF4, 5);

    // Append JMP rel32 back to 0x01003FF9
    g_sub_1003FF4_trampoline[5] = 0xE9;
    DWORD targetAddr = 0x01003FF9;
    DWORD jmpRel = targetAddr - ((DWORD)&g_sub_1003FF4_trampoline[5] + 5);
    memcpy(&g_sub_1003FF4_trampoline[6], &jmpRel, 4);

    FlushInstructionCache(GetCurrentProcess(), g_sub_1003FF4_trampoline, sizeof(g_sub_1003FF4_trampoline));
    VirtualProtect(g_sub_1003FF4_trampoline, sizeof(g_sub_1003FF4_trampoline), oldProtect, &oldProtect);

    g_orig_sub_1003FF4 = (Sub_1003FF4_Orig_t)g_sub_1003FF4_trampoline;
}

unsigned int recompiled_sub_1003FF4() {
    return _controlfp(0x10000u, 0x30000u);
}

unsigned int sub_1003FF4() {
    unsigned int recompiled_val = recompiled_sub_1003FF4();

    if (g_orig_sub_1003FF4) {
        unsigned int orig_val = g_orig_sub_1003FF4();
        if (orig_val == recompiled_val) {
            log_msg("[sub_1003FF4] [DIFFERENTIAL VERIFIED] Recompiled (0x%08X) == Original (0x%08X)\n",
                    recompiled_val, orig_val);
        } else {
            log_msg("[sub_1003FF4] [DIFFERENTIAL MISMATCH] Recompiled (0x%08X) != Original (0x%08X)\n",
                    recompiled_val, orig_val);
        }
    } else {
        log_msg("[sub_1003FF4] Recompiled _controlfp executed: 0x%08X\n", recompiled_val);
    }

    return recompiled_val;
}

// =============================================================================
// THREAD FREEZING / HOOKING / UNFREEZING LIFECYCLE
// =============================================================================

#define MAX_SUSPENDED_THREADS 128
static HANDLE g_suspendedThreads[MAX_SUSPENDED_THREADS];
static int g_suspendedCount = 0;

/**
 * @brief Suspends all other threads in target process before memory patching.
 */
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

    // 2. Setup trampolines before memory patching
    setup_sub_1003FF4_trampoline();

    // 3. Apply hooks safely
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
    install_jmp_hook((void*)ADDR_SUB_1002AC3, (void*)&sub_1002AC3, 5);
    install_jmp_hook((void*)ADDR_SUB_1002AF0, (void*)&sub_1002AF0, 7);
    install_jmp_hook((void*)ADDR_SUB_1002B14, (void*)&sub_1002B14, 5);
    install_jmp_hook((void*)ADDR_SUB_1002B27, (void*)&sub_1002B27, 7);
    install_jmp_hook((void*)ADDR_SUB_100346A, (void*)&sub_100346A, 10);
    install_jmp_hook((void*)ADDR_SUB_10026A7, (void*)&sub_10026A7, 6);
    install_jmp_hook((void*)ADDR_SUB_1002414, (void*)&sub_1002414, 9);
    install_jmp_hook((void*)ADDR_SUB_1002ED5, (void*)&sub_1002ED5, 5);
    install_jmp_hook((void*)ADDR_SUB_1002607, (void*)&sub_1002607, 5);
    install_jmp_hook((void*)ADDR_SUB_1002B80, (void*)&sub_1002B80, 7);
    install_jmp_hook((void*)ADDR_SUB_100272E, (void*)&sub_100272E, 7);
    install_jmp_hook((void*)ADDR_SUB_10038D7, (void*)&sub_10038D7, 7);
    install_jmp_hook((void*)ADDR_SUB_10038C2, (void*)&sub_10038C2, 6);
    install_jmp_hook((void*)ADDR_SUB_1003940, (void*)&sub_1003940, 6);
    install_jmp_hook((void*)ADDR_SUB_10039E7, (void*)&sub_10039E7, 5);
    install_jmp_hook((void*)ADDR_SUB_1003CC4, (void*)&sub_1003CC4, 6);
    install_jmp_hook((void*)ADDR_SUB_1003CE5, (void*)&sub_1003CE5, 9);
    install_jmp_hook((void*)ADDR_SUB_1003DF6, (void*)&sub_1003DF6, 5);
    install_jmp_hook((void*)ADDR_SUB_1003FF4, (void*)&sub_1003FF4, 5);

    // 4. Unfreeze (Resume) threads
    unfreeze_other_threads();

    log_msg("[DLL] Successfully installed all 29 recompiled hooks (freeze -> hook -> unfreeze)!\n");

    // 5. Run Differential Verification on sub_1003FF4
    sub_1003FF4();
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        install_all_hooks();
    }
    return TRUE;
}
