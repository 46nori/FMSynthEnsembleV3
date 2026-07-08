# ドキュメント一覧

## 設計ドキュメント

| ドキュメント | 内容 |
|---|---|
| [architecture.md](architecture.md) | プロジェクト全体構成・各レイヤの役割・依存制約 |
| [system_spec.md](system_spec.md) | ハードウェア構成・GPIO 接続・電気的仕様 |
| [design_concurrency.md](design_concurrency.md) | デュアルコアと FreeRTOS SMP による並列実行アーキテクチャ |
| [design_midi_message.md](design_midi_message.md) | MIDI メッセージのパース・ルーティング・Core 間転送設計 |
| [design_voice_allocation.md](design_voice_allocation.md) | 動的ボイスアロケーションアルゴリズム |
| [design_effect.md](design_effect.md) | ビブラート / ソフトウェア LFO / ピッチ合成 |
| [design_rhythm.md](design_rhythm.md) | MIDI CH10 / YM2608 リズム音源設計 |
| [design_csm_frame.md](design_csm_frame.md) | CSM フレームタスク・IRQ・IPC |
| [design_volume_controller.md](design_volume_controller.md) | NJU72343 電子ボリューム制御 |
| [design_midi_panel.md](design_midi_panel.md) | MIDI パネルのソフトウェア設計 |
| [midi_implementation_chart.md](midi_implementation_chart.md) | MIDI 1.0 インプリメンテーションチャート（CC/RPN/チャンネル差） |

## ドメイン図

| ドキュメント | 内容 |
|---|---|
| [domain/README.md](domain/README.md) | ドメインチャートと各ドメインのクラス図 |

## ハードウェア仕様

| ドキュメント | 内容 |
|---|---|
| [spec_opn.md](spec_opn.md) | FM音源LSI（YM2608/YM2203）のレジスタ仕様・操作方法 |
| [spec_midi_panel.md](spec_midi_panel.md) | MIDI パネル（PanelSubsystem）のハードウェア仕様 |
| [../src/drivers/fm/opn_piolib/doc/piolib_spec.md](../src/drivers/fm/opn_piolib/doc/piolib_spec.md) | RaspberryPi PicoのPIOを使用したOPN/OPNAバス制御ライブラリ仕様 |

## 回路図

| ドキュメント | 内容 |
|---|---|
| [schematics/README.md](schematics/README.md) | 回路図一覧 |
| [schematics/](schematics/) | コントローラモジュール<br>FM音源モジュール<br>オーディオミキサモジュール<br>電源モジュール<br>MIDIパネルモジュール |

## データシート

| ドキュメント | 内容 |
|---|---|
| [datasheet/README.md](datasheet/README.md) | データシート・参考文献一覧 |
| [datasheet/](datasheet/) | YM2608, YM2203, YM3014B, YM3016 |
