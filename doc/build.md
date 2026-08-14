# Build and flashing

Firmware build with VS Code or the CLI, board switching, CMake options, debugger use, and CI. Product overview and the short entry point are in [README.md](../README.md).

## 1. VS Code (recommended)

The Raspberry Pi Pico extension provides pico-sdk, the toolchain, CMake, and Ninja. It is available on **macOS, Windows, and Linux**.

### 1.1 Setup (first time only)

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

### 1.2 Build

| OS | Build | Command Palette |
|---|---|---|
| macOS | `Cmd+Shift+B` | `Cmd+Shift+P` |
| Windows / Linux | `Ctrl+Shift+B` | `Ctrl+Shift+P` |

Run `Compile Project`, or use `Compile` from Quick Access.  
On success, `build/FMSynthEnsembleV3.uf2` is generated.

### 1.3 Flash firmware

1. Hold the Raspberry Pi Pico `BOOTSEL` button while connecting it to your PC via USB (it appears as the `RPI-RP2` mass-storage device)
2. Copy `build/FMSynthEnsembleV3.uf2` to `RPI-RP2`

   | OS | Typical destination |
   |---|---|
   | macOS | The `RPI-RP2` volume in Finder, or `/Volumes/RPI-RP2/` |
   | Windows | The `RPI-RP2` drive in Explorer (e.g. `D:\`) |
   | Linux | The mount point in your file manager (e.g. `/media/<user>/RPI-RP2` or `/run/media/<user>/RPI-RP2`) |

3. After the copy completes, the board reboots automatically and runs the new firmware

The device appears on your PC as a USB MIDI device. The default board is **Raspberry Pi Pico 2 (RP2350A)**. For Pico (RP2040), see [3. Switching boards](#3-switching-boards).

## 2. Command line

If you have pico-sdk 2.3.0, ARM GCC 15.2, CMake 3.30+ (required by `cmake --preset` / `CMakePresets.json` schema version 9), and Ninja, you can build without VS Code.

| OS | Notes |
|---|---|
| macOS | Install the toolchain with Homebrew, etc., or point `PICO_SDK_PATH` and related variables at the SDK/toolchain installed by the Pico extension |
| Linux | Use your distribution packages, or follow the [pico C SDK instructions](https://www.raspberrypi.com/documentation/pico-sdk/) |
| Windows | Run the commands below in **WSL2** or **MSYS2 / Git Bash**. Native Windows shells make ARM GCC setup cumbersome, so WSL2 is recommended for CLI builds |

### 2.1 Setup (first time only)

```bash
git submodule update --init --recursive
```

FreeRTOS-Kernel is not a git submodule (neither is pico-sdk). CMake needs one of:

1. `FREERTOS_KERNEL_PATH` (environment or `-D`)
2. `${PICO_SDK_PATH}/../FreeRTOS-Kernel`
3. `${PICO_SDK_PATH}/../../FreeRTOS-Kernel`

If you already use the Pico VS Code extension, it installs the SDK and FreeRTOS together. Point `PICO_SDK_PATH` at that SDK (e.g. `~/.pico-sdk/sdk/2.3.0`) and the adjacent `FreeRTOS-Kernel` is used — no extra clone.

If you installed a standalone pico-sdk only:

```bash
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git
export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
```

### 2.2 Build (regular)

```bash
cmake --preset default   # first time, or after CMakeLists / preset / board changes
ninja -C build           # after source changes
```

`cmake --preset default` also generates `build/compile_commands.json`. clangd reads it through `.clangd` at the repository root to resolve include paths for pico-sdk and ARM GCC.

### 2.3 Flash firmware

Same as [1.3 Flash firmware](#13-flash-firmware). Copy `build/FMSynthEnsembleV3.uf2` to the Pico in BOOTSEL mode.

## 3. Switching boards

The default is `pico2` (RP2350), but you can switch to `pico` (RP2040).

In VS Code, run `Raspberry Pi Pico: Switch Board` from the command palette (macOS: `Cmd+Shift+P` / Windows and Linux: `Ctrl+Shift+P`), choose `pico2` or `pico`, then run Configure and Build again.

For manual switching, change the following line in `CMakeLists.txt`, then reconfigure and rebuild.

```cmake
set(PICO_BOARD pico2 CACHE STRING "Board type")   # RP2350: pico2 / RP2040: pico
```

## 4. Build options

| Option | Default | Description |
|---|:---:|---|
| `BUILD_MIDI_PANEL` | `ON` | Enable the MIDI panel controller |
| `BUILD_SD_CARD` | `OFF` | Enable the SD card module |
| `ENABLE_MIDI_TIMING_STATS` | `OFF` | Enable detailed MIDI queue-delay and event-execution timing instrumentation |
| `USB_MIDI_IRQ_DRIVEN` | `ON` | Run TinyUSB in FreeRTOS integrated (interrupt-driven) mode. `OFF` selects Pico's standard polling mode |

Diagnostic build with detailed timing:

```bash
cmake --preset default -DENABLE_MIDI_TIMING_STATS=ON
ninja -C build
```

CMake vs `config.h` placement, and the full switch list including `ENABLE_DEBUG_PRINT` / `ENABLE_CSM`, are in [architecture.md](architecture.md#7-build-time-switch).

Option values are managed in two places. **Keep them identical.**

| Location | Configure trigger | Purpose |
|---|---|---|
| `option()` defaults in `CMakeLists.txt` | Quick Access `Configure CMake` | Normal VS Code builds |
| `cacheVariables` in `CMakePresets.json` | Task `Configure: Default` / CLI `cmake --preset` | Preset-based builds and configuration reference |

## 5. Debugger

With a [Raspberry Pi Debug Probe](https://www.raspberrypi.com/products/debug-probe/) connected, you can flash over SWD and use the serial console.

### 5.1 Flashing the ELF (OpenOCD / picotool)

- Run `Raspberry Pi Pico: Flash Pico Project (SWD)` from the command palette (macOS: `Cmd+Shift+P` / Windows and Linux: `Ctrl+Shift+P`)
- Or run `picotool load build/FMSynthEnsembleV3.elf -fx` in a terminal (macOS / Linux / WSL2)

| File | Purpose |
|------|---------|
| `build/FMSynthEnsembleV3.uf2` | Drag-and-drop flashing in BOOTSEL mode |
| `build/FMSynthEnsembleV3.elf` | Debug flashing with OpenOCD / picotool |

### 5.2 Serial console wiring

Baud rate: 115200.

| Raspberry Pi Pico | Debug Probe |
|-------------------|-------------|
| Pin1 (UART0 TX) | Yellow (RX) |
| Pin2 (UART0 RX) | Orange (TX) |
| Pin3 (GND) | Black (GND) |

## 6. CI / CD

Push and pull requests to `main` run [`.github/workflows/build.yml`](../.github/workflows/build.yml). It checks out pico-sdk and FreeRTOS-Kernel separately (they are not git submodules), builds `PICO_BOARD=pico2` and `PICO_BOARD=pico` with the default CMake options, and uploads firmware artifacts for each.
