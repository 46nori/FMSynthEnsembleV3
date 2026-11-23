# ADR 0001: 演奏イベントの Core 間転送を単一 FIFO にする

- 状態: 採用
- 日付: 2026-08-14（#PR77、Issue #75）

## 1. 背景

以前は発音が不安定だったため、Note と Effect を別キュー（`gMidiNoteQueue` / `gMidiEventQueue`）に分け、Note を優先して Effect を間引けるようにしていた。NoteOff 用の予約スロットと pending NoteOff ビットマップも持っていた。

その後、SSG の I/O Port へのアクセスが FM レジスタを壊す不具合を修正したところ、発音は大きく安定した。キュー分割が本当に必要だったのかは確かめていなかった。

## 2. 決定

| 項目 | 決定 |
|---|---|
| 演奏イベント | Core1 へ渡す `MidiEvent` は到着順の単一 FIFO（`gMidiQueue`）にする |
| 制御イベント | Reset / デバッグ / パネルは型も Producer も異なるので、`gMidiControlQueue` として分けたままにする。Reset が失われないよう `gPendingReset` のフォールバックも残す |
| Core0 フィルタ | 音源が扱わない Channel Voice は Core0 で捨て、キューに積まない |
| Note 優先・Effect 間引き | 行わない。到着順を崩さない |
| 詳細計測 | 通常の処理経路から外し、`ENABLE_MIDI_TIMING_STATS=ON` の診断ビルドだけに含める |
| 容量 | 旧 Note(128) + Event(64) の合計 192 を引き継ぐ |

旧キュー、NoteOff 予約スロット、pending NoteOff ビットマップは廃止した。

## 3. 根拠

`feature/simplified-queue` で単一 FIFO に戻し、3 曲で `ENABLE_MIDI_TIMING_STATS=ON` の `stats`（`MidiIpcGetStats`）を計測した。CSM は対象曲で発音しないので切り分けから外した。

- 3 曲とも Drop は 0、high water は 8〜16 / 192 で、キューは満杯にならなかった
- NoteOff の最大滞留は 0.4〜2.1 ms で、鳴り残りの主因にはならない
- ビブラート 1 周期の実行時間は 0.12〜0.19 ms で、数 ms の Note の遅れを説明しない
- 遅れの本体は Core1 の NoteOn 実行（1 件 0.7〜1.0 ms）だった。和音が続くと直列に積もり 3〜8 ms 遅れる。これは Note 同士の待ちなので、Note 専用キューでも同じ遅れが出る

計測時点では未対応 CC も同じ FIFO に乗っており、Effect が NoteOn の後ろで 1.6〜4.6 ms 待つなどの二次的な遅れがあった。いずれも NoteOn のコストより小さく、Core0 で未対応 Channel Voice を捨てることで解消した。

キューを分けると到着順が崩れ、振り分け・予約スロット・二重ドレインで実装が重くなる。MIDI は順序付きのストリームなので、到着順のまま渡す方が意味としても素直である。

## 4. 影響

- 遅れを減らすには、キュー構造ではなく NoteOn 1 件の処理時間（`SetProgram` と KeyOn 前後のレジスタ書き込み）を減らす必要がある。その一環として、NoteOn / Retrigger ではチャンネル内の全 Voice ではなく当該 Voice にだけピッチを適用するようにした（#PR78）
- Reset などの制御イベントは、演奏イベントのバッチ処理の後に扱う。演奏中は Reset がわずかに遅れることを許容する

## 5. 関連

- 現在のキューとタスクの構成: [design_concurrency.md](../architecture/design_concurrency.md)
- Core0 での転送判定と統計カウンタ: [design_midi_pipeline.md](../architecture/design_midi_pipeline.md)
