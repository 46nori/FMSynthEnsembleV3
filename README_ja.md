# FMSynthEnsembleV3

[![Build](https://github.com/46nori/FMSynthEnsembleV3/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/46nori/FMSynthEnsembleV3/actions/workflows/build.yml)

**[English version is here](README.md)**

![](./doc/image/FMSynthEnsembleV3.jpeg)

## 特徴

YAMAHAのOPN系FM音源LSIを使ったUSB MIDIシンセサイザ

- **FM音源LSI**
  - YAMAHA **YM2608（OPNA） / YM2203(OPN) / YMF288(OPN3-L)**
    - [最大4基（混在可・自動識別）](doc/spec_fm_chip.md)
  - CSM音声合成対応（YM2608 / YM2203）
- **MIDI**
  - [MIDIインプリメンテーションチャート](./doc/midi_implementation_chart.md)
  - **16 チャンネル** マルチティンバー、チャンネルごとのON/OFF
  - **最大24音同時発音**

- **システムコントローラ**
  - **Raspberry Pi Pico**（RP2040 / RP2350A）
  - Programmable I/O（PIO）によるFM音源LSIへの高速バスアクセス
  - FreeRTOSによるマルチコア(SMP)活用
- **オーディオ**
  - LINE OUT
  - LINE IN(ADPCM用・アンチエイリアスフィルタあり)
  - 電子ボリューム/LPF付きオーディオミキサー
- **モジュール形式のハードウェア**

## ドキュメント

- [ドキュメント一覧](./doc/README.md)
- [ファームウェアのビルドと書き込み方法](doc/build_ja.md)
- [回路図](./doc/schematics/README.md)

## ギャラリー


### モジュール

<table>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/YM2608_Module.jpeg"><img src="doc/image/YM2608_Module.jpeg" width="280" alt="OPNA モジュール"></a><br>
      <b>YM2608モジュール</b><br>
      <sub>YM2608B + YM3016<br>MIDI Panel接続コネクタ</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/YM2203_Module.jpeg"><img src="doc/image/YM2203_Module.jpeg" width="280" alt="YM2203 モジュール"></a><br>
      <b>YM2203モジュール</b><br>
      <sub>YM2203 + YM3014B<br>MIDI Panel接続コネクタ</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/YMF288_Module.jpeg"><img src="doc/image/YMF288_Module.jpeg" width="280" alt="YMF288 モジュール"></a><br>
      <b>YMF288モジュール</b><br>
      <sub>YMF288 + BU9480F</sub>
    </td>
  </tr>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/Main_Module.jpeg"><img src="doc/image/Main_Module.jpeg" width="280" alt="コントローラモジュール"></a><br>
      <b>コントローラモジュール</b><br>
      <sub>Raspberry Pi Pico2<br>FM音源モジュール用コネクタ(裏面)</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/AudioMixer_Module.jpeg"><img src="doc/image/AudioMixer_Module.jpeg" width="280" alt="ミキサーモジュール"></a><br>
      <b>ミキサーモジュール</b><br>
      <sub>FM音源モジュール出力の<br>オーディオミックス</sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/Power_Module.jpeg"><img src="doc/image/Power_Module.jpeg" width="280" alt="電源モジュール"></a><br>
      <b>電源モジュール</b><br>
      <sub>+6V, 2A入力<br>+5V / ±12V 出力</sub>
    </td>
  </tr>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/MIDIPanel.jpeg"><img src="doc/image/MIDIPanel.jpeg" width="280" alt="MIDI パネル"></a><br>
      <b>MIDIパネルモジュール</b><br>
      <sub>16CH ON/OFF + LED<br>YM2608/YM2203モジュールへ接続</sub>
    </td>
    <td></td>
    <td></td>
  </tr>
</table>

### モジュール接続

<table>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/StackedModules.jpeg"><img src="doc/image/StackedModules.jpeg" width="280" alt="スタック上面"></a><br>
      <b>スタック</b><br>
      <sub>FM音源モジュールを4枚まで接続可能<br></sub>
    </td>
    <td align="center" width="33%">
      <a href="doc/image/BackPlane.jpeg"><img src="doc/image/BackPlane.jpeg" width="280" alt="Dock接続"></a><br>
      <b>Dock 接続</b><br>
      <sub>コントローラモジュールの裏面</sub>
    </td>
  </tr>
  <tr>
    <td align="center" width="33%">
      <a href="doc/image/ConnectedModules.jpeg"><img src="doc/image/ConnectedModules.jpeg" width="280" alt="モジュール接続"></a><br>
      <b>全体接続</b><br>
      <sub>電源・コントローラ・<br>FM音源・ミキサー</sub>
    </td>
  </tr>
</table>
