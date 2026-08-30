CXX = x86_64-w64-mingw32-g++
CXX32 = i686-w64-mingw32-g++
CXXFLAGS = -static -O2

all: hello.exe mydll.dll inject.exe target.exe inject32.exe winmine_hook.dll

hello.exe: hello.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

mydll.dll: mydll.cpp
	$(CXX) -shared $(CXXFLAGS) -o $@ $<

inject.exe: inject.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< -ladvapi32

target.exe: target.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< -lkernel32

inject32.exe: inject32.cpp
	$(CXX32) $(CXXFLAGS) -o $@ $< -ladvapi32

winmine_hook.dll: winmine_hook.cpp
	$(CXX32) -shared $(CXXFLAGS) -o $@ $< -lgdi32

test: all
	./test.sh

clean:
	rm -f hello.exe mydll.dll inject.exe target.exe inject32.exe winmine_hook.dll winmine_hook.log

.PHONY: all test clean
