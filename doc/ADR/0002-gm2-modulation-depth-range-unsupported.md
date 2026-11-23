# ADR 0002: GM2 RPN 0,0,5（Modulation Depth Range）をサポートしない

- 状態: 採用
- 日付: 2026-08-18

## 1. 背景

ビブラートの深さを指定する MIDI メッセージには、2 つの流儀がある。

| 流儀 | メッセージ | 値の意味 |
|---|---|---|
| XG | NRPN 1:9（Vibrato Depth） | 64 を中心とした相対値。音色プリセットの深さに対する増減 |
| GM2 | RPN 0,0,5（Modulation Depth Range） | CC#1 の最大値で掛かる深さを、半音 + セントの絶対値で指定 |

このシンセは音色プリセットにビブラートの深さを持たず、CC#1 と NRPN 1:9 を同じ `vbdepth` に格納して、固定レンジ（0〜`VIBRATO_DEPTH_MAX_CENTS`）の線形セントに変換している。

## 2. 決定

GM2 RPN 0,0,5 は受信しても無視する（RPN は 0,0,0 Pitch Bend Sensitivity と 0,0,2 Coarse Tune だけを扱う）。ビブラートの深さは XG 準拠の固定レンジ・相対値モデルに一本化する。

## 3. 根拠

- 絶対値モデル（GM2）と相対値モデル（XG）を混ぜると、どちらのメッセージを後に受けたかで深さの意味が変わり、`vbdepth` 1 つでは表せない
- ビブラート遅延（NRPN 1:10）も XG と同じ相対値の変換式を使っており、XG 側にそろえる方が一貫する

## 4. 影響

- GM2 向けに作られた曲で RPN 0,0,5 を使っている場合、意図した深さにならない
- MIDI 実装チャートでは「非サポート」と記載する

## 5. 関連

- ビブラートの深さと遅延の設計: [design_pitch_effect.md](../synth/design_pitch_effect.md)
- 受信メッセージの一覧: [midi_implementation_chart.md](../midi_implementation_chart.md)
