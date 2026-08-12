# Portable CMake-preset wrappers for Open Engine Simulator.
#
# Select one of: macos-arm64, linux-x86_64, windows-x86_64.
PLATFORM ?= macos-arm64
CMAKE_ARGS ?=

CORE_PRESET := $(PLATFORM)
DESKTOP_PRESET := $(PLATFORM)-desktop
CI_PRESET := $(PLATFORM)-ci

ifeq ($(PLATFORM),windows-x86_64)
DESKTOP_EXECUTABLE := build/$(DESKTOP_PRESET)/engine-sim-desktop.exe
else
DESKTOP_EXECUTABLE := build/$(DESKTOP_PRESET)/engine-sim-desktop
endif

.DEFAULT_GOAL := help
.PHONY: help portable-configure portable-build portable-run portable-test portable-validate export-meshes

help:
	@echo "make PLATFORM=<target> portable-build  Configure and build the desktop host"
	@echo "make PLATFORM=<target> portable-run    Configure, build, and run the desktop host"
	@echo "make PLATFORM=<target> portable-test   Configure, build, and test the portable core"
	@echo "make PLATFORM=<target> portable-validate Run the CI-equivalent core and SDL audio tests"
	@echo "make export-meshes                     Export authored Blender meshes"

portable-configure:
	cmake --preset $(DESKTOP_PRESET) $(CMAKE_ARGS)

portable-build: portable-configure
	cmake --build --preset $(DESKTOP_PRESET)

portable-run: portable-build
	$(DESKTOP_EXECUTABLE)

portable-test:
	cmake --preset $(CORE_PRESET) $(CMAKE_ARGS)
	cmake --build --preset $(CORE_PRESET)
	ctest --preset $(CORE_PRESET)

portable-validate:
	cmake --preset $(CI_PRESET) $(CMAKE_ARGS)
	cmake --build --preset $(CI_PRESET)
	ctest --preset $(CI_PRESET) --exclude-regex SdlAudioOutput
	ctest --preset $(CI_PRESET) --tests-regex SdlAudioOutput

export-meshes:
	uv run --with bpy --python 3.13 tools/export_blender_meshes.py
