# GM Tone Table Generator

[Wohlstand/libOPNMIDI](https://github.com/Wohlstand/libOPNMIDI) 同梱の [`fm_banks/fmmidi.wopn`](https://github.com/Wohlstand/libOPNMIDI/tree/master/fm_banks) から、General MIDI Level 1 相当の `tone_table.inc` を生成する。

入力ファイル（WOPN 形式）の FM 音色データを、本システム用の 29 バイト配列 `fm_tone_table[][29]` に変換して書き出す。

## 構成

```text
tools/gen_tone_table/          ← ここで作業・実行
  generate_tone_table.py       生成スクリプト
  README.md                    このファイル

../../extern/libOPNMIDI/       入力（clone 先、git 管理外）
../../src/drivers/fm/tone/
  tone_table.inc               出力
```

## 使い方

```sh
cd tools/gen_tone_table

# 初回のみ: libOPNMIDI を extern/ に clone
git clone https://github.com/Wohlstand/libOPNMIDI.git ../../extern/libOPNMIDI

# 生成
python3 generate_tone_table.py
```

## データ変換

1. 入力は `fmmidi.wopn` の **標準バンク**（MSB=0, LSB=0）だけを使う
2. プログラム番号 **N** の FM パラメータ → 出力 `tone_table.inc` の **N 行目**
3. 128 個すべて、番号どおりにそのまま写す

例: プログラム 0 → `Acoustic Grand Piano` の 29 バイト OPN 値、`fmmidi.wopn` の 0 番音色から取得。

`fmmidi.wopn` は fmmidi 由来の GM セットを OPNA（YM2608）向けに並べたデータ（libOPNMIDI の `utils/fmmidi_prog/` で生成される）。

**`gm.wopn`を使わない理由:** `gm.wopn` の中身は `xg.wopn` と同一でXG 向けの部分セット。GM 128 音色を番号順に並べたものは `fmmidi.wopn` のみ。

## Tone Table のフォーマット

1 プログラム = 29 バイト（OPN レジスタ値）

| オフセット | レジスタ | 内容 |
|-----------|---------|------|
| 0–3 | 0x30 | DT/MULTI |
| 4–7 | 0x40 | TL |
| 8–11 | 0x50 | KS/AR |
| 12–15 | 0x60 | DR（AMS ビット含む） |
| 16–19 | 0x70 | SR |
| 20–23 | 0x80 | SL/RR |
| 24–27 | 0x90 | SSG-EG |
| 28 | 0xb0 | FB/Algorithm |

## ライセンス

生成される `tone_table.inc` の先頭には、音色データの出所と BSD 3-Clause ライセンス全文がコメントとして付与される。

| 段階 | ソース | ライセンス |
|------|--------|-----------|
| 原始データ | [supercatexpert/fmmidi `program.h`](https://github.com/supercatexpert/fmmidi/blob/master/program.h) | [BSD 3-Clause](https://github.com/supercatexpert/fmmidi/blob/master/license.txt) (Copyright 2003–2006 yuno) |
| WOPN バンク | [libOPNMIDI `fm_banks/fmmidi.wopn`](https://github.com/Wohlstand/libOPNMIDI/blob/master/fm_banks/fmmidi.wopn) | 同上（[`fmmidi-readme.txt`](https://github.com/Wohlstand/libOPNMIDI/blob/master/fm_banks/fmmidi-readme.txt) 参照） |
| 本出力 | `tone_table.inc` | 同上（変換のみ、ライセンス条件を満たすため著作権表示・免責条項を保持） |

## 参考

- [libOPNMIDI / fm_banks](https://github.com/Wohlstand/libOPNMIDI/tree/master/fm_banks)
- [WOPN specification](https://github.com/Wohlstand/libOPNMIDI/blob/master/docs/wopn%20specification.txt)
- [General MIDI - Wikipedia](https://ja.wikipedia.org/wiki/General_MIDI)
