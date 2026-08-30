# Windows Cross-Compilation, DLL Injection & Function Hooking Notes

## Environment

- **Host:** Linux (openSUSE, zypper)
- **Compiler:** MinGW-w64 cross-compiler (`x86_64-w64-mingw32-g++`)
- **Runtime:** Wine (`wine`, `wineserver`)
- **Virtual Display:** `Xvfb` (for GUI testing)

## Tools & Packages

| Tool | Package | Purpose |
|------|---------|---------|
| `x86_64-w64-mingw32-g++` | `mingw64-cross-gcc-c++` | Cross-compile C++ for Windows x86_64 |
| `i686-w64-mingw32-g++` | `mingw32-cross-gcc-c++` | Cross-compile C++ for Windows x86 (32-bit) |
| `wine` | `wine` | Run Windows PE binaries on Linux |
| `wineserver` | `wine` | Manage Wine IPC and process lifecycle |
| `Xvfb` | `Xvfb` | Virtual framebuffer for headless GUI execution |

---

## Architecture & Components

### 1. `hello.cpp` → `hello.exe` (64-bit)
Minimal sanity-check executable.
- Built with `-static` to bundle C/C++ runtimes.
- Run: `wine hello.exe`

### 2. `target.cpp` → `target.exe` (64-bit)
Target CLI worker process with exported functions:
- `calculate_price(item_id, base_price, tax_rate)`: Computes order pricing. Marked `extern "C" __declspec(dllexport) __attribute__((noinline))`.
- `execute_job(job_id, job_name)`: Worker task execution. Marked `extern "C" __declspec(dllexport) __attribute__((noinline))`.
- Continuous CLI loop running tasks and reporting results every second.

### 3. `inject.cpp` → `inject.exe` (64-bit) & `inject32.cpp` → `inject32.exe` (32-bit)
Robust DLL injectors:
- Accepts process name (`target.exe` / `WINMINE.EXE`) or numeric PID (`<PID>`).
- Finds process PID via `CreateToolhelp32Snapshot` / `Process32First` / `Process32Next`.
- Opens process via `OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid)`.
- Resolves full DLL path with `GetFullPathNameA`.
- Allocates target memory via `VirtualAllocEx` and copies path with `WriteProcessMemory`.
- Spawns remote thread calling `kernel32.dll!LoadLibraryA` via `CreateRemoteThread`.
- Verifies remote thread exit code using `GetExitCodeThread` (non-zero `HMODULE` check).

### 4. `mydll.cpp` → `mydll.dll` (64-bit)
Multi-hook instrumentation DLL payload:
- `DllMain` entry declared `extern "C"` to prevent C++ name mangling.
- Resolves host exported functions via `GetModuleHandleA(NULL)` and `GetProcAddress`.
- Installs 64-bit inline prologue hooks (`calculate_price` parameter mutation/profiling, `execute_job` return mutation).

### 5. `winmine_hook.cpp` → `winmine_hook.dll` (32-bit)
Testing wrapper hook for 32-bit Windows XP `WINMINE.EXE`:
- Hooks `sub_100346a` (`0x0100346a`, mine counter update function).
- Reads internal memory counter `0x01005194` before and after execution.
- Calls original `100346a(delta)` cleanly.
- Logs call index, argument delta, and pre/post mine count state.
- Automated runner: `test_winmine_wrapper.sh` (supports `headless` via Xvfb or active `gui` display).



---

## 32-bit vs 64-bit Hooking Mechanics

| Feature | 32-bit (x86) | 64-bit (x86_64) |
|---|---|---|
| **Jump Size** | 5 bytes (`0xE9 [4-byte rel32]`) | 14 bytes (`FF 25 00 00 00 00 [8-byte addr]`) |
| **Relative Address Calculation** | `dest_addr - (source_addr + 5)` | N/A (Absolute 64-bit pointer RIP-relative dereference) |
| **Toolchain** | `i686-w64-mingw32-g++` | `x86_64-w64-mingw32-g++` |

---

## Build System & Automated Testing

### Build (`Makefile`)
```bash
make        # Compiles 64-bit and 32-bit binaries and DLLs
make clean  # Cleans binaries and log files
make test   # Runs 6/6 automated test suite
```

### Test Suite (`test.sh`)
1. **Invalid Arguments**: CLI argument checking.
2. **Missing Process**: Process error handling.
3. **Missing DLL**: DLL loading failure handling.
4. **64-bit Multi-Hook by Name**: Process name injection and parameter/return mutation.
5. **64-bit Multi-Hook by PID**: Numeric PID injection.
6. **32-bit WINMINE.EXE Hook**: Injects into 32-bit GUI Minesweeper under Xvfb, hooks `sub_100347c`, intercepts game event.

---

## Key Troubleshooting & Lessons Learned

| Issue | Root Cause | Solution |
|-------|------------|----------|
| `libstdc++-6.dll not found` | Dynamic CRT linkage across Wine | Use `-static` compiler flag |
| Remote DLL load silent failure | C++ name mangling altered `DllMain` symbol | Add `extern "C"` to `DllMain` definition |
| False positive injection status | `CreateRemoteThread` succeeded but `LoadLibraryA` returned `NULL` | Check thread exit code via `GetExitCodeThread` |
| Inlined target function calls | GCC `-O2` inlined function into loop | Use `__attribute__((noinline))` on hook targets |
| 32-bit vs 64-bit mismatch | 64-bit injector cannot inject 64-bit DLL into 32-bit PE | Compile 32-bit injector & DLL using `i686-w64-mingw32-g++` |
| GUI app crash in headless CI | Minesweeper requires X display | Run virtual framebuffer using `Xvfb :99` |
| Zombie Wine processes | Previous test runs left wine processes alive | Use `wineserver -k` in test suite cleanup traps |




