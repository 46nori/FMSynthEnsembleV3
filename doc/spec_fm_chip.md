# FM 音源 LSI の機能差分と組み合わせ制約

Dock に接続する YM2608 / YM2203 / YMF288 モジュールの機能差と、最大 4 台混在時の制約。

## 1. FM 音源 LSI の機能比較

| | YM2203 (OPN) | YM2608 (OPNA) | YMF288 (OPN3-L) |
|---|:---:|:---:|:---:|
| FM ボイス | **3** | **6** | **6** |
| リズム（MIDI ch10） | — | ◯ | ◯ |
| CSM 音声合成 | ◯ | ◯ | — |
| I/O ポート（MIDI パネル接続用） | ◯ | ◯ | — |

- FM はいずれも 4 オペレータ / 1 チャンネル
- リズムはチップ内蔵 6 種（BD/SD/TOM/TOP/HH/RIM）。詳細は [design_rhythm.md](design_rhythm.md)
- CSM は CH3 を使う。詳細は [design_csm_frame.md](design_csm_frame.md)
- MIDI パネルは PortA/B を使用。詳細は [design_midi_panel.md](design_midi_panel.md) / [spec_midi_panel.md](spec_midi_panel.md)

## 2. 組み合わせの前提

- Dock は **最大 4**（#0–#3）。未接続・3 種チップの混在・自動識別可
- メロディ FM の NoteVoice 上限は **24**（`VoiceLimits::kMaxNoteVoices` = 4 × 6）。YM2203 だけでは 24 に届かない
- CSM は論理ボイス 1 個。CH3 を複数チップにまたがって使う

## 3. 必要な FM 音源 LSI

どれか 1 台でも条件を満たせば、その機能は有効になる。満たさない機能は単に使えない（起動はする）。

| 使いたい機能 | 必要な FM 音源 LSI | 無いときの動き |
|---|---|---|
| MIDI パネル | **YM2608 または YM2203** | パネル無効、MIDI 全チャンネル ON |
| リズム（MIDI ch10） | **YM2608 または YMF288** が 1 台以上 | ch10 発音しない |
| CSM 音声合成 | **YM2608 または YM2203** が 1 台以上 | 通常 FM のみ |
| FM 最大 24 音 | **YM2608 または YMF288** を中心に構成 | YM2203 のみなら最大 12 音 |

パネルを使うなら **Dock3** を YM2608 または YM2203 にする。
接続先 Dock を変えたい場合は `CreateMidiPanelDriver(modules[3])` の添え字を変更する。

## 4. CSM の ON/OFF と FM 発音数

CSM は実行時スイッチではなく、`src/app/config.h` のビルド時定数で切り替える。変更後はファームを再ビルド・書き込みする。

| 定数 | 既定 | 意味 |
|---|---|---|
| `ENABLE_CSM` | `1` | `1` で CSM 有効、`0` で無効 |
| `CSM_N_MAX` | `12` | CSM が使うオペレータ数の上限（1–16）。リザーブする CH3 の台数に効く |

無効化の例:

```c
#define ENABLE_CSM  0
```

`ENABLE_CSM == 0` のときは CSM ボイス / `CsmFrameTask` を作らず、CH3 リザーブもしない。FM はフルで NoteVoice になる。FM 音源 LSI が YMF288 のみでも同様に CSM は使えない。

`ENABLE_CSM != 0` のとき、CSM 対応チップ（YM2608 / YM2203）の **CH3 を先頭から順にリザーブ**する。YMF288 の CH は触らない。

予約台数:

```
(CSM_N_MAX - 1) / 4 + 1
```

既定 `CSM_N_MAX = 12` なら **3 台**。接続されている CSM 対応チップがそれより少なければ、その台数だけリザーブする。

リザーブされた CH3 は NoteVoice にならない。**メロディ FM の最大同時発音 = 各チップの FM ch 合計 − リザーブした CH3 の数**。CSM ボイス自体は別枠 1 個。

Note On の先取りや Note Off 即停止は同ファイルの `ENABLE_CSM_START_PREEMPT` / `ENABLE_CSM_STOP_IMMEDIATE`。詳細は [design_csm_frame.md](design_csm_frame.md)。

## 5. 構成例

「FM」はメロディ NoteVoice 数（CSM 既定オン時は CH3 リザーブ後）。リズム / パネル / CSM は機能の可否。

| 構成（4 Dock） | FM（CSM オフ） | FM（CSM 既定オン） | リズム | パネル（Dock3） | CSM |
|---|---:|---:|:---:|:---:|:---:|
| YM2608 × 4 | 24 | 21（−3） | ◯ | ◯ | ◯ |
| YMF288 × 4 | 24 | 24（リザーブなし） | ◯ | — | — |
| YM2203 × 4 | 12 | 9（−3） | — | ◯ | ◯ |
| YM2608 × 2 + YM2203 × 2 | 18 | 15（−3） | ◯ | ◯ | ◯ |
| YMF288 × 3 + YM2203 × 1 | 21 | 20（−1） | ◯ | YM2203 なら ◯ | ◯ |
| YMF288 × 3 + YM2608 × 1 | 24 | 23（−1） | ◯ | YM2608 なら ◯ | ◯ |
| YM2203 × 3 + YMF288 × 1 | 15 | 12（−3） | ◯ | YM2203 なら ◯ | ◯ |

読み方:

- 24 音を狙う → YM2608 / YMF288 を揃える。YM2203 を混ぜるたびに 3 声分減る
- パネル → Dock3 を YM2608 か YM2203 にする。YMF288 × 4 では不可
- リズム → YM2608 か YMF288 を 1 台以上
- CSM → YM2608 か YM2203 を 1 台以上。入れると、そのチップの CH3 がメロディから 1 音ずつ減る
- YMF288 × 4 は 24 音 + リズムだが、パネルも CSM も不可

## 6. 関連

| 文書 | 内容 |
|---|---|
| [spec_opn.md](spec_opn.md) | レジスタ差分・識別・タイミング |
| [system_spec.md](system_spec.md) | Dock 配線・クロック |
| [design_rhythm.md](design_rhythm.md) | MIDI ch10 / リズム割当 |
| [design_csm_frame.md](design_csm_frame.md) | CSM フレーム処理 |
| `src/app/config.h` | `ENABLE_CSM` / `CSM_N_MAX` ほか CSM 関連定数 |
| [design_midi_panel.md](design_midi_panel.md) | パネル接続（現行 Dock3） |
| [design_voice_allocation.md](design_voice_allocation.md) | NoteVoice / CSM の割当 |
