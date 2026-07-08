# システム仕様書

## ハードウェア

- システムコントローラは以下に対応する。
  - RaspberryPi Pico (RP2040)
  - RaspberryPi Pico2 (RP2350A)
- FM音源LSIは以下をサポートする。
  - YAMAHA YM2203 (OPN)
  - YAMAHA YM2608 (OPNA)
- MIDIインターフェースはRaspberryPi PicoのUSBインターフェースを使用する。
- microSDHCインターフェースはSPI0を使用する。
- 標準出力としてUART0を使用する。
- FM音源LSIのバス制御、電子ボリューム(NJU72343)の制御にはRaspberryPi PicoのPIOをそれぞれ使用する。

## RaspberryPi Picoとの接続

Raspberry Pi Picoに接続される外部デバイスとの結線は以下の通り。
`信号`は回路図上の信号名。`方向`はRaspberryPi Picoから見た場合の入出力。

### シリアルインターフェース

| 信号    | RPi Pico | 方向    | 論理 |　用途 |
| :-----: | :------: | :----: | :-: | --- |
|   TX    |  GPIO0 / UART0 TX | 出力 |  正  |　シリアル出力　|
|   RX    |  GPIO1 / UART0 RX | 入力 |  正  |　シリアル入力 |

### FM音源LSI

| 信号    | RPi Pico | 方向       | 論理  |　用途 |
| :-----: | :------: | :-------: | :-:  | --- |
|   D0    |  GPIO2   |    入出力  |  正  |　データバス　|
|   D1    |  GPIO3   |    入出力  |  正  |　データバス　|
|   D2    |  GPIO4   |    入出力  |  正  |　データバス　|
|   D3    |  GPIO5   |    入出力  |  正  |　データバス　|
|   D4    |  GPIO6   |    入出力  |  正  |　データバス　|
|   D5    |  GPIO7   |    入出力  |  正  |　データバス　|
|   D6    |  GPIO8   |    入出力  |  正  |　データバス　|
|   D7    |  GPIO9   |    入出力  |  正  |　データバス　|
|   A0    |  GPIO10  |    出力    |  正  | A0 |
|   A1    |  GPIO11  |    出力    |  正  | A1 |
|  CS0    |  GPIO12  |    出力    |  正  | 2to4に出力するLSI選択用のCS |
|  CS1    |  GPIO13  |    出力    |  正  | 2to4に出力するLSI選択用のCS |
|  /WR    |  GPIO14  |    出力    |  負  | ライトストローブ |
|  /RD    |  GPIO15  |    出力    |  負  | リードストローブ |
|  /IC    |  GPIO22  |    出力    |  正  | ハードウェアリセット |
|  /IRQ   |  GPIO26  |    入力    |  負  | 割り込み(全IRQのWired-OR) |

OPNA/OPNは最大4個(#0-#3)まで接続でき、GPIO12,13 の 2bit(0-3)で選択する。GPIO12,13 は74HC138(2to4デコーダとして使用)でデコードされ、#0-#3の/CSに接続される。

| CS1    | CS0    | /CS         |
| :----: | :----: | :---------: |
|   0    |   0    |   FM音源#0   |
|   0    |   1    |   FM音源#1   |
|   1    |   0    |   FM音源#2   |
|   1    |   1    |   FM音源#3   |

74HC138のイネーブル入力をGNDに固定するため、CS0/CS1の組み合わせで常にいずれかの/CSがアクティブになる。/WRおよび/RDがHighを保持している限りFM音源LSIは動作しないため、これはシステムの前提として問題ない。電源投入後の過渡状態で意図せずアクティブになる可能性があるが、初期化時に/ICでハードウェアリセットさせることで影響を回避する。

OPNA/OPNは5V電源のため、3.3VのRaspberryPiのGPIOとは直接接続できない。FXMA108などのレベル変換ICを介して接続する。このため、これらの信号のGPIO内蔵のプルアップ・プルダウン設定は無効にすること。

OPNA/OPNからの割り込み信号はオープンドレインなので、すべての/IRQのワイヤードORをとり、10Kのプルアップ抵抗を介してGPIOに接続している。GPIO内蔵のプルアップ設定ではうまく認識されないので注意。

### microSDHC

| 信号 | RPi Pico | 方向 | 論理  | 用途 |
| :-----: | :------: | :-------: | :-: | --- |
| SD_MISO |  GPIO16 / SPI0 RX  |    入力     |  正  | microSDHCのDO(DAT0) |
| /SDCS   |  GPIO17 / SPI0 CSn |    出力     |  正  | microSDHCのCS(DAT3) |
| SD_CLK  |  GPIO18 / SPI0 SCK |    出力     |  正  | microSDHCのSCLK |
| SD_MOSI |  GPIO19 / SPI0 TX  |    出力     |  正  | microSDHCのDI(CMD) |
| /SD_SW  |  GPIO20  |    入力     |  負  | microSDHCのSW |

### 電子ボリューム(NJU72343)

| 信号 | RPi Pico | 方向 | 論理  | 用途 |
| :-----: | :------: | :-------: | :-: | --- |
|  V_DATA |  GPIO27  |    出力     |  正  | NJU72343のDATA |
|  V_CLK  |  GPIO28  |    出力     |  正  | NJU72343のCLK |


## FM音源LSI

### マスタークロック

供給するマスタークロック（$\phi M$）は以下の通り。

|  供給先        | $\phi M$ |
| :-----------: | :------: |
|  YM2203(OPN)  |  4MHz    |
|  YM2608(OPNA) |  8MHz    |

16MHz発信器出力を74HC74で分周し、8MHzと4MHzを生成する。

### バス制御

バス制御では D0–D7、A0、A1、/RD、/WR、CS0、CS1 を同時に正確に操作する必要があるが、CPU の GPIO 操作では割り込み禁止にしない限り、アトミックに実行できない。Raspberry Pi Pico では PIO を使って独自の GPIO シーケンスを定義でき、PIO のステートマシン動作中は CPU の介入を受けない。これによりバスサイクル単位のアトミック性が得られ、CPU 側ではマルチタスクや割り込みを扱いやすくなる。

FM バスはソフトウェアライブラリ **`opn_piolib`**（仕様: `src/drivers/fm/opn_piolib/doc/piolib_spec.md`）で制御する。要点は次のとおり。

| 項目 | 内容 |
| --- | --- |
| PIO ブロック | 典型的に **PIO0**（[アーキテクチャ](./architecture.md) 参照） |
| ステートマシン | **1 本**（プログラム `fm_bus`）。write / read は同一 SM の別エントリで処理 |
| GPIO | 本節の FM 音源表（GPIO2–15）と一致 |
| チップ選択 | `chip_id` 0–3 が CS0/CS1 の 2bit と対応（上表の #0–#3） |
| 排他 | ハードウェアスピンロック 1 個で FIFO 投入を直列化 |

RP2040/RP2350 の PIO では、**複数の有効なステートマシンが同一 GPIO を出力駆動すると論理がビット OR で合成される**。そのため GPIO2–15 を FM バスに割り当てたあと、**別の PIO プログラムの SM を同じピンへ同時有効化してはならない**（`opn_piolib` も read 用に第 2 SM を起動しない）。

74HC138 により CS0/CS1 の組み合わせで常にいずれかの `/CS` がアクティブになる前提（上記「FM音源LSI」）に対し、`opn_piolib` はアイドル中も **CS / A0 / A1 / D を前回値で保持**し、**/WR・/RD のみ High（非アクティブ）** に保つ。/WR・/RD が High の間は LSI は動作しないため、本節の前提と整合する。実装の詳細は [piolib_spec.md](../src/drivers/fm/opn_piolib/doc/piolib_spec.md) の「設計方針」「アーキテクチャ」を参照する。

## RaspberryPi Pico

### リセット信号

以下のトリガにより、DS1232で生成する。
- パワーオン
- リセットボタン

ウォッチドッグリセットが発生しないように、/STに4MHzのクロックを供給する。

