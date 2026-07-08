# FMSynthEnsembleV3 — AI エージェント向け作業指針

コーディングエージェント（Cursor、Claude Code、GitHub Copilot、Codex 等）向けの共通指示。
人向けの概要・ビルド手順は [README.md](README.md) を参照。

## プロジェクト概要

Raspberry Pi Pico（RP2040 / RP2350A）上で YAMAHA YM2608（OPNA）/ YM2203（OPN）を最大 4 基駆動する USB MIDI シンセサイザ。
FreeRTOS SMP で Core0 = I/O、Core1 = 音源エンジン。

## 必須: doc/ を先に読む

実装・修正・レビュー・調査の**前に**、関連する `doc/` の仕様を読み、設計意図と制約を把握すること。推測でアーキテクチャやハード仕様を書き換えない。

索引は [doc/README.md](doc/README.md)。主な参照先:

| 作業内容 | 参照ドキュメント |
|---------|----------------|
| 全体構成・レイヤ・依存関係 | [doc/architecture.md](doc/architecture.md) |
| GPIO・PIO・ハード接続 | [doc/system_spec.md](doc/system_spec.md) |
| 並列実行・タスク・Core 間通信 | [doc/design_concurrency.md](doc/design_concurrency.md) |
| MIDI パース・ルーティング | [doc/design_midi_message.md](doc/design_midi_message.md) |
| MIDI 実装状況（CC/RPN 等） | [doc/midi_implementation_chart.md](doc/midi_implementation_chart.md) |
| ボイスアロケーション | [doc/design_voice_allocation.md](doc/design_voice_allocation.md) |
| ビブラート / LFO | [doc/design_effect.md](doc/design_effect.md) |
| リズム（ch10） | [doc/design_rhythm.md](doc/design_rhythm.md) |
| CSM フレーム | [doc/design_csm_frame.md](doc/design_csm_frame.md) |
| 電子ボリューム | [doc/design_volume_controller.md](doc/design_volume_controller.md) |
| MIDI パネル | [doc/design_midi_panel.md](doc/design_midi_panel.md)（ハード仕様: [doc/spec_midi_panel.md](doc/spec_midi_panel.md)） |
| FM LSI レジスタ | [doc/spec_opn.md](doc/spec_opn.md) |
| ドメイン図・クラス図 | [doc/domain/README.md](doc/domain/README.md) |
| FM バス PIO ライブラリ | [src/drivers/fm/opn_piolib/doc/piolib_spec.md](src/drivers/fm/opn_piolib/doc/piolib_spec.md) |

仕様と実装が食い違う場合は、doc を正とするか実装を正とするかを判断し、必要なら doc 更新を提案する。
実装を正としてドキュメントを合わせる作業では、推測で仕様を書き足さない。不明点は `src/` を読んで確認する。

## ドキュメントの書き方

`doc/` を追加・改稿するときの恒常ルール。ルート README（[README.md](README.md) / [README_ja.md](README_ja.md)）はビルド・書き込み・クイックスタート（人向け）、`doc/` は設計・制約・ハード仕様（開発者向け）。重複は削り、相互にリンクする。

### 命名

```
design_<topic>.md   # 設計判断・振る舞い
spec_<topic>.md     # レジスタ・配線・機械仕様（既存の system_spec.md も同趣旨）
```

リネームや新規追加時は、`doc/README.md`・本ファイルの参照表・文書間リンクを更新する。

### 章立て・見出し

- 見出し番号は `## 1. タイトル` のようにアラビア数字 + ピリオドでよい
- **`§`（セクション記号）など、多バイト文字は使わない。** 相互参照はファイル名 + 見出しテキスト / アンカーで行う
- 文書内の章番号付けは一貫させる。階層はおおむね 3 段まで（`####` 以下の乱立を避ける）
- 各ドキュメント先頭に目的を置く。長い文書は目次も置く（短い文書は目次省略可）

### 文体・品質

- 自然な日本語の常体。翻訳調・生成 AI 特有の冗長な導入（「本ドキュメントでは〜」「〜することが重要です」の連発）を避ける
- 中国語由来の直訳調（「〜を行う」の乱用、「进行」「通过」的な構文）を避ける
- **絵文字は使わない**
- 技術判断の経緯（なぜそうしたか）は簡潔に残す。チューニングやデバッグの生ログは入れない
- **既知の制約・既知のバグ** は明示してよい
- **今後の対応計画・ロードマップ・未着手 Phase の予定** は設計書に書かない。未実装は「実装予定」とせず、実装チャートや制約として事実だけ書く
- ドメイン図・クラス図は現状の実装のみ描く。未実装の将来クラスは描かない

### 図表・コード

- 仮想コード・疑似コードは極力減らし、振る舞いは **Mermaid**（シーケンス・フロー・クラス図）で示す
- 実装と一致する設定値・マクロ名・関数名は表やインラインコードで参照してよい
- ASCII 図は Mermaid が不向きな箇所（ハード構成の概略など）に限る

### ソースと doc の参照方向

- ソースコメントから `*.md` の見出し番号や特定アンカーへ依存しない。コメントはコードだけで意味が通る説明にする
- 新規にソースから `doc/` の特定見出しへのリンクを追加しない
- ドキュメント同士の相互リンクはファイル名 + 見出しアンカーでよい

## レイヤと依存制約

```
app → midi, synth, platform, drivers/usb
synth → drivers/fm, drivers/midi_panel（インターフェース経由）
platform → drivers, extern, pico-sdk
drivers → extern, pico-sdk（platform には依存しない）
```

- `app/`: ハード直接操作禁止。`Platform::*` と `synth` API のみ
- `midi/`: pico-sdk・FreeRTOS・ドライバに依存しない。Single Parse Rule（Core0 のみパース）
- `synth/`: FM アクセスは `drivers/fm` 経由
- `platform/`: ピン割り当て・PIO 所有・初期化順を集約。GPIO は上位でハードコードしない
- `extern/`: 直接編集禁止。ラッパー（主に `platform`）経由で利用

## ビルド・検証

```bash
git submodule update --init --recursive
cmake --preset default
ninja -C build
```

- デフォルトターゲット: **RP2350A**（RP2040 は CMake オプションで切替）
- 生成物: `build/FMSynthEnsembleV3.uf2`, `build/FMSynthEnsembleV3.elf`
- 変更後は可能な範囲でビルドが通ることを確認する

## 実装時の注意

- タスク優先度・スタック・Core Affinity は `src/app/task_config.h` が唯一の定義元
- FM バス用 GPIO2–15 は `opn_piolib` 専用。別 PIO プログラムを同ピンに同時有効化しない
- Build-time スイッチは CMake `target_compile_definitions`。`config.h` はアプリ層の実行時ポリシー定数に限定
- 変更は最小スコープで。既存の命名・抽象化・コメント水準に合わせる
- コミットはユーザーが明示的に依頼したときのみ作成する
