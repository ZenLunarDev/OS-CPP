# MeowOS - wrapper around the CMake build (CMake is the source of truth)
BUILD_DIR = build

.PHONY: all configure build clean run test

all: build

configure:
	cmake -B $(BUILD_DIR) -G "MinGW Makefiles"

build: configure
	cmake --build $(BUILD_DIR)

clean:
	cmake --build $(BUILD_DIR) --target clean

# Run in QEMU (VGA display + real keyboard)
run: build
	qemu-system-i386 -kernel $(BUILD_DIR)/kernel.bin -m 64M

# Automated smoke test (headless + serial log)
test: build
	python tools/smoke_test.py
