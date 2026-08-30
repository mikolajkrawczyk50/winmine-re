#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

DWORD findProcess(const char* name) {
    DWORD pid = (DWORD)atoi(name);
    if (pid != 0) return pid;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe = { sizeof(pe) };
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: inject32.exe <process_name|PID> <dll_path>\n");
        return 1;
    }

    DWORD pid = findProcess(argv[1]);
    if (!pid) {
        printf("Process not found: %s\n", argv[1]);
        return 1;
    }

    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) {
        printf("OpenProcess failed (PID: %lu, error: %lu)\n", pid, GetLastError());
        return 1;
    }

    char fullPath[MAX_PATH];
    GetFullPathNameA(argv[2], MAX_PATH, fullPath, NULL);

    void* mem = VirtualAllocEx(proc, NULL, strlen(fullPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) {
        printf("VirtualAllocEx failed (error: %lu)\n", GetLastError());
        CloseHandle(proc);
        return 1;
    }

    WriteProcessMemory(proc, mem, fullPath, strlen(fullPath) + 1, NULL);

    LPTHREAD_START_ROUTINE pfnLoadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE thread = CreateRemoteThread(proc, NULL, 0, pfnLoadLib, mem, 0, NULL);

    if (thread) {
        WaitForSingleObject(thread, 5000);
        DWORD exitCode = 0;
        GetExitCodeThread(thread, &exitCode);
        CloseHandle(thread);
        if (exitCode != 0) {
            printf("DLL injected successfully into PID %lu (HMODULE: 0x%08lx)!\n", pid, exitCode);
        } else {
            printf("LoadLibraryA returned NULL in target process\n");
        }
    } else {
        printf("CreateRemoteThread failed (error: %lu)\n", GetLastError());
    }

    VirtualFreeEx(proc, mem, 0, MEM_RELEASE);
    CloseHandle(proc);
    return 0;
}
