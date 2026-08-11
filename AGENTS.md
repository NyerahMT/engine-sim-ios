# Open Engine Simulator — agent guide

- Build with CMake presets; do not add platform-specific project build files.
  `engine-sim-core` and scripting must remain free of SDL, GPU, and OS-device
  types. SDL3 belongs only in the desktop host/adapters.
- Keep runtime assets self-contained under `assets/`. Piranha standard scripts
  live in `assets/es`; authored meshes are exported from `art/assets.blend` to
  `assets/authored_meshes.obj` with `make export-meshes`.
- Preserve the audio boundary: simulation/synthesis produces PCM without
  owning SDL or an audio device. Never allocate, log, perform I/O, or take a
  blocking lock in real-time audio code.
- Validate proportional to the change: `make PLATFORM=<target> portable-test`
  for core changes; rebuild the desktop target for host/render/audio changes.
  Run the script compile and SDL dummy-audio tests when touching their paths.
- Keep public wording accurate: this is an independent, community-driven fork
  of AngeTheGreat's Engine Simulator. Preserve upstream attribution and MIT
  notices.
