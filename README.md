# FMSynthEnsembleV3

[![Build](https://github.com/46nori/FMSynthEnsembleV3/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/46nori/FMSynthEnsembleV3/actions/workflows/build.yml)

**[日本語版はこちら](README_ja.md)**

![](./doc/image/YM2608.jpeg)

A USB MIDI synthesizer using YAMAHA FM sound LSIs.

- System controller: **Raspberry Pi Pico** (RP2040 / RP2350A)
- FM sound LSIs: up to four **YM2608 (OPNA)** or **YM2203 (OPN)** chips (mixed configurations supported)
- MIDI: 16 channels, multi-timbral polyphonic playback with up to 24 voices
  - With four YM2608 chips. YM2203 provides 3 FM voices per chip
- Supports CSM (Composite Sinusoidal Modeling) speech synthesis

See [doc/README.md](doc/README.md) for design documentation and schematics.

## Quick Start

We recommend building with VS Code and the Raspberry Pi Pico extension.  
The extension automatically provides pico-sdk, the toolchain, CMake, and Ninja, so you do not need to install them separately. It is available on **macOS, Windows, and Linux**.

### 1. Setup (first time only)

1. Install [VS Code](https://code.visualstudio.com/)
2. Install the official Raspberry Pi extension **Raspberry Pi Pico** (ID: `raspberry-pi.raspberry-pi-pico`) in VS Code, then restart VS Code
3. Clone the repository and initialize submodules

   ```bash
   git clone <this-repository>
   cd FMSynthEnsembleV3
   git submodule update --init --recursive
   ```

   On Windows, run this in Git Bash from Git for Windows. On macOS and Linux, use the system terminal.

4. Open this folder in VS Code and run `Configure CMake` from the Raspberry Pi Pico view (Quick Access) in the sidebar

### 2. Build

| OS | Build | Command Palette |
|---|---|---|
| macOS | `Cmd+Shift+B` | `Cmd+Shift+P` |
| Windows / Linux | `Ctrl+Shift+B` | `Ctrl+Shift+P` |

Run `Compile Project`, or use `Compile` from Quick Access.  
On success, `build/FMSynthEnsembleV3.uf2` is generated.

### 3. Flash firmware

1. Hold the Raspberry Pi Pico `BOOTSEL` button while connecting it to your PC via USB (it appears as the `RPI-RP2` mass-storage device)
2. Copy `build/FMSynthEnsembleV3.uf2` to `RPI-RP2`

   | OS | Typical destination |
   |---|---|
   | macOS | The `RPI-RP2` volume in Finder, or `/Volumes/RPI-RP2/` |
   | Windows | The `RPI-RP2` drive in Explorer (e.g. `D:\`) |
   | Linux | The mount point in your file manager (e.g. `/media/<user>/RPI-RP2` or `/run/media/<user>/RPI-RP2`) |

3. After the copy completes, the board reboots automatically and runs the new firmware

That is all. The device appears on your PC as a USB MIDI device.

> The default board is **Raspberry Pi Pico 2 (RP2350A)**. To use Pico (RP2040), see [Switching Boards](#switching-boards).

## Building from the Command Line

If you have pico-sdk 2.2.0, ARM GCC 14.2, CMake 3.13+, and Ninja installed manually, you can build without VS Code.

| OS | Notes |
|---|---|
| macOS | Install the toolchain with Homebrew, etc., or point `PICO_SDK_PATH` and related variables at the SDK/toolchain installed by the Pico extension |
| Linux | Use your distribution packages, or follow the [pico C SDK instructions](https://www.raspberrypi.com/documentation/pico-sdk/) |
| Windows | Run the commands below in **WSL2** or **MSYS2 / Git Bash**. Native Windows shells make ARM GCC setup cumbersome, so WSL2 is recommended for CLI builds |

```bash
git submodule update --init --recursive

# Configure (uses the "default" preset in CMakePresets.json)
cmake --preset default

# Build
ninja -C build
```

Configure also generates `build/compile_commands.json`. clangd reads it through `.clangd` at the repository root to resolve include paths for pico-sdk and ARM GCC.

## CI / CD

Push and pull requests to `main` run [`.github/workflows/build.yml`](.github/workflows/build.yml). It builds `PICO_BOARD=pico2` and `PICO_BOARD=pico` with the default CMake options, and uploads firmware artifacts for each.

## Using a Debugger

With a [Raspberry Pi Debug Probe](https://www.raspberrypi.com/products/debug-probe/) connected, you can flash over SWD and use the serial console.

### Flashing the ELF (OpenOCD / picotool)

- Run `Raspberry Pi Pico: Flash Pico Project (SWD)` from the command palette (macOS: `Cmd+Shift+P` / Windows and Linux: `Ctrl+Shift+P`)
- Or run `picotool load build/FMSynthEnsembleV3.elf -fx` in a terminal (macOS / Linux / WSL2)

Build artifacts:

| File | Purpose |
|------|---------|
| `build/FMSynthEnsembleV3.uf2` | Drag-and-drop flashing in BOOTSEL mode |
| `build/FMSynthEnsembleV3.elf` | Debug flashing with OpenOCD / picotool |

### Serial Console Wiring

Baud rate: 115200.

| Raspberry Pi Pico | Debug Probe |
|-------------------|-------------|
| Pin1 (UART0 TX) | Yellow (RX) |
| Pin2 (UART0 RX) | Orange (TX) |
| Pin3 (GND) | Black (GND) |

## Build Configuration

### Switching Boards

The default is `pico2` (RP2350), but you can switch to `pico` (RP2040).

In VS Code, run `Raspberry Pi Pico: Switch Board` from the command palette (macOS: `Cmd+Shift+P` / Windows and Linux: `Ctrl+Shift+P`), choose `pico2` or `pico`, then run Configure and Build again.

For manual switching, change the following line in `CMakeLists.txt`, then reconfigure and rebuild.

```cmake
set(PICO_BOARD pico2 CACHE STRING "Board type")   # RP2350: pico2 / RP2040: pico
```

### Build Options

Main options:

| Option | Default | Description |
|---|:---:|---|
| `BUILD_MIDI_PANEL` | `ON` | Enable the MIDI panel controller |
| `BUILD_SD_CARD` | `OFF` | Enable the SD card module |
| `USB_MIDI_IRQ_DRIVEN` | `ON` | Run TinyUSB in FreeRTOS integrated (interrupt-driven) mode. `OFF` selects Pico's standard polling mode |

Option values are managed in two places. **Keep them identical.**

| Location | Configure trigger | Purpose |
|---|---|---|
| `option()` defaults in `CMakeLists.txt` | Quick Access `Configure CMake` | Normal VS Code builds |
| `cacheVariables` in `CMakePresets.json` | Task `Configure: Default` / CLI `cmake --preset` | Preset-based builds and configuration reference |

## Gallery

<table>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/overview_1.jpeg"><img src="doc/image/overview_1.jpeg" width="280" alt="Overview 1"></a><br>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/overview_2.jpeg"><img src="doc/image/overview_2.jpeg" width="280" alt="Overview 2"></a><br>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/overview_3.jpeg"><img src="doc/image/overview_3.jpeg" width="280" alt="Overview 3"></a><br>
      <sub>MIDI connection and playback</sub>
    </td>
  </tr>
</table>

### Modules

<table>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/module_controller.jpeg"><img src="doc/image/module_controller.jpeg" width="280" alt="Controller module"></a><br>
      <b>Controller module</b><br>
      <sub>Raspberry Pi Pico 2<br>OPNA module connector (rear)</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/module_ym2608.jpeg"><img src="doc/image/module_ym2608.jpeg" width="280" alt="OPNA module"></a><br>
      <b>OPNA module</b><br>
      <sub>YM2608B + YM3016<br>MIDI panel connector</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/module_mixer.jpeg"><img src="doc/image/module_mixer.jpeg" width="280" alt="Mixer module"></a><br>
      <b>Mixer module</b><br>
      <sub>Mixes OPNA module outputs<br>LINE IN / LINE OUT</sub>
    </td>
  </tr>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/module_power.jpeg"><img src="doc/image/module_power.jpeg" width="280" alt="Power module"></a><br>
      <b>Power module</b><br>
      <sub>+6V, 2A input<br>+5V / ±12V output</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/module_midi_panel.jpeg"><img src="doc/image/module_midi_panel.jpeg" width="280" alt="MIDI panel"></a><br>
      <b>MIDI panel module</b><br>
      <sub>16-ch ON/OFF + LED<br>Connects to OPNA module</sub>
    </td>
    <td></td>
  </tr>
</table>

### Stack Assembly / Dock Connection

<table>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/stack_1.jpeg"><img src="doc/image/stack_1.jpeg" width="280" alt="Top view"></a><br>
      <b>Top view</b><br>
      <sub>Side stack of controller<br>and power modules</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/stack_2.jpeg"><img src="doc/image/stack_2.jpeg" width="280" alt="Side view"></a><br>
      <b>Side view</b><br>
      <sub>OPNA module connections</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/stack_3.jpeg"><img src="doc/image/stack_3.jpeg" width="280" alt="Dock connection"></a><br>
      <b>Dock connection</b><br>
      <sub>OPNA modules docked<br>to the backplane</sub>
    </td>
  </tr>
</table>
