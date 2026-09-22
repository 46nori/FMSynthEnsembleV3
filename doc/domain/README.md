# ドメイン図

`src/` 配下の 5 パッケージ（`app` / `midi` / `synth` / `platform` / `drivers`）をドメインとみなし、依存関係と各ドメインの主要クラスを示す。レイヤの役割と依存制約の背景は [architecture.md](../architecture.md) を参照。

## 全体ドメインチャート

```mermaid
flowchart TD
    app["app<br>Tasks / IPC / Entry point"]
    midi["midi<br>MIDI parse / Routing"]
    synth["synth<br>Channels / Voices / Sound abstraction"]
    platform["platform<br>Board integration / Resource ownership"]
    drivers["drivers<br>fm / midi_panel / display / usb / storage"]
    ext["extern / pico-sdk / FreeRTOS"]

    app --> midi
    app --> synth
    app --> platform
    app -- "drivers/usb (TinyUSB API)" --> drivers
    synth -- "drivers/fm · drivers/midi_panel<br>(via interfaces)" --> drivers
    synth -. "CsmVoice: FM /IRQ ISR登録のみ（例外）" .-> platform
    platform --> drivers
    platform --> ext
    drivers --> ext

    classDef external fill:#eee,stroke:#999,color:#666;
    class ext external;
```

依存制約（[AGENTS.md](../../AGENTS.md) と同一）:

- `app` → `midi`, `synth`, `platform`, `drivers/usb`。例外: `app/ui` は `extern/LcdMenu` を直接扱う（[design_display_menu.md](../design_display_menu.md#6-レイヤ配置)）
- `synth` → `drivers/fm`, `drivers/midi_panel`（インターフェース経由）。例外: `CsmVoice` の FM `/IRQ` ISR 登録・`FrameTick` 通知のみ `platform`/`app` に直接依存する（[domain_synth.md](domain_synth.md)、[design_csm_frame.md](../design_csm_frame.md) 7章）
- `platform` → `drivers`, `extern`, pico-sdk
- `drivers` → `extern`, pico-sdk（`platform` には依存しない）
- `midi` → pico-sdk・FreeRTOS・ドライバに非依存

## 各ドメインのクラス図

| ドキュメント | 内容 |
|---|---|
| [domain_app.md](domain_app.md) | タスク・IPC・デバッガ・設定・LCD メニュー |
| [domain_midi.md](domain_midi.md) | MidiMessage / MidiParser / MidiController / MidiSysEx |
| [domain_synth.md](domain_synth.md) | MidiProcessor / チャンネル / Voice / アロケータ |
| [domain_platform.md](domain_platform.md) | 初期化 / VolumeController / ISR / ディスプレイ |
| [domain_drivers.md](domain_drivers.md) | OpnBase 系 / opn_piolib / MIDI パネル / ディスプレイ / USB |
