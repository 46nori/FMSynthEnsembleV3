# platform ドメイン

ボード統合レイヤ（`src/platform/`）。この基板で使うハードウェア資源の所有、初期化順、GPIO/PIO 割り当てを集約する。`drivers` / `extern` / pico-sdk に依存してよいが、`synth` / `app` には依存しない。

関連設計書: [design_volume_controller.md](../design_volume_controller.md)、[system_spec.md](../system_spec.md)

```mermaid
classDiagram
    class Platform_init {
        <<module>>
        +Initialize()
        +SetupFmModules(out_error) unique_ptr~FmSystem~
    }

    class FmSystem {
        +bus : fm_bus_t
        +devices : fm_device_t[4]
        +module_ptr : unique_ptr~OpnBase~[4]
        +modules : OpnBase*[4]
    }

    class VolumeController {
        <<singleton>>
        +GetInstance() VolumeController$
        +InitializeEarlyMute()
        +SetDockModuleTypes(types)
        +MuteFmSsg() / MuteLineIn()
        +MuteLineMix() / MuteLineSample()
        +SetFmSsgVolumeDb(db)
        +SetLineMixVolumeDb(db) / SetLineSampleVolumeDb(db)
        +SetVolumeRaw(chip_addr, channel, value)
    }

    class Platform_isr {
        <<module>>
        +FM_IRQ : uint
        +AttachIsrCallback(pin, func, ctx)
        +EnableIsrCallback(pin)
        +DisableIsrCallback(pin)
    }

    class Platform_display {
        <<module>>
        +InitializeI2cBus()
        +DisplayWrite(column, row, text)
        +GetCharacterDisplay() CharacterDisplayInterface&
    }

    class LcdCharacterDisplayAdapter {
        <<implements CharacterDisplayInterface>>
        行0をステータス行として予約し、LcdMenu には行1-3を見せる
    }

    class Rw1063Display {
        <<drivers/display>>
    }

    class freertos_hooks {
        <<module>>
        vApplicationStackOverflowHook
        vApplicationMallocFailedHook
    }

    class NJU72343 {
        <<extern>>
    }
    class opn_piolib {
        <<drivers/fm>>
    }
    class OpnBase {
        <<drivers/fm>>
    }

    Platform_init ..> FmSystem : creates
    Platform_init --> VolumeController : early mute on boot
    Platform_init --> opn_piolib : fm_bus_init / fm_device_init
    FmSystem o-- OpnBase : 4 dock
    VolumeController --> NJU72343 : PIO1 / GPIO27-28
    Platform_display --> Rw1063Display : I2C0 / GPIO20-21
    Platform_display --> LcdCharacterDisplayAdapter : owns
    LcdCharacterDisplayAdapter --> Rw1063Display : wraps
```

`NJU72343` は `extern/` の外部ライブラリ、`opn_piolib` / `OpnBase` は `drivers/fm` に属する（灰色の外部要素として `<<extern>>` / `<<drivers/fm>>` を注記）。

| 要素 | ファイル | 責務 |
|---|---|---|
| `Platform::Initialize` | `init.cpp` | stdio・GPIO・FM リセット・電子ボリューム早期ミュート・SD・USB の初期化 |
| `Platform::SetupFmModules` | `init.cpp` | FM バス（PIO0）初期化、モジュール自動識別（YM2608/YM2203/YMF288/未接続）、`FmSystem` 構築 |
| `Platform::VolumeController` | `volume_controller.h/cpp` | NJU72343 のボード固有ラッパー。PIO1/GPIO27/28 の所有、dock 状態管理、dB 指定 API |
| `Platform::AttachIsrCallback` 等 | `isr.h/cpp` | GPIO 割り込み登録（`FM_IRQ` = 全 Dock /IRQ の Wired-OR） |
| `Platform::DisplayWrite` 等 | `display.h/cpp` | I2C バスとキャラクタ LCD の所有・初期化（`BUILD_I2C_DISPLAY=ON` 時）。ステータス行用の書き込み API と LcdMenu 向け `CharacterDisplayInterface` を提供 |
| `LcdCharacterDisplayAdapter` | `lcd_character_display_adapter.h/cpp` | LcdMenu の `CharacterDisplayInterface` を `drivers/display` で実装（[design_display_menu.md](../design_display_menu.md#4-ディスプレイ側-characterdisplayinterfaceアダプタ)） |
| FreeRTOS フック | `freertos_hooks.cpp` | スタックオーバーフロー・ヒープ枯渇時の記録 |
| `FreeRTOSConfig.h` | — | FreeRTOS カーネル設定（[design_concurrency.md](../design_concurrency.md#6-freertos-設定)） |
