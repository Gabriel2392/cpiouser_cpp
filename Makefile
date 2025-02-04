CXX = clang++
CXXFLAGS = -std=c++23 -O3 -ffast-math

all: cpio_extract cpio_build

cpio_extract: cpio_extract.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

cpio_build: cpio_build.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f cpio_extract cpio_build

.PHONY: all clean
