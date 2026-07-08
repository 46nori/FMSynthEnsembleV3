# 電子ボリューム制御設計仕様（VolumeController）

`Platform::VolumeController` による NJU72343 電子ボリュームの制御設計を定義する。実装は `src/platform/volume_controller.h/cpp`。

## 1. オーディオ信号の接続仕様

### 1.1 FM音源モジュールの出力信号
FM音源モジュールは2種類あり、それぞれの出力信号は以下の通り。

| FM音源モジュール | FM-L(FM音源) | FM-R(FM音源) | SSG(SSG音源) |
|:---------:|:----------:|:----------:|:----------:|
| YM2608    | あり         | あり         | あり         |
| YM2203    | あり         | **なし**     | あり         |

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
YM2203の場合はFM-Rは未接続となることに注意。

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

> Note: NJU72343 の G/H はそれぞれ G1/G2、H1/H2 の2入力セレクタを持つが、このシステムでは常に G1/H1 を選択して使う。G2/H2 はセレクタで切り離されるため、VolumeController のミュート対象や接続表には含めない。

## 2. NJU72343の設定

### 2.1 共通制約
- ノイズ対策のため、未接続の入力はミュートにしなければならない。
- 未接続dockに対応するFM/SSG入力は常にミュートする。
- YM2203搭載dockのFM-R入力は未接続として扱い、常にミュートする。
- 起動直後はFM音源モジュール検出前のため、まず全入力をミュートする。
- FM音源モジュール検出後、dockごとの接続状態とモジュール種別(YM2608/YM2203/未接続)を `VolumeController` に登録し、以後の全音量設定で利用する。
- NJU72343 の入力セレクタは G1/H1 固定とする。`0x09` レジスタを書き込む場合は、G/H セレクタを G1/H1 に保つ。
- Zero Cross Detection はデフォルトONとする。

### 2.2 音量指定
- 音量は0.5dB単位で扱う。
- 有効範囲は NJU72343 の仕様に合わせて +31.5dB 〜 -95.0dB、および Mute とする。
- 公開APIではdB値を `float` で受け取り、0.5dBの倍数でない値は最寄りの0.5dBステップに丸める。
- 内部状態とデバッグ表示用のシャドウ値は、0.5dB単位を正確に保持するため `dB * 2` の整数値で保持する。
- NJU72343 の raw register 値は `0x00`/`0xff` が Mute、`0x40` が 0dB である。通常APIではdB指定を使い、raw値指定は調整・デバッグ用途に限定する。

### 2.3 Zero Cross Detection
- Zero Cross Detection は、音量変更やミュート切り替えを音声波形が0V付近を通過するタイミングで反映する機能である。
- 波形の振幅が大きい瞬間にゲインを切り替えると不連続が発生し、クリックノイズやポップノイズになりやすい。Zero Cross Detection をONにすることで、音量変更時のノイズ低減を期待できる。
- 本システムでは、起動時ミュート解除、全CH 0dB復帰、デバッガや将来のMIDIコマンドによる音量変更を想定し、Zero Cross Detection をデフォルトONにする。

### 2.4 機能
- FM/SSG系ミュート
	- 接続されているFM-L、FM-R、SSGの入力CHをミュートする。
	- 未接続dock、YM2203のFM-R、その他未使用入力はミュート状態を維持する。
- LINE_IN系ミュート
	- LineMix (G) と LineSample (H) の L/R 入力CHをミュートする。
- LineMix系ミュート / LineSample系ミュート
	- G または H の L/R のみを個別にミュートする。
- 全入力ミュート
	- FM/SSG系、LINE_IN系、未接続dockに対応する入力CHを含むNJU72343の全音量CH A-Hをミュートする。
	- 起動時ポップノイズ抑止ではこの操作を使う。
- FM/SSG系全音量設定
	- 接続されているすべてのFM-L、FM-R、SSG入力CHを指定音量に設定する。
	- 未接続dockに対応する入力CH、YM2203のFM-R入力、未使用入力はミュートする。
	- 呼び出し側はdockを意識せず、「接続されているFM/SSG系全体」に対して操作する。
- 個別音量設定
	- 指定した論理対象、またはNJU chip/channelに音量を設定する。
	- 現時点では外部公開APIとしてのユースケースはないため、まずは内部実装またはデバッグ用に留める。

### 2.5 API設計方針
- 外部向けの基本APIは「全入力ミュート」「FM/SSG系ミュート」「LINE_IN系ミュート」「FM/SSG系全音量設定」を優先する。
- 呼び出し側は通常dockを意識しない。dockごとの接続状態・モジュール種別は `VolumeController` が保持する。
- dockごとの接続状態・モジュール種別は、起動時検出結果を `SetDockModuleTypes()` で4 dock分まとめて登録する。
- FM/SSG系全音量設定は `SetFmSsgVolumeDb(float db)`、LineMix は `SetLineMixVolumeDb(float db)`、LineSample は `SetLineSampleVolumeDb(float db)` とする。どちらも指定値を0.5dB単位へ丸める。`MuteLineIn()` は LineMix + LineSample の合成ミュート。
- 内部実装では、物理接続表をもとにdock単位の操作へ分解してもよい。ただしdock単位APIがかえって複雑になる場合は、NJU chip/channel の接続テーブルを直接走査する実装でよい。
- 将来、dock単位の音量調整が必要になった場合は、内部関数を公開APIへ昇格する。
- NJU chip/channel raw操作は、最適値探索やデバッグには有用である。ただし通常制御の整合性を崩しやすいため、初期実装では公開必須とはせず、必要になった時点でデバッグ用APIとして追加する。

### 2.6 デバッグ用状態保持
- NJU72343 は現在値を読み戻せないため、`VolumeController` は最後に設定した音量値をシャドウ状態として保持する。
- シャドウ状態は2基のNJU72343それぞれについて、入力CH A-H の8チャンネル分を持つ。
- 保持する値は通常APIと同じ0.5dB単位、または Mute とする。raw register 値のみを保持するとデバッグ表示が読みづらいため、表示用にはdB値へ変換できる形にする。
- すべての音量変更APIは、NJUへ送信した後にシャドウ状態を更新する。
- raw操作を許可する場合も、raw値から Mute / dB 値へ変換できる範囲ではシャドウ状態を更新する。変換不能な制御レジスタ操作は音量テーブルの対象外とする。
- デバッガからは、このシャドウ状態を参照して現在の設定値一覧を表示する。表示は「実チップから読み戻した値」ではなく「最後にVolumeControllerが送信した値」であることを明記する。
- 例:

```text
CHIP_ADR0: A=0.0dB B=Mute C=-6.0dB D=0.0dB E=0.0dB F=0.0dB G=Mute H=Mute
CHIP_ADR1: A=0.0dB B=Mute C=Mute  D=0.0dB E=0.0dB F=Mute  G=Mute H=Mute
```

### 2.7 初期化と状態保持
- `Platform::Initialize()` では、モジュール検出前に `VolumeController` を初期化し、全入力ミュートを行う。
- 初期化時に NJU72343 の制御レジスタ `0x09` を設定し、Zero Cross Detection を有効にするとともに、G/H 入力セレクタを G1/H1 固定にする。
	- `D5=0`: G1 選択
	- `D4=0`: G出力は InG1/InG2 系を使用
	- `D3=0`: H1 選択
	- `D2=0`: H出力は InH1/InH2 系を使用
	- `D0=1`: Zero Cross Detection ON
	- したがって通常設定値は `0x01` とする。
- `Platform::SetupFmModules()` でdockごとの検出結果が確定した後、`VolumeController` に以下の状態を登録する。
	- dock未接続
	- YM2203接続
	- YM2608接続
- `main.cpp` のマスターボリューム復帰では、登録済みのdock状態に基づき、`SetFmSsgVolumeDb(0)` で接続されているFM/SSG系入力だけを0dBに設定し、未接続入力はミュートする。
