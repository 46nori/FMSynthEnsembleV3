# ドキュメント一覧

## ビルド関連

| ドキュメント | 内容 |
|---|---|
| [build_ja.md](build_ja.md) | ビルド・書き込み・CMake 構成（日本語） |
| [build.md](build.md) | Build, flashing, CMake options (English) |

## ソフトウェア仕様

| ドキュメント | 内容 |
|---|---|
| [architecture.md](architecture.md) | プロジェクト全体構成・各レイヤの役割・依存制約・Build-time Switch |
| [domain/README.md](domain/README.md) | ドメインチャートと各ドメインのクラス図 |
| [design_concurrency.md](design_concurrency.md) | デュアルコアと FreeRTOS SMP による並列実行アーキテクチャ |
| [design_midi_ipc.md](design_midi_ipc.md) | 演奏イベント IPC を単一 FIFO とした判断経緯と計測結果 |
| [design_midi_message.md](design_midi_message.md) | MIDI メッセージのパース・ルーティング・Core 間転送設計 |
| [design_voice_allocation.md](design_voice_allocation.md) | 動的ボイスアロケーションアルゴリズム |
| [design_lfo.md](design_lfo.md) | ソフトウェア LFO（チャンネル共有・生成部分） |
| [design_pitch_effect.md](design_pitch_effect.md) | ピッチエフェクト（Pitch Bend・coarse tune・ビブラート深さ/遅延）とピッチ合成 |
| [design_tremolo.md](design_tremolo.md) | トレモロ（CC#92）と FM TL への振幅合成 |
| [design_rhythm.md](design_rhythm.md) | MIDI CH10 / リズム音源設計 |
| [design_csm_frame.md](design_csm_frame.md) | CSM音声合成 フレームタスク・IRQ・IPC |
| [design_volume_controller.md](design_volume_controller.md) | NJU72343 電子ボリューム制御 |
| [design_midi_panel.md](design_midi_panel.md) | MIDI パネルのソフトウェア設計 |
| [midi_implementation_chart.md](midi_implementation_chart.md) | MIDI 1.0 インプリメンテーションチャート |

## ハードウェア仕様

| ドキュメント | 内容 |
|---|---|
| [system_spec.md](system_spec.md) | ハードウェア構成・GPIO 接続・電気的仕様 |
| [spec_fm_chip.md](spec_fm_chip.md) | YM2608/YM2203/YMF288 の機能比較と最大 4 台混在時の制約 |
| [spec_opn.md](spec_opn.md) | FM音源LSI（YM2608/YM2203/YMF288）のレジスタ仕様・操作方法 |
| [spec_midi_panel.md](spec_midi_panel.md) | MIDI パネル（PanelSubsystem）のハードウェア仕様 |
| [piolib_spec.md](../src/drivers/fm/opn_piolib/doc/piolib_spec.md) | RaspberryPi PicoのPIOを使用したOPN/OPNAバス制御ライブラリ仕様 |

## 回路図

| ドキュメント | 内容 |
|---|---|
| [schematics/README.md](schematics/README.md) | 回路図一覧<br>コントローラモジュール<br>FM音源モジュール<br>オーディオミキサモジュール<br>電源モジュール<br>MIDIパネルモジュール|

## データシート

| ドキュメント | 内容 |
|---|---|
| [datasheet/README.md](datasheet/README.md) | データシート・参考文献一覧 |
