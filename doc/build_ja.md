# ファームウェアのビルドと書き込み方法

## 1. VSCode による方法（推奨）

VS CodeにRaspberry Pi Pico 拡張をインストールすることで、ビルド環境が自動でセットアップされる。この拡張は **macOS / Windows / Linux** で利用できる。

### 1.1 準備（初回のみ）

1. [VS Code](https://code.visualstudio.com/) をインストールする
2. VS Code に Raspberry Pi 公式拡張 **Raspberry Pi Pico**（ID: `raspberry-pi.raspberry-pi-pico`）をインストールし、VS Code を再起動する
3. リポジトリを取得し、サブモジュールを展開する

   ```bash
   git clone <このリポジトリ>
   cd FMSynthEnsembleV3
   git submodule update --init --recursive
   ```

   Windows では Git for Windows 付属の Git Bash、macOS / Linux では各 OS 標準のターミナルで実行する。

4. VS Code でこのフォルダを開き、サイドバーの Raspberry Pi Pico ビュー（Quick Access）から `Configure CMake` を実行する

### 1.2 ビルド

| OS | ビルド | コマンドパレット |
|---|---|---|
| macOS | `Cmd+Shift+B` | `Cmd+Shift+P` |
| Windows / Linux | `Ctrl+Shift+B` | `Ctrl+Shift+P` |

`Compile Project` を実行する。Quick Access の `Compile` でもよい。  
成功すると `build/FMSynthEnsembleV3.uf2` が生成される。

### 1.3 書き込み

1. Raspberry Pi Pico の `BOOTSEL` ボタンを押しながら USB ケーブルで PC に接続する（マスストレージ `RPI-RP2` として認識される）
2. `build/FMSynthEnsembleV3.uf2` を `RPI-RP2` へコピーする

   | OS | コピー先の例 |
   |---|---|
   | macOS | Finder の `RPI-RP2` ボリューム、または `/Volumes/RPI-RP2/` |
   | Windows | エクスプローラーの `RPI-RP2` ドライブ（例: `D:\`） |
   | Linux | ファイルマネージャのマウント先（例: `/media/<user>/RPI-RP2` や `/run/media/<user>/RPI-RP2`） |

3. コピー完了後に自動で再起動し、新しいファームウェアが起動する

PC からは USB MIDI デバイスとして見える。ボードのデフォルトは **Raspberry Pi Pico 2 (RP2350A)**。Pico (RP2040) は [3. ボードの切替](#3-ボードの切替)。

## 2. コマンドライン(CLI)による方法

pico-sdk 2.3.0、ARM GCC 15.2、CMake 3.30+（`cmake --preset` / `CMakePresets.json` schema version 9 に必要）、Ninja があれば VS Code なしでもビルドできる。

| OS | 補足 |
|---|---|
| macOS | Homebrew 等でツールチェーンを入れるか、Pico 拡張が配置した SDK / ツールチェーンを `PICO_SDK_PATH` 等で参照する |
| Linux | ディストリビューションのパッケージ、または [pico C SDK](https://www.raspberrypi.com/documentation/pico-sdk/) に従って導入する |
| Windows | **WSL2** または **MSYS2 / Git Bash** で以下のコマンドを実行するのが一般的。ネイティブ Windows シェルだけでは ARM GCC の導入が煩雑なため、CLI ビルドは WSL2 を推奨 |

### 2.1 準備（初回のみ）

```bash
git submodule update --init --recursive
```

FreeRTOS-Kernel は git submodule ではない（pico-sdk も同様）。CMake は次のいずれかがあればよい。

1. `FREERTOS_KERNEL_PATH`（環境変数または `-D`）
2. `${PICO_SDK_PATH}/../FreeRTOS-Kernel`
3. `${PICO_SDK_PATH}/../../FreeRTOS-Kernel`

Pico 拡張を既に使っている場合は、拡張が SDK と FreeRTOS をセットで置いている。`PICO_SDK_PATH` をその SDK（例: `~/.pico-sdk/sdk/2.3.0`）に向ければ、隣の `FreeRTOS-Kernel` が使われ、追加 clone は不要。

素の pico-sdk だけを入れた場合の例:

```bash
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git
export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
```

### 2.2 ビルド（通常）

```bash
cmake --preset default   # 初回、または CMakeLists / プリセット / ボード切替のあと
ninja -C build           # ソース変更のたび
```

`cmake --preset default` は `build/compile_commands.json` も生成する。clangd はリポジトリ直下の `.clangd` 経由でこれを読み、pico-sdk と ARM GCC の include パスを解決する。

### 2.3 書き込み

[1.3 書き込み](#13-書き込み)と同じ。`build/FMSynthEnsembleV3.uf2` を BOOTSEL 中の Pico へコピーする。

## 3. RaspberryPi Picoのボード切替方法

デフォルトは `pico2`（RP2350）だが、`pico`（RP2040）にも切り替えられる。

VS Code の場合はコマンドパレット（macOS: `Cmd+Shift+P` / Windows・Linux: `Ctrl+Shift+P`）で `Raspberry Pi Pico: Switch Board` を実行して `pico2` / `pico` を選び、Configure と Build をやり直す。

手動の場合は `CMakeLists.txt` の次の行を変更してから再構成・再ビルドする。

```cmake
set(PICO_BOARD pico2 CACHE STRING "Board type")   # RP2350: pico2 / RP2040: pico
```

## 4. ビルドオプション

| オプション | デフォルト | 説明 |
|---|:---:|---|
| `BUILD_MIDI_PANEL` | `ON` | MIDI パネルコントローラを有効にする |
| `BUILD_SD_CARD` | `OFF` | SD カードモジュールを有効にする |
| `USB_MIDI_IRQ_DRIVEN` | `ON` | TinyUSB を FreeRTOS 統合モード（割り込み駆動）で動作させる。`OFF` で Pico 標準のポーリングモード |

オプションの値は次の 2 か所で管理されており、**常に同じ値にそろえる**こと。

| 場所 | Configure の契機 | 用途 |
|---|---|---|
| `CMakeLists.txt` の `option()` デフォルト値 | Quick Access の `Configure CMake` | VS Code からの通常ビルド |
| `CMakePresets.json` の `cacheVariables` | Tasks `Configure: Default` / CLI `cmake --preset` | プリセット明示ビルド・構成仕様書 |

## 5. デバッガ

[Raspberry Pi Debug Probe](https://www.raspberrypi.com/products/debug-probe/) を接続すると、SWD 経由の書き込みとシリアルコンソールが使える。

### 5.1 ELF の書き込み（OpenOCD / picotool）

- コマンドパレット（macOS: `Cmd+Shift+P` / Windows・Linux: `Ctrl+Shift+P`）で `Raspberry Pi Pico: Flash Pico Project (SWD)` を実行する
- またはターミナルで `picotool load build/FMSynthEnsembleV3.elf -fx` を実行する（macOS / Linux / WSL2）

| ファイル | 用途 |
|---------|------|
| `build/FMSynthEnsembleV3.uf2` | BOOTSEL モードでのドラッグ＆ドロップ書き込み |
| `build/FMSynthEnsembleV3.elf` | OpenOCD / picotool でのデバッグ書き込み |

### 5.2 シリアルコンソール接続

ボーレートは 115200 baud。

| Raspberry Pi Pico | Debug Probe |
|-------------------|------------|
| Pin1 (UART0 TX) | 黄 (RX) |
| Pin2 (UART0 RX) | 橙 (TX) |
| Pin3 (GND) | 黒 (GND) |

## 6. CI / CD

`main`ブランチ への push / pull request で [`.github/workflows/build.yml`](../.github/workflows/build.yml) が走る。
pico-sdk と FreeRTOS-Kernel は submodule ではないため CI が別途 checkout する。
`PICO_BOARD=pico2` と `PICO_BOARD=pico` をデフォルトの CMake オプションでビルドし、それぞれ Artifact として保存する。
