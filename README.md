# Minesweeper Binary Recompilation & Hot-Patching Project

A modular framework for incrementally decompiling, recompiling, hot-patching, and testing Windows binaries (`WINMINE.EXE`) using native C/C++ MinGW toolchains.

---

## 📁 Repository Structure

```
playground/
├── src/
│   ├── injector/
│   │   ├── inject.cpp          # 64-bit remote thread injector
│   │   └── inject32.cpp        # 32-bit remote thread injector (WINMINE target)
│   ├── hooks/
│   │   ├── mydll.cpp           # 64-bit multi-hook demonstration DLL
│   │   └── winmine_hook.cpp    # 32-bit recompiled Minesweeper hook DLL (15 routines)
│   └── targets/
│       ├── hello.cpp           # Basic 64-bit testing target
│       └── target.cpp          # Multi-function 64-bit testing target
├── bin/                        # Output directory for compiled PE32/PE64 binaries
├── docs/
│   ├── NOTES.md                # Architecture notes & routine reverse engineering
│   ├── TUTORIAL.md             # Integration tutorial for automated agents
│   └── RECOMPILATION_PIPELINE.md # In-depth recompilation & memory mapping guide
├── resources/
│   ├── WINMINE.EXE             # Target Windows XP Minesweeper binary
│   ├── subroutine_list.txt     # Complete 85-subroutine decompilation catalog
│   ├── decompile.txt           # Reference decompiled C routines
│   ├── Minesweeper-Windows-XP.zip # Original asset package
│   └── Minesweeper-Windows-XP/ # Unpacked game assets (.EXE, .HLP, .CHM)
├── scripts/
│   ├── test.sh                 # Automated 6-stage test suite
│   ├── run_winmine.sh          # Interactive launcher with live log streaming
│   └── test_winmine_wrapper.sh # Headless wrapper verification
├── Makefile                    # Multi-arch master build configuration
└── .gitignore                  # Git ignore rules for build artifacts
```

---

## 🚀 Quick Start

### Build All Binaries
```bash
make all
```

### Run Automated Test Suite (6/6 Tests)
```bash
make test
# or
./scripts/test.sh
```

### Run Hooked Game Interactively
```bash
make run
# or
./scripts/run_winmine.sh
```

---

## 🛠️ Recompiled Minesweeper Subroutines (15/85)

| Function | Address | Subsystem | Description |
|---|---|---|---|
| `sub_1002752` | `0x01002752` | Digit Renderer | 13x23 7-segment digital glyph blitter via `SetDIBitsToDevice` |
| `sub_1002785` | `0x01002785` | Mine Counter | Mine counter digits formatter (hundreds/tens/ones/minus) |
| `sub_1002801` | `0x01002801` | Mine Counter | Mine counter DC acquisition and refresh wrapper |
| `draw_number_sub_1002825` | `0x01002825` | Timer | Elapsed seconds timer 3-digit formatter and drawer |
| `redraw_number_sub_10028B5` | `0x010028B5` | Timer | Timer DC acquisition and refresh wrapper |
| `sub_10028D9` | `0x010028D9` | Smiley Face | 24x24 smiley face button icon renderer (5 states) |
| `sub_1002913` | `0x01002913` | Smiley Face | Smiley face DC acquisition and refresh wrapper |
| `set_draw_mode_sub_100293D` | `0x0100293D` | GDI Engine | Raster operation (ROP2) and brush/pen selector |
| `sub_1002971` | `0x01002971` | Bevel Engine | 3D beveled rectangle border drawing engine (`MoveToEx`/`LineTo`) |
| `sub_1002A22` | `0x01002A22` | UI Layout | Main window frames layout orchestrator (6 frames) |
| `sub_1002AC3` | `0x01002AC3` | Master Paint | Master game window repaint dispatcher |
| `sub_1002AF0` | `0x01002AF0` | Master Paint | Master repaint DC acquisition wrapper |
| `sub_1002B14` | `0x01002B14` | Game Controller | Game initialization & reset controller |
| `sub_1002B27` | `0x01002B27` | Settings | Clamped registry DWORD setting reader (`RegQueryValueExW`) |
| `sub_100346A` | `0x0100346A` | Mine Counter | Remaining mine counter delta updater (flag/unflag) |
