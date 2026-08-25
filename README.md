# Engine Simulator for iOS

A native iOS port of **Engine Simulator**, bringing AngeTheGreat's real-time internal combustion engine simulation to iPhone and iPad.

This project ports the existing Engine Simulator / Open Engine Simulator codebase to iOS while preserving the original simulation, engine scripting system, rendering, and real-time synthesized engine audio.

> **Status:** Active development — the simulator is running natively on iOS, with remaining work focused primarily on stability, performance, audio behavior, and mobile polish.

## About

[Engine Simulator](https://github.com/ange-yaghi/engine-sim), created by [Ange Yaghi (AngeTheGreat)](https://github.com/ange-yaghi), is a real-time internal combustion engine simulator designed primarily to simulate engine response and generate engine audio.

Unlike a conventional game that relies on prerecorded engine sounds, Engine Simulator models the behavior of an engine and uses the simulation to synthesize its sound in real time.

**Engine Simulator for iOS** brings that experience to Apple's mobile platforms.

This is not a recreation, soundboard, or simplified mobile version of Engine Simulator. The goal of this project is to run the actual simulator on iOS while making the platform-specific changes necessary for touchscreen input, mobile graphics, audio output, application lifecycle management, and performance.

## Features

The iOS port currently supports the major systems of Engine Simulator, including:

- Real-time internal combustion engine simulation
- Procedural / synthesized engine audio
- Engine scripting and configuration
- Multiple engine configurations
- Piston, connecting rod, crankshaft, and valvetrain simulation
- Intake and exhaust simulation
- Ignition and combustion
- Starter motor
- Throttle control
- Dynamometer controls
- Transmission and gearing
- Real-time RPM and engine information
- Engine visualization
- Touchscreen controls
- Native iOS audio output
- Native iOS application packaging
- iPhone display and safe-area handling
- Runtime engine selection

The simulator remains extremely lightweight because much of the experience is generated at runtime rather than stored as large prerecorded audio, texture, or model assets.

## iOS Port

Engine Simulator was originally designed as a desktop application.

Running it properly on iOS requires more than simply compiling the existing source for ARM64. This repository contains the platform work required to make the simulator behave as an iOS application.

That work includes areas such as:

- iOS/ARM64 compilation
- SDL3 integration on iOS
- Touch input
- Mobile UI adaptations
- iOS audio-device integration
- Audio buffering and synchronization
- Application lifecycle handling
- Resource and asset packaging
- iPhone display scaling
- Safe-area support
- Engine selection UI
- Simulator teardown and reinitialization
- Mobile performance safeguards
- GitHub Actions based iOS build pipeline
- IPA packaging and signing support

Where possible, the actual engine simulation is kept consistent with upstream behavior.

Platform-specific changes should solve iOS compatibility or performance problems without unnecessarily changing the underlying physics.

## Current Status

The port is functional and Engine Simulator runs natively on iOS.

The project has moved beyond initial bring-up and is currently primarily in the **stabilization and optimization phase**.

Core functionality is present, while remaining development is focused on issues such as:

- Engine switching reliability
- Audio stability
- Low-RPM / idle behavior
- Performance of computationally expensive engine scripts
- Simulator lifecycle cleanup
- Background / foreground handling
- Mobile UI polish
- Device-specific performance
- General bug fixing

Some complex community engine configurations can place significantly more load on the simulator than others. Mobile-specific safeguards are being developed where necessary while attempting to preserve the behavior of the desktop simulator.

## Controls

The iOS version replaces or supplements desktop keyboard controls with touchscreen interaction.

Current touch controls include support for:

- Ignition
- Starter
- Throttle
- Dynamometer
- RPM hold
- Gear up/down
- Engine selection
- Simulator UI interaction

The goal is to retain the information and functionality of the original Engine Simulator interface while making it practical to operate entirely from a touchscreen.

The mobile control interface is still being refined.

## Engine Scripts

Engine Simulator uses `.mr` scripts to define engines and related simulation objects.

The iOS port retains this scripting architecture.

Engine configurations can describe properties including:

- Cylinder count and layout
- Bore and stroke
- Connecting rod geometry
- Crankshaft geometry
- Compression
- Cylinder heads
- Camshafts
- Valve timing
- Intake systems
- Exhaust systems
- Ignition timing
- Fuel properties
- Starter characteristics
- Simulation parameters

Compatibility with existing Engine Simulator scripts is an important goal of this project.

Some unusually demanding scripts may require mobile-specific limits to maintain real-time simulation on iOS hardware.

## Performance

Engine Simulator performs a significant amount of simulation and audio processing in real time.

Desktop engine configurations can therefore vary dramatically in computational cost.

The iOS port attempts to preserve simulation fidelity while preventing pathological settings from overwhelming mobile hardware.

Performance depends on factors including:

- Engine complexity
- Cylinder count
- Simulation frequency
- Fluid simulation steps
- Audio processing
- Device generation
- Thermal conditions

Modern iPhones are capable of running many Engine Simulator configurations at high frame rates, but optimization work is ongoing for particularly expensive engines.

## Audio

Engine audio is generated from the running simulation rather than being based primarily on prerecorded engine loops.

The audio pipeline includes simulation output, synthesis, filtering, convolution, buffering, and final playback through the iOS audio device.

Maintaining synchronization between a computationally expensive physics simulation and the mobile audio device is one of the more important platform-specific areas of the port.

Audio behavior is currently under active development.

## Building

### Requirements

The project targets iOS and ARM64.

The repository contains an automated GitHub Actions build pipeline so development builds can be produced without requiring the repository itself to be built on a local Mac.

Depending on the build method, you may need:

- An Apple signing certificate
- A provisioning profile
- A compatible iOS device
- GitHub Actions

For conventional local Apple development, Xcode and the appropriate Apple SDK/toolchain may also be used.

### Clone

Clone the repository and its required submodules:

```sh
git clone --recurse-submodules https://github.com/NyerahMT/engine-sim-ios.git
cd engine-sim-ios
```

If the repository has already been cloned without its submodules:

```sh
git submodule update --init --recursive
```

### GitHub Actions

Development of this port uses GitHub Actions to provide a remote iOS build environment.

The workflow handles the iOS build process and can produce artifacts for installation or further packaging.

Check the repository's **Actions** tab for the current workflow and build status.

Signing requirements depend on how the resulting application is intended to be installed.

## Installation

There is currently no official App Store release.

Development builds may require sideloading and valid code signing before they can be installed on an iPhone or iPad.

Installation methods and availability may change as the project approaches a stable release.

Do not assume that an arbitrary IPA from this repository will install without appropriate signing for the target device.

## Project Structure

The project retains much of the architecture of Engine Simulator while adding the components necessary to host it on iOS.

Major areas include:

```text
assets/          Engine scripts, simulator resources, and other assets
include/         C++ headers
src/             Core simulator and platform implementation
.github/         GitHub Actions workflows and automation
```

The simulator itself remains predominantly native C++.

Platform-specific code is kept separate where practical so that iOS support does not unnecessarily diverge from the underlying simulator.

## Development Philosophy

The primary objective of this port is:

> **Make Engine Simulator run properly on iOS without turning it into a different simulator.**

When an iOS-specific problem appears, preference is given to adapting the platform layer before modifying simulation behavior.

Changes to physics or engine behavior should only be made when necessary and should remain as close as practical to upstream Engine Simulator behavior.

This makes it easier to:

- Maintain engine-script compatibility
- Compare behavior with desktop builds
- Merge useful upstream changes
- Diagnose platform-specific bugs
- Avoid silently changing engine behavior
- Keep the iOS port maintainable

## Not an Engineering Tool

Engine Simulator is an educational and entertainment-oriented simulation.

It should **not** be treated as an engineering validation, calibration, engine tuning, or design tool.

Simulation results should not be assumed to accurately predict the performance, durability, emissions, safety, or behavior of a real engine.

## Relationship to Upstream

This repository exists because of the work of the original Engine Simulator project and the subsequent cross-platform/community development around it.

### Original Engine Simulator

Engine Simulator was created by **Ange Yaghi / AngeTheGreat**.

Original project:

https://github.com/ange-yaghi/engine-sim

The original simulation architecture, engine scripting system, visual design, engine configurations, audio concepts, and substantial portions of the underlying source originate from that project.

### Open Engine Simulator

This iOS port also builds upon work performed by the **Open Engine Simulator** community fork, particularly its cross-platform and SDL-related development.

This repository should therefore be considered a downstream iOS-focused port rather than an independent implementation of Engine Simulator.

## Attribution

Credit for the original Engine Simulator belongs to:

**Ange Yaghi (AngeTheGreat)**

https://github.com/ange-yaghi

and to the contributors whose work is present in the upstream and community projects.

The iOS-specific work in this repository focuses on adapting that software to Apple's mobile platform.

Please preserve upstream copyright and attribution notices when redistributing or modifying this project.

## Disclaimer

This is an **unofficial community port**.

It is not an official iOS release from Ange Yaghi and should not be represented as one.

The existence of this project does not imply endorsement, support, or affiliation with Ange Yaghi, Apple, or any other upstream contributor or organization.

"Engine Simulator" is used here to identify the software being ported.

## License

This project preserves the licensing requirements of the upstream software.

See:

[LICENSE](LICENSE)

for the license text and applicable copyright notices.

Third-party libraries, assets, engine scripts, and other dependencies may contain their own copyright and licensing requirements.

Anyone redistributing builds of this project is responsible for complying with the licenses of all included components.

## Contributing

Contributions, bug reports, performance investigations, and iOS-specific improvements are welcome.

Useful areas for contribution include:

- iOS lifecycle behavior
- Audio stability
- Performance optimization
- Touchscreen UI
- Engine-script compatibility
- Memory management
- Simulator teardown/reload reliability
- iPad support
- Documentation
- Build automation

When changing simulation behavior, please document why the divergence from upstream is necessary.

Platform fixes that preserve upstream physics are preferred.

## Bug Reports

When reporting a bug, include as much of the following as possible:

- iPhone or iPad model
- iOS version
- Engine configuration being used
- Steps to reproduce
- Whether the problem occurs immediately or after switching engines
- Approximate FPS
- Screen recording, if relevant
- Crash log, if a crash occurred
- Build/commit where the issue occurred

Performance and audio problems are particularly useful when accompanied by a screen recording showing the simulator's RPM/FPS displays.

## Roadmap

Current priorities are centered around stabilization rather than major feature expansion.

Near-term goals include:

- Stable engine switching
- Reliable simulator teardown/reload
- Consistent audio at idle and under load
- Better handling of demanding engine scripts
- Robust iOS application lifecycle behavior
- Improved touchscreen controls
- UI polish
- Performance tuning
- Release packaging
- Broader device compatibility

Once those areas are stable, development can focus more heavily on additional iOS-specific quality-of-life features.

---

### Credits

**Engine Simulator** — Ange Yaghi / AngeTheGreat  
**Open Engine Simulator contributors** — cross-platform/community development  
**Engine Simulator for iOS** — iOS port and mobile integration

This project would not exist without the original Engine Simulator and the work of its contributors.
