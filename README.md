# Engine Simulator: iOS

<p align="center">
  <img src="https://raw.githubusercontent.com/ange-yaghi/engine-sim/master/resources/logo.png" alt="Engine Simulator" width="700">
</p>

<p align="center">
  <strong>Engine Simulator, brought to iPhone and iPad.</strong>
</p>

<p align="center">
  A native iOS port of Ange Yaghi's Engine Simulator.
</p>

---

## Engine Simulator on iOS

**Engine Simulator: iOS** brings the open-source Engine Simulator experience to Apple devices while staying as faithful as possible to the original simulator.

The goal of this port is simple: **Engine Simulator should feel like Engine Simulator.**

The simulation, engine scripting system, synthesized engine audio, mechanical visualization, gauges, and overall presentation are preserved while the surrounding platform layer has been adapted for iOS.

This project is an **unofficial iOS port** and is not an official release by Ange Yaghi.

---

## Thank You, Ange

Engine Simulator: iOS exists because of the work of **Ange Yaghi (AngeTheGreat)**.

Creating something as unique as Engine Simulator is impressive enough; making the project open source and allowing others to learn from, modify, and bring it to new platforms is what made this port possible.

I've also had the opportunity to speak briefly with Ange about the iOS port, and I appreciate his openness toward the project.

Thank you, Ange, for creating Engine Simulator and making projects like this possible.

<p align="center">
  <a href="https://github.com/ange-yaghi/engine-sim">
    <img src="https://img.shields.io/badge/Original_Project-Engine_Simulator-black?style=for-the-badge&logo=github">
  </a>
</p>

---

## The Simulator

Engine Simulator is a real-time internal combustion engine simulation built around physically modeled engine components.

Rather than relying on prerecorded engine sounds, the simulator generates its audio from the behavior of the simulated engine itself.

<p align="center">
  <img src="https://raw.githubusercontent.com/ange-yaghi/engine-sim/master/resources/screenshot.png" alt="Engine Simulator">
</p>

Engines are defined using Engine Simulator's scripting system, allowing everything from ordinary production engines to unusual and experimental configurations to be simulated.

### From Engine Simulator

- Real-time engine simulation
- Procedurally synthesized engine audio
- Engine Simulator `.mr` scripting
- Cylinder, crankshaft, valvetrain, intake, and exhaust simulation
- Dyno and performance information
- Oscilloscope visualization
- Interactive throttle and simulation controls
- Mechanical engine visualization
- Engine switching
- Custom engine support

---

## Built for iOS

This isn't a streamed desktop application, remote interface, or recreation.

**Engine Simulator runs directly on the device.**

The original application has been adapted around an iOS-native runtime while preserving the simulator itself as closely as practical.

The iOS port includes:

- Native ARM64 support for iPhone and iPad
- Touchscreen controls
- Native iOS application lifecycle handling
- Metal-backed GPU rendering through SDL3
- Native iOS audio output
- High-refresh-rate and ProMotion support
- iOS Files integration
- Custom `.mr` engine importing
- Mobile-friendly engine selection
- Device-aware interface behavior

The result is the original Engine Simulator running as an actual iOS application.

---

## Custom Engines

Engine Simulator: iOS supports Engine Simulator's `.mr` engine scripts.

Custom engines can be imported through iOS and stored directly inside the application's **Custom Engines** directory.

This allows engines created by the Engine Simulator community to be brought directly onto an iPhone or iPad without requiring a separate mobile engine format.

---

## Download

<p align="center">
  <a href="#">
    <img src="https://img.shields.io/badge/Download_on_the-App_Store-000000?style=for-the-badge&logo=apple&logoColor=white">
  </a>
</p>

<p align="center">
  <strong>App Store link coming soon.</strong>
</p>

---

## About the Original Project

Engine Simulator was created by **Ange Yaghi (AngeTheGreat)**.

The original Engine Simulator project, its source code, documentation, and desktop releases can be found here:

<p align="center">
  <a href="https://github.com/ange-yaghi/engine-sim">
    <img src="https://img.shields.io/badge/GitHub-Original_Engine_Simulator-181717?style=for-the-badge&logo=github">
  </a>
</p>

If you're interested in how Engine Simulator works, creating engines, contributing to the original project, or running the desktop version, the original repository is the place to go.

---

## License & Attribution

Engine Simulator: iOS is derived from the open-source **Engine Simulator** project created by Ange Yaghi.

Original Engine Simulator copyright and attribution remain with their respective owners and contributors.

This port does not claim ownership of Engine Simulator itself. The work in this repository primarily concerns adapting, integrating, and maintaining Engine Simulator as a native iOS application.

See the repository's license for the applicable licensing terms.

---

## AI-Assisted Development

AI tools were used during development primarily to improve efficiency while porting, debugging, reviewing, and adapting a large existing C++ codebase for iOS.

AI did not independently create or maintain this project. I directed the port, tested changes on real hardware, diagnosed behavior, made implementation decisions, reviewed generated changes, and integrated the resulting work into the application.

The port has also been tested heavily throughout development, with AI assistance used to accelerate investigation of platform-specific problems and iteration on fixes while keeping the focus on preserving the behavior and character of the original simulator.

---

<p align="center">
  <strong>Engine Simulator: iOS</strong>
  <br>
  Engine Simulator, wherever you take your iPhone.
</p>
