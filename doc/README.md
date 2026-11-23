# ドキュメント一覧

## 目的別の入口

| 目的 | 最初に読む文書 |
|---|---|
| ファームウェアのビルドと書き込み | [build_ja.md](build_ja.md)（English: [build.md](build.md)） |
| MIDI インプリメンテーションチャート| [midi_implementation_chart.md](midi_implementation_chart.md) |
| ハードウェア仕様 | [hardware/spec_system.md](hardware/spec_system.md)、[hardware/schematics/README.md](hardware/schematics/README.md) |
| ソフトウェア仕様 | [architecture/architecture.md](architecture/architecture.md) → [architecture/design_concurrency.md](architecture/design_concurrency.md) |
| 設計判断 | [ADR/README.md](ADR/README.md) |

## architecture/ — 全体構成

| ドキュメント | 内容 |
|---|---|
| [architecture.md](architecture/architecture.md) | ディレクトリ構成、各レイヤの役割、依存制約、Build-time Switch |
| [design_concurrency.md](architecture/design_concurrency.md) | デュアルコアと FreeRTOS SMP によるタスク構成、Core 間通信 |
| [design_midi_pipeline.md](architecture/design_midi_pipeline.md) | MIDI メッセージのパース、ルーティング、Core 間転送 |
| [domain/README.md](architecture/domain/README.md) | ドメインチャートと各ドメインのクラス図 |

## synth/ — シンセサイザの基本設計

| ドキュメント | 内容 |
|---|---|
| [design_voice_allocation.md](synth/design_voice_allocation.md) | 動的ボイスアロケーション |
| [design_rhythm.md](synth/design_rhythm.md) | MIDI CH10 とリズム音源 |
| [design_csm_frame.md](synth/design_csm_frame.md) | CSM 音声合成のフレームタスク、IRQ、IPC |
| [design_lfo.md](synth/design_lfo.md) | ソフトウェア LFO（チャンネル共有、位相とレートの生成） |
| [design_pitch_effect.md](synth/design_pitch_effect.md) | Pitch Bend、coarse tune、ビブラート深さ/遅延とピッチ合成 |
| [design_tremolo.md](synth/design_tremolo.md) | トレモロ（CC#92）と FM TL への振幅合成 |

## smf/ — SMF 再生

| ドキュメント | 内容 |
|---|---|
| [design_smf_player.md](smf/design_smf_player.md) | SD カード上の SMF ファイル再生（SmfPlayerTask、パーサー） |
| [design_smf_playback.md](smf/design_smf_playback.md) | 再生制御（Stop/Pause、プレイリスト、リピート、シャッフル） |

## midi_panel/ — MIDI パネル

| ドキュメント | 内容 |
|---|---|
| [design_midi_panel.md](midi_panel/design_midi_panel.md) | ソフトウェア設計 |
| [spec_midi_panel.md](midi_panel/spec_midi_panel.md) | ハードウェア仕様（PanelSubsystem） |

## display/ — LCD とメニュー

| ドキュメント | 内容 |
|---|---|
| [design_display_menu.md](display/design_display_menu.md) | LcdMenu ベースのジョイスティック操作メニュー |
| [spec_display_i2c.md](display/spec_display_i2c.md) | I2C 接続キャラクタ LCD のデバイス仕様、GPIO 割り当て、バス接続 |

## volume/ — 電子ボリューム

| ドキュメント | 内容 |
|---|---|
| [design_volume_controller.md](volume/design_volume_controller.md) | NJU72343 の制御方針と API |
| [spec_volume_controller.md](volume/spec_volume_controller.md) | NJU72343 の配線とレジスタ仕様 |

## hardware/ — ハードウェア共通

| ドキュメント | 内容 |
|---|---|
| [spec_system.md](hardware/spec_system.md) | ハードウェア構成、GPIO 接続、電気的仕様 |
| [spec_fm_chip.md](hardware/spec_fm_chip.md) | YM2608/YM2203/YMF288 の機能比較と最大 4 台混在時の制約 |
| [spec_opn.md](hardware/spec_opn.md) | FM 音源 LSI のレジスタ仕様、バスタイミング、自動識別、音量制御 |
| [piolib_spec.md](../src/drivers/fm/opn_piolib/doc/piolib_spec.md) | PIO による FM 音源バス制御ライブラリ |
| [schematics/README.md](hardware/schematics/README.md) | 回路図一覧 |
| [datasheet/README.md](hardware/datasheet/README.md) | データシート、参考文献一覧 |

## ADR/ — 設計判断の記録

一覧と書き方は [ADR/README.md](ADR/README.md)。

## process/ — 開発プロセス

| ドキュメント | 内容 |
|---|---|
| [sbom.md](process/sbom.md) | SBOM（CycloneDX）とサードパーティライセンス表記の生成・検査、依存更新の影響判定 |
