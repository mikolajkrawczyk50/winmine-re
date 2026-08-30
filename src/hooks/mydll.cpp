#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef int (*CalcPriceFunc_t)(int, int, int);
typedef int (*ExecuteJobFunc_t)(int, const char*);

struct HookEntry {
    void* targetAddr;
    BYTE origBytes[14];
    BYTE patchBytes[14];
};

static HookEntry g_hookCalcPrice;
static HookEntry g_hookExecuteJob;
static LARGE_INTEGER g_freq;
static int g_totalIntercepted = 0;

static void write_log(const char* fmt, ...) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == NULL) return;
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DWORD written;
    WriteFile(hOut, buffer, len, &written, NULL);
}

static void apply_hook(HookEntry* hook, void* targetAddr, void* hookFunc) {
    hook->targetAddr = targetAddr;
    DWORD oldProtect;
    VirtualProtect(hook->targetAddr, 14, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Save original 14 bytes
    memcpy(hook->origBytes, hook->targetAddr, 14);

    // Build 64-bit jump: FF 25 00 00 00 00 [8-byte addr]
    hook->patchBytes[0] = 0xFF;
    hook->patchBytes[1] = 0x25;
    hook->patchBytes[2] = 0x00;
    hook->patchBytes[3] = 0x00;
    hook->patchBytes[4] = 0x00;
    hook->patchBytes[5] = 0x00;
    uint64_t addr = (uint64_t)hookFunc;
    memcpy(&hook->patchBytes[6], &addr, sizeof(addr));

    // Install hook
    memcpy(hook->targetAddr, hook->patchBytes, 14);
    FlushInstructionCache(GetCurrentProcess(), hook->targetAddr, 14);
    VirtualProtect(hook->targetAddr, 14, oldProtect, &oldProtect);
}

static void unhook_temp(HookEntry* hook, DWORD* pOldProtect) {
    VirtualProtect(hook->targetAddr, 14, PAGE_EXECUTE_READWRITE, pOldProtect);
    memcpy(hook->targetAddr, hook->origBytes, 14);
    FlushInstructionCache(GetCurrentProcess(), hook->targetAddr, 14);
}

static void rehook_temp(HookEntry* hook, DWORD oldProtect) {
    memcpy(hook->targetAddr, hook->patchBytes, 14);
    FlushInstructionCache(GetCurrentProcess(), hook->targetAddr, 14);
    VirtualProtect(hook->targetAddr, 14, oldProtect, &oldProtect);
}

// --- Hook 1: calculate_price (Parameter Mutation + High-Res Profiling) ---
int hooked_calculate_price(int item_id, int base_price, int tax_rate) {
    g_totalIntercepted++;
    LARGE_INTEGER t1, t2;
    QueryPerformanceCounter(&t1);

    int discounted_base = base_price * 80 / 100; // 20% discount
    int reduced_tax = 5;                        // Reduced 5% tax

    write_log("[HOOK] calculate_price INTERCEPTED! Mutating base: %d -> %d, tax: %d%% -> %d%%\n",
              base_price, discounted_base, tax_rate, reduced_tax);

    DWORD oldProt;
    unhook_temp(&g_hookCalcPrice, &oldProt);
    int res = ((CalcPriceFunc_t)g_hookCalcPrice.targetAddr)(item_id, discounted_base, reduced_tax);
    rehook_temp(&g_hookCalcPrice, oldProt);

    QueryPerformanceCounter(&t2);
    double elapsed_us = (double)(t2.QuadPart - t1.QuadPart) * 1000000.0 / g_freq.QuadPart;
    write_log("[HOOK] calculate_price completed in %.2f us (total intercepted: %d)\n", elapsed_us, g_totalIntercepted);
    return res;
}

// --- Hook 2: execute_job (Payload Mutation + Return Value Alteration) ---
int hooked_execute_job(int job_id, const char* job_name) {
    g_totalIntercepted++;
    char modified_name[128];
    snprintf(modified_name, sizeof(modified_name), "MUTATED_%s", job_name);

    write_log("[HOOK] execute_job INTERCEPTED! Renaming '%s' -> '%s', ID: %d -> %d\n",
              job_name, modified_name, job_id, job_id + 1000);

    DWORD oldProt;
    unhook_temp(&g_hookExecuteJob, &oldProt);
    int ret = ((ExecuteJobFunc_t)g_hookExecuteJob.targetAddr)(job_id + 1000, modified_name);
    rehook_temp(&g_hookExecuteJob, oldProt);

    int altered_ret = ret + 999;
    write_log("[HOOK] execute_job return value altered: %d -> %d\n", ret, altered_ret);
    return altered_ret;
}

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

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        QueryPerformanceFrequency(&g_freq);

        write_log("[DLL] Attached! Freezing threads and installing multi-function hooks...\n");

        freeze_other_threads();

        HMODULE hExe = GetModuleHandleA(NULL);
        if (hExe) {
            void* pCalc = (void*)GetProcAddress(hExe, "calculate_price");
            if (pCalc) {
                apply_hook(&g_hookCalcPrice, pCalc, (void*)&hooked_calculate_price);
                write_log("[DLL] Hook installed: calculate_price\n");
            }

            void* pExec = (void*)GetProcAddress(hExe, "execute_job");
            if (pExec) {
                apply_hook(&g_hookExecuteJob, pExec, (void*)&hooked_execute_job);
                write_log("[DLL] Hook installed: execute_job\n");
            }
        }

        unfreeze_other_threads();
        write_log("[DLL] Threads unfrozen, hooks live.\n");
    }
    return TRUE;
}
