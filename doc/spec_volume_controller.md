# 電子ボリューム ハードウェア仕様（NJU72343）

NJU72343 電子ボリュームの配線・レジスタ仕様を定義する。GPIO/PIO割り当ては[system_spec.md](system_spec.md#電子ボリュームnju72343)、`Platform::VolumeController` による制御方針・API設計は [design_volume_controller.md](design_volume_controller.md) を参照。実装は `src/platform/volume_controller.h/cpp`。

## 1. オーディオ信号の接続仕様

### 1.1 FM音源モジュールの出力信号
FM音源モジュールは3種類あり、それぞれの出力信号は以下の通り。

| FM音源モジュール | FM-L(FM音源) | FM-R(FM音源) | SSG(SSG音源) |
|:---------:|:----------:|:----------:|:----------:|
| YM2608    | あり         | あり         | あり         |
| YM2203    | あり         | あり(※3)     | あり         |
| YMF288    | あり(※1)     | あり(※1)     | **なし**(※2) |

- (※1) YMF288はFM・リズム・SSGをチップ内部でディジタルミックスし、外付けD/Aコンバータ（BU9480F）を介してFM-L/FM-Rの1系統ステレオ出力のみを持つ（[spec_opn.md](./spec_opn.md)参照）。
- (※2) YMF288はSSG単独の出力を持たない。モジュール側でSSG出力ピンをGNDレベルで駆動しており、ミキサー入力からはオープン（未接続）ではなくGNDレベルの信号として供給される。
- (※3) YM2203モジュールはFM-R出力を、FM-Lと共通の信号にするかGNDレベルにするかをモジュール上で静的に切り替えられる。本システムはFM-Lと共通信号側（デフォルト）を前提とする。

### 1.2 ミキサー基板とFM音源モジュールの信号接続
ミキサー基板には電子ボリュームLSI(NJU72343)が2基搭載されており、dockに接続されたFM音源モジュールとの接続は以下の通り。NJU72343の指定は、アドレス`NJU72343::CHIP_ADR0` あるいは `NJU72343::CHIP_ADR1`で行う。

#### NJU72343::CHIP_ADR0

| 入力CH |    信号     | dock |
| :--: | :-------: | :--: |
|  A   |    SSG    |  0   |
|  B   |    SSG    |  1   |
|  C   |   FM-L    |  0   |
|  D   |   FM-L    |  2   |
|  E   |   FM-L    |  1   |
|  F   |   FM-L    |  3   |
|  G   | LINE_MIX_L (G1) | N/A  |
|  H   | LINE_SAMPLE_L (H1) | N/A  |
#### NJU72343::CHIP_ADR1
YM2203の場合はFM-R入力にFM-Lと同一信号が供給されることに注意（デフォルト設定、1.1節※3参照）。YMF288の場合はCHIP_ADR0/ADR1双方のSSG入力（A/B）にGNDレベルの信号が供給されることに注意（1.1節※2参照）。

| 入力CH |    信号     | dock |
| :--: | :-------: | :--: |
|  A   |    SSG    |  2   |
|  B   |    SSG    |  3   |
|  C   |   FM-R    |  0   |
|  D   |   FM-R    |  2   |
|  E   |   FM-R    |  1   |
|  F   |   FM-R    |  3   |
|  G   | LINE_MIX_R (G1) | N/A  |
|  H   | LINE_SAMPLE_R (H1) | N/A  |

> Note: NJU72343 の A/B/G/H はそれぞれ A1/A2、B1/B2、G1/G2、H1/H2 の2入力セレクタを持つ。本システムはセレクタを常に A1/B1/G1/H1 に固定する（設定方法は[2.2節](#22-制御レジスタ-0x09入力セレクタzero-cross-detection)参照）ため、A2/B2/G2/H2 は電気的に切り離され、上表には A1/B1/G1/H1 のみを示す。

## 2. NJU72343 音量制御レジスタ

### 2.1 音量値
- raw register 値と実際の音量の対応は次の通り。
  - `0x00` / `0xff`: Mute
  - `0x40`: 0dB
- 有効範囲は `+31.5dB` 〜 `-95.0dB`（0.5dBステップ）で、NJU72343チップの仕様値である。

### 2.2 制御レジスタ 0x09（入力セレクタ、Zero Cross Detection）
Zero Cross Detection は、音量変更やミュート切り替えを音声波形が0V付近を通過するタイミングで反映する、NJU72343の機能である。

制御レジスタ `0x09` の各ビットは次の通り。

| ビット | 意味 |
|:--:|---|
| D7 | A1/A2 選択（`0`=A1） |
| D6 | B1/B2 選択（`0`=B1） |
| D5 | G1/G2 選択（`0`=G1） |
| D4 | G出力のソース（`0`=InG1/InG2系を使用） |
| D3 | H1/H2 選択（`0`=H1） |
| D2 | H出力のソース（`0`=InH1/InH2系を使用） |
| D1 | Don't Care（`0`を書き込む） |
| D0 | Zero Cross Detection（`1`=ON） |

A1/B1/G1/H1固定・Zero Cross Detection ONの設定値は `0x01` になる（D7〜D1=0、D0=1）。この設定を使う運用方針は [design_volume_controller.md](design_volume_controller.md) を参照。
