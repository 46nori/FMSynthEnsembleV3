# FMSynthEnsembleV3 — AI エージェント向け作業指針

コーディングエージェント（Cursor、Claude Code、GitHub Copilot、Codex 等）向けの共通指示。
人向けの概要は [README.md](README.md)、ビルド手順は [doc/build.md](doc/build.md) / [doc/build_ja.md](doc/build_ja.md) を参照。

## プロジェクト概要

Raspberry Pi Pico（RP2040 / RP2350A）上で YAMAHA YM2608（OPNA）/ YM2203（OPN）/ YMF288（OPN3-L）を最大 4 基駆動する USB MIDI シンセサイザ。
FreeRTOS SMP で Core0 = I/O、Core1 = 音源エンジン。

## 必須: doc/ を先に読む

実装・修正・レビュー・調査の**前に**、関連する `doc/` の仕様を読み、設計意図と制約を把握すること。推測でアーキテクチャやハード仕様を書き換えない。

`doc/` はサブシステムごとのフォルダに分かれている。どのフォルダを読むかは下表で決め、個々のファイルは索引 [doc/README.md](doc/README.md) で探す。

| 作業の対象 | フォルダ |
|---|---|
| 全体構成・レイヤ・依存関係・Build-time Switch、タスク・Core 間通信、MIDI パース・ルーティング、クラス図 | [doc/architecture/](doc/architecture/) |
| ボイス割り当て・リズム・CSM・LFO・ビブラート・トレモロ（`src/synth/`） | [doc/synth/](doc/synth/) |
| SMF 再生（`src/smf/`、`SmfPlayerTask`） | [doc/smf/](doc/smf/) |
| MIDI パネル | [doc/midi_panel/](doc/midi_panel/) |
| LCD とメニュー（`app/ui/`） | [doc/display/](doc/display/) |
| 電子ボリューム（NJU72343） | [doc/volume/](doc/volume/) |
| GPIO・PIO・ハード接続、FM 音源 LSI のレジスタと混在制約、回路図、データシート | [doc/hardware/](doc/hardware/) |
| 過去の設計判断とその理由（ADR） | [doc/ADR/](doc/ADR/) |
| SBOM・ライセンス表記・依存更新の影響判定 | [doc/process/](doc/process/) |
| MIDI 実装状況（CC/RPN 等） | [doc/midi_implementation_chart.md](doc/midi_implementation_chart.md) |
| ビルド・CMake 構成 | [doc/build.md](doc/build.md) / [doc/build_ja.md](doc/build_ja.md) |
| FM バス PIO ライブラリ | [src/drivers/fm/opn_piolib/doc/piolib_spec.md](src/drivers/fm/opn_piolib/doc/piolib_spec.md) |

仕様と実装が食い違う場合は、doc を正とするか実装を正とするかを判断し、必要なら doc 更新を提案する。
実装を正としてドキュメントを合わせる作業では、推測で仕様を書き足さない。不明点は `src/` を読んで確認する。

## ドキュメントの書き方

`doc/` を追加・改稿するときの恒常ルール。ルート README（[README.md](README.md) / [README_ja.md](README_ja.md)）は概要・クイックスタート入口・ギャラリー（人向け）、[doc/build.md](doc/build.md) / [doc/build_ja.md](doc/build_ja.md) はビルド・書き込み・CMake 構成、その他の `doc/` は設計・制約・ハード仕様（開発者向け）。重複は削り、相互にリンクする。

### 配置と命名

`doc/` 直下には利用者向けの文書（`build.md` / `build_ja.md`、`midi_implementation_chart.md`）と索引 `README.md` だけを置く。それ以外はサブシステムごとのフォルダに置き、同じサブシステムの設計（design）とハード仕様（spec）は同じフォルダに並べる。

```
design_<topic>.md   # 設計・振る舞い（現在どうなっているか）
spec_<topic>.md     # レジスタ・配線・機械仕様
ADR/NNNN-<要約>.md  # 設計判断の記録（ADR）。書き方は doc/ADR/README.md
```

- 設計書には現在の設計を書く。なぜそう決めたかの経緯や、比較した選択肢・計測結果は ADR に書き、設計書からは要点を一文で示してリンクする
- 同じ事実を複数の文書に書かない。定義元を 1 つ決め、他からはリンクする（例: タスク構成は `design_concurrency.md`）
- テスト件数のように開発のたびに変わる数値は書かない。設定値を書くときは定義元の定数名を併記する
- 新規追加・移動・リネーム時は、`doc/README.md` と文書間リンクを更新する。新しいフォルダを作ったときは本ファイルのフォルダ表も更新する

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

- ソース・テストのコメントから文書を参照するときは、ファイル名だけを書く（例: `design_smf_player.md 参照`）。`doc/` からのパス、見出し番号、アンカーは書かない。文書の移動や章の振り直しでソースを直さずに済むようにするため
- `doc/` 内のファイル名は（`README.md` を除き）重複させない。ファイル名だけで文書が一意に決まることを前提にしている
- 文書のファイル名を変えたときは、ソース・テストのコメント内の参照も合わせて直す
- コメントはコードだけで意味が通る説明にし、文書への参照は補足にとどめる
- ドキュメント同士の相互リンクはファイル名 + 見出しアンカーでよい

## レイヤと依存制約

```
app → midi, synth, platform, drivers/usb, smf（BUILD_SD_CARD=ON時）, extern/LcdMenu（app/ui のみ、BUILD_I2C_DISPLAY=ON時）, pico-sdk（時刻・stdio のみ）
synth → midi（純粋なイベント型・Controller Action のみ）, drivers/fm, drivers/midi_panel（インターフェース経由）
platform → drivers, extern, pico-sdk, smf（BUILD_SD_CARD=ON時）
drivers → extern, pico-sdk（platform には依存しない）
smf → なし
```

- `app/`: ハード直接操作禁止。`Platform::*` と `synth` API のみ。FM モジュールの構成は `Platform::ModuleInfo` で参照し、`OpnBase` に依存しない。例外: デバッグ用（`MidiControlType::Debug*` の TL Trim トグルとデバッガコンソール）に限り `drivers/fm` の static API を直接呼ぶ。`main.cpp` は全体の組み立てとして `drivers/midi_panel` の `CreateMidiPanelDriver()` を呼ぶ。pico-sdk は時刻と stdio に限る。`app/ui/` は UI ロジックである LcdMenu（`extern/LcdMenu`）を直接扱う（[doc/display/design_display_menu.md](doc/display/design_display_menu.md)）
- `midi/`: pico-sdk・FreeRTOS・ドライバに依存しない。Single Parse Rule（Core0 のみパース）
- `smf/`: pico-sdk・FreeRTOS・ドライバに依存しない（`midi/`と同じ制約）。SMFファイルフォーマット（チャンク・VLQ・ランニングステータス・メタイベント）の解釈と、連続再生の順序決定（`PlaybackSequence`。純粋ロジック）のみ行い、実I/O（SDカードアクセス）は `platform/` 側の実装（`SmfSdByteSource`）に委ねる。詳細は [doc/smf/design_smf_player.md](doc/smf/design_smf_player.md) 参照
- `synth/` から `midi/` への依存は純粋な MIDI イベント型・Controller Action 定義に限る。`midi/` から `synth/` への逆依存は禁止
- `synth/`: FM アクセスは `drivers/fm` 経由。ハードウェア操作は行わないが、ログマクロ（`debugger.h`）に限り `app/` を include してよい。例外: `CsmVoice` は FM `/IRQ` の ISR 登録に限り `platform`/`app` に直接依存する（レイテンシ要件による）
- `platform/`: ピン割り当て・PIO 所有・初期化順を集約。GPIO は上位でハードコードしない。例外: `drivers/storage/hw_config.c` は no-OS-FatFS が要求する静的コールバック構造体のため SPI/CS ピン番号を直書きする
- `extern/`: submodule の中身は直接編集禁止（`extern/` 直下の組み込み用ファイルは対象外）。ラッパー（主に `platform`）経由で利用。例外: `app/ui/` からの LcdMenu

## ビルド・検証

```bash
git submodule update --init --recursive
cmake --preset default
ninja -C build
```

FreeRTOS-Kernel は `extern/FreeRTOS-Kernel` の submodule で、RP2350 用ポートはその中の submodule にあるため `--recursive` が必須。pico-sdk は submodule ではない。配置は [doc/build.md](doc/build.md) / [doc/build_ja.md](doc/build_ja.md) の CLI 手順を参照。`cmake --preset` には CMake 3.30+ が必要。

- デフォルトターゲット: **RP2350A**（RP2040 は CMake オプションで切替）
- 生成物: `build/FMSynthEnsembleV3.uf2`, `build/FMSynthEnsembleV3.elf`
- 変更後は可能な範囲でビルドが通ることを確認する

## 実装時の注意

- タスク優先度・スタック・Core Affinity は `src/app/task_config.h` が唯一の定義元
- FM バス用 GPIO2–15 は `opn_piolib` 専用。別 PIO プログラムを同ピンに同時有効化しない
- ビルド時の設定は 2 か所に分ける。外部ライブラリ・SDK の構成・特定ハードウェアに依存するコンポーネントの有無（構造）はルート `CMakeLists.txt` の `fmse_build_option()`、独自に設計した部分の有無と調整値は `src/config.h`。`src/config.h` はどのレイヤから include してもよい。構造オプションは生成される `build_config.h` で 0 / 1 に定義されるので、`-D` のコンパイル定義や `CMakePresets.json` に値を書かない。基準と一覧は [doc/architecture/architecture.md](doc/architecture/architecture.md) の「Build-time Switch」
- submodule を追加・更新したら `extern/submodules.lock` も同じコミットに更新する（Configure 時に照合される）。詳細は [doc/architecture/architecture.md](doc/architecture/architecture.md)
- 変更は最小スコープで。既存の命名・抽象化・コメント水準に合わせる
- コミットはユーザーが明示的に依頼したときのみ作成する
