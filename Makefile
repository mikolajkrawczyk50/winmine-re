CXX64 = x86_64-w64-mingw32-g++
CXX32 = i686-w64-mingw32-g++
CXXFLAGS = -static -O2

BIN_DIR = bin
SRC_DIR = src

TARGETS = $(BIN_DIR)/hello.exe \
          $(BIN_DIR)/target.exe \
          $(BIN_DIR)/inject.exe \
          $(BIN_DIR)/inject32.exe \
          $(BIN_DIR)/mydll.dll \
          $(BIN_DIR)/winmine_hook.dll

ROOT_BINARIES = hello.exe target.exe inject.exe inject32.exe mydll.dll winmine_hook.dll

all: $(BIN_DIR) $(TARGETS) $(ROOT_BINARIES)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# 64-bit Test Targets
$(BIN_DIR)/hello.exe: $(SRC_DIR)/targets/hello.cpp
	$(CXX64) $(CXXFLAGS) -o $@ $<

$(BIN_DIR)/target.exe: $(SRC_DIR)/targets/target.cpp
	$(CXX64) $(CXXFLAGS) -o $@ $< -lkernel32

# 64-bit Injector & DLL
$(BIN_DIR)/inject.exe: $(SRC_DIR)/injector/inject.cpp
	$(CXX64) $(CXXFLAGS) -o $@ $< -ladvapi32

$(BIN_DIR)/mydll.dll: $(SRC_DIR)/hooks/mydll.cpp
	$(CXX64) -shared $(CXXFLAGS) -o $@ $<

# 32-bit Injector & Recompiled Minesweeper Hook DLL
$(BIN_DIR)/inject32.exe: $(SRC_DIR)/injector/inject32.cpp
	$(CXX32) $(CXXFLAGS) -o $@ $< -ladvapi32

$(BIN_DIR)/winmine_hook.dll: $(SRC_DIR)/hooks/winmine_hook.cpp
	$(CXX32) -shared $(CXXFLAGS) -o $@ $< -lgdi32 -ladvapi32 -lwinmm

# Root binary synchronization for convenient execution
hello.exe: $(BIN_DIR)/hello.exe
	cp $< $@

target.exe: $(BIN_DIR)/target.exe
	cp $< $@

inject.exe: $(BIN_DIR)/inject.exe
	cp $< $@

inject32.exe: $(BIN_DIR)/inject32.exe
	cp $< $@

mydll.dll: $(BIN_DIR)/mydll.dll
	cp $< $@

winmine_hook.dll: $(BIN_DIR)/winmine_hook.dll
	cp $< $@

test: all
	./scripts/test.sh

run: all
	./scripts/run_winmine.sh

clean:
	rm -rf $(BIN_DIR) $(ROOT_BINARIES) *.log send_click.exe

.PHONY: all test run clean
