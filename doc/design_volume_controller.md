# 電子ボリューム制御設計仕様（VolumeController）

`Platform::VolumeController` による NJU72343 電子ボリュームの制御方針・API設計を定義する。配線・レジスタなどのハードウェア仕様は [spec_volume_controller.md](spec_volume_controller.md) を参照。実装は `src/platform/volume_controller.h/cpp`。

## 1. 制御方針
- ノイズ対策のため、未接続の入力はミュートにしなければならない。
- 未接続dockに対応するFM/SSG入力は常にミュートする。
- YM2203搭載dockのFM-R入力には、デフォルト設定でFM-Lと同一信号が供給される（[spec_volume_controller.md 1.1節](spec_volume_controller.md#11-fm音源モジュールの出力信号)参照）。FM-L・SSGと同様に音量制御の対象とする。
- YMF288搭載dockのSSG入力には、モジュール側でGNDレベルに駆動された信号が供給される（同1.1節参照）。信号として常に無音のため常にミュートする。
- 起動直後はFM音源モジュール検出前のため、まず全入力をミュートする。
- FM音源モジュール検出後、dockごとの接続状態とモジュール種別(YM2608/YM2203/YMF288/未接続)を `VolumeController` に登録し、以後の全音量設定で利用する。
- NJU72343 の入力セレクタは A1/B1/G1/H1 固定とする。`0x09` レジスタを書き込む場合は、各セレクタを入力1側に保つ（レジスタのビット定義は[spec_volume_controller.md 2.2節](spec_volume_controller.md#22-制御レジスタ-0x09入力セレクタzero-cross-detection)参照）。
- Zero Cross Detection はデフォルトONとする（理由は[3節](#3-zero-cross-detection)）。

## 2. 音量指定
- 音量は0.5dB単位で扱う。
- 有効範囲は NJU72343 の仕様（[spec_volume_controller.md 2.1節](spec_volume_controller.md#21-音量値)）に合わせて +31.5dB 〜 -95.0dB、および Mute とする。
- 公開APIではdB値を `float` で受け取り、0.5dBの倍数でない値は最寄りの0.5dBステップに丸める。
- 内部状態とデバッグ表示用のシャドウ値は、0.5dB単位を正確に保持するため `dB * 2` の整数値で保持する。
- NJU72343 の raw register 値とdBの対応は[spec_volume_controller.md 2.1節](spec_volume_controller.md#21-音量値)を参照。通常APIではdB指定を使い、raw値指定は調整・デバッグ用途に限定する。

## 3. Zero Cross Detection
- Zero Cross Detectionの機能そのものは[spec_volume_controller.md 2.2節](spec_volume_controller.md#22-制御レジスタ-0x09入力セレクタzero-cross-detection)を参照。
- 波形の振幅が大きい瞬間にゲインを切り替えると不連続が発生し、クリックノイズやポップノイズになりやすい。Zero Cross Detection をONにすることで、音量変更時のノイズ低減を期待できる。
- 本システムでは、起動時ミュート解除、全CH 0dB復帰、デバッガやLCDメニューによる音量変更を行うため、Zero Cross Detection をデフォルトONにする。

## 4. 機能
- FM/SSG系ミュート
	- 接続されているFM-L、FM-R、SSGの入力CHをミュートする。
	- 未接続dock、YMF288のSSG、その他未使用入力はミュート状態を維持する。
- LINE_IN系ミュート
	- LineMix (G) と LineSample (H) の L/R 入力CHをミュートする。
- LineMix系ミュート / LineSample系ミュート
	- G または H の L/R のみを個別にミュートする。
- 全入力ミュート
	- FM/SSG系、LINE_IN系、未接続dockに対応する入力CHを含むNJU72343の全音量CH A-Hをミュートする。
	- 起動時ポップノイズ抑止ではこの操作を使う。
- FM/SSG系全音量設定
	- 接続されているすべてのFM-L、FM-R、SSG入力CHを指定音量に設定する（YM2203のFM-R入力を含む）。
	- 未接続dockに対応する入力CH、YMF288のSSG入力、未使用入力はミュートする。
	- 呼び出し側はdockを意識せず、「接続されているFM/SSG系全体」に対して操作する。
- 個別音量設定
	- 指定した論理対象、またはNJU chip/channelに音量を設定する。
	- LCDメニューの音量調整画面から利用するため、個別チャンネルAPIを公開する。

## 5. API設計方針
- 外部向けの基本APIは「FM/SSG系ミュート」「LINE_IN系ミュート」「FM/SSG系全音量設定」を優先する。起動時の全入力ミュートは`InitializeEarlyMute()`がFM/SSG系とLINE_IN系のミュートを組み合わせて行う。
- 呼び出し側は通常dockを意識しない。dockごとの接続状態・モジュール種別は `VolumeController` が保持する。
- dockごとの接続状態・モジュール種別は、起動時検出結果を `SetDockModuleTypes()` で4 dock分まとめて登録する。
- FM/SSG系全音量設定は `SetFmSsgVolumeDb(float db)`、LineMix は `SetLineMixVolumeDb(float db)`、LineSample は `SetLineSampleVolumeDb(float db)` とする。どちらも指定値を0.5dB単位へ丸める。`MuteLineIn()` は LineMix + LineSample の合成ミュート。
- 内部実装では、物理接続表をもとにdock単位の操作へ分解してもよい。ただしdock単位APIがかえって複雑になる場合は、NJU chip/channel の接続テーブルを直接走査する実装でよい。
- 個別チャンネル単位の操作は、LCDメニューの音量調整画面（[design_display_menu.md](design_display_menu.md) 7.4節）が必要とするため公開APIとする。
	- `SetChannelVolumeDb(uint8_t chip_addr, uint8_t channel, float db)`: 指定チャンネルを0.5dB単位に丸めてdB設定する。
	- `SetChannelMute(uint8_t chip_addr, uint8_t channel)`: 指定チャンネルをミュートする。
	- `GetChannelVolume(uint8_t chip_addr, uint8_t channel) const`: 指定チャンネルのシャドウ値（`VolumeValue`）を取得する。
	- `IsChannelAvailable(uint8_t chip_addr, uint8_t channel) const`: dock未接続、またはYMF288搭載dockのSSG入力に該当するチャンネルは`false`を返す。LineMix/LineSampleは常に`true`。`SetFmSsgVolumeDb()`が使う可否判定ロジックを共通ヘルパーへ抽出し、両者で共有する（判定条件を二重化しない）。
	- `chip_addr`は`kChipAddr[]`の値、`channel`は0=A〜7=Hで、[spec_volume_controller.md 1.2節](spec_volume_controller.md#12-ミキサー基板とfm音源モジュールの信号接続)のCH表・デバッガの`vraw`コマンドと同じ単位に揃える。
	- `SetChannelVolumeDb()`は利用不可チャンネルをミュート状態に維持する。未知の`chip_addr`または範囲外の`channel`を指定した個別チャンネル操作は無視し、`GetChannelVolume()`はMute値を返す。
- 音量の有効範囲（`+31.5dB`〜`-95.0dB`、0.5dBステップ）は `kMinDb` / `kMaxDb` / `kStepDb` として公開定数にする。呼び出し側（LCDメニューなど）が範囲を独自にハードコードしないようにするため。
- NJU chip/channel raw操作（`SetVolumeRaw()`）は、最適値探索やデバッグ用途に残す。通常制御はdB単位のAPIを使う。

## 6. デバッグ用状態保持
- NJU72343 は現在値を読み戻せないため、`VolumeController` は最後に設定した音量値をシャドウ状態として保持する。
- シャドウ状態は2基のNJU72343それぞれについて、入力CH A-H の8チャンネル分を持つ。
- 保持する値は通常APIと同じ0.5dB単位、または Mute とする。raw register 値のみを保持するとデバッグ表示が読みづらいため、表示用にはdB値へ変換できる形にする。
- すべての音量変更APIは、NJUへ送信した後にシャドウ状態を更新する。
- raw操作を許可する場合も、raw値から Mute / dB 値へ変換できる範囲ではシャドウ状態を更新する。変換不能な制御レジスタ操作は音量テーブルの対象外とする。
- デバッガからは、このシャドウ状態を参照して現在の設定値一覧を表示する。表示は「実チップから読み戻した値」ではなく「最後にVolumeControllerが送信した値」であることを明記する。
- 例:

```text
CHIP_ADR0: A=0.0 B=Mute C=-6.0 D=0.0 E=0.0 F=0.0 G=Mute H=Mute (dB)
CHIP_ADR1: A=0.0 B=Mute C=Mute D=0.0 E=0.0 F=Mute G=Mute H=Mute (dB)
```

## 7. 初期化と状態保持
- `Platform::Initialize()` では、モジュール検出前に `VolumeController` を初期化し、全入力ミュートを行う。
- 初期化時に NJU72343 の制御レジスタ `0x09` を設定し、Zero Cross Detection を有効にするとともに、A/B/G/H 入力セレクタを入力1側へ固定する（ビット定義・設定値は[spec_volume_controller.md 2.2節](spec_volume_controller.md#22-制御レジスタ-0x09入力セレクタzero-cross-detection)参照）。
- `Platform::SetupFmModules()` でdockごとの検出結果が確定した後、`VolumeController` に以下の状態を登録する。
	- dock未接続
	- YM2203接続
	- YM2608接続
	- YMF288接続
- `main.cpp` のマスターボリューム復帰では、登録済みのdock状態に基づき、`SetFmSsgVolumeDb(0)` で接続されているFM/SSG系入力だけを0dBに設定し、未接続入力はミュートする。
