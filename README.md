# FMSynthEnsembleV3

[![Build](https://github.com/46nori/FMSynthEnsembleV3/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/46nori/FMSynthEnsembleV3/actions/workflows/build.yml)

**[日本語版はこちら](README_ja.md)**

![](./doc/image/FMSynthEnsembleV3.jpeg)

## Features

A USB MIDI synthesizer using YAMAHA OPN-series FM sound chips.

- **FM sound chips**
  - YAMAHA **YM2608 (OPNA) / YM2203 (OPN) / YMF288 (OPN3-L)**
    - [Up to four (mixed, auto-detected)](doc/spec_fm_chip.md)
  - CSM speech synthesis (YM2608 / YM2203)
- **MIDI**
  - [MIDI implementation chart](./doc/midi_implementation_chart.md)
  - **16 channels**, multi-timbral, per-channel ON/OFF
  - **Up to 24 simultaneous voices**
- **System controller**
  - **Raspberry Pi Pico** (RP2040 / RP2350A)
  - High-speed FM bus via Programmable I/O (PIO)
  - Multicore with FreeRTOS
- **Audio**
  - LINE OUT
  - LINE IN (for ADPCM, with anti-aliasing filter)
  - Electronic volume / LPF audio mixer
- **Modular hardware**

## Documentation

- [Document index](./doc/README.md)
- [Build and flashing](doc/build.md)
- [Schematics](./doc/schematics/README.md)

## Quick Start

VS Code with the Raspberry Pi Pico extension is recommended (macOS / Windows / Linux). Full steps: [Build and flashing](doc/build.md).

1. Clone the repo and run `git submodule update --init --recursive`
2. Open in VS Code, then Pico view → `Configure CMake` → `Compile Project`
3. Connect the Pico in `BOOTSEL` mode and copy `build/FMSynthEnsembleV3.uf2` to `RPI-RP2`

Default board is **Pico 2 (RP2350A)**. CLI, board switching, debugger, and CI: [doc/build.md](doc/build.md).

## Gallery

### Modules

<table>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/YM2608_Module.jpeg"><img src="doc/image/YM2608_Module.jpeg" width="280" alt="YM2608 module"></a><br>
      <b>YM2608 module</b><br>
      <sub>YM2608B + YM3016<br>MIDI panel connector</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/YM2203_Module.jpeg"><img src="doc/image/YM2203_Module.jpeg" width="280" alt="YM2203 module"></a><br>
      <b>YM2203 module</b><br>
      <sub>YM2203 + YM3014B<br>MIDI panel connector</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/YMF288_Module.jpeg"><img src="doc/image/YMF288_Module.jpeg" width="280" alt="YMF288 module"></a><br>
      <b>YMF288 module</b><br>
      <sub>YMF288 + BU9480F</sub>
    </td>
  </tr>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/Main_Module.jpeg"><img src="doc/image/Main_Module.jpeg" width="280" alt="Controller module"></a><br>
      <b>Controller module</b><br>
      <sub>Raspberry Pi Pico 2<br>FM sound module connector (rear)</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/AudioMixer_Module.jpeg"><img src="doc/image/AudioMixer_Module.jpeg" width="280" alt="Mixer module"></a><br>
      <b>Mixer module</b><br>
      <sub>Mixes FM sound module outputs</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/Power_Module.jpeg"><img src="doc/image/Power_Module.jpeg" width="280" alt="Power module"></a><br>
      <b>Power module</b><br>
      <sub>+6V, 2A input<br>+5V / ±12V output</sub>
    </td>
  </tr>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/MIDIPanel.jpeg"><img src="doc/image/MIDIPanel.jpeg" width="280" alt="MIDI panel"></a><br>
      <b>MIDI panel module</b><br>
      <sub>16-ch ON/OFF + LED<br>Connects to YM2608/YM2203 module</sub>
    </td>
    <td></td>
    <td></td>
  </tr>
</table>

### Module connection

<table>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/StackedModules.jpeg"><img src="doc/image/StackedModules.jpeg" width="280" alt="Stack"></a><br>
      <b>Stack</b><br>
      <sub>Up to four FM sound modules<br></sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/BackPlane.jpeg"><img src="doc/image/BackPlane.jpeg" width="280" alt="Dock connection"></a><br>
      <b>Dock connection</b><br>
      <sub>Rear of the controller module</sub>
    </td>
  </tr>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/ConnectedModules.jpeg"><img src="doc/image/ConnectedModules.jpeg" width="280" alt="Full assembly"></a><br>
      <b>Full assembly</b><br>
      <sub>Power, controller,<br>FM sound, mixer</sub>
    </td>
  </tr>
</table>
