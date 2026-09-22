# I2C接続ディスプレイ ハードウェア仕様

I2C接続のキャラクタLCDディスプレイのデバイス仕様と、Raspberry Pi Pico側のバス接続を定義する。ハードウェア全体構成は [system_spec.md](system_spec.md)、レイヤ制約は [architecture.md](architecture.md) を参照。

## 1. 目的とスコープ

対象デバイスは **ACM2004D-FLW-FBW-IIC**（20×4桁キャラクタLCD、コントローラ RW1063-0A 互換）とする。本書はデバイス仕様、GPIO割り当て、I2Cバス接続、バス動作条件を扱う。

ソフトウェア側の設計は次の文書で扱う。

- レイヤ配置・Build-time Switch: [architecture.md](architecture.md)
- 画面構成・入力・ディスプレイアダプタ: [design_display_menu.md](design_display_menu.md)

## 2. ディスプレイの仕様

出典: [ZETTLER DISPLAYS "SPECIFICATIONS FOR LIQUID CRYSTAL DISPLAY" ACM2004D-FLW-FBW-IIC VER1.1](https://akizukidenshi.com/goodsaffix/ACM2004D-FLW-FBW-IIC.pdf)

| 項目 | 内容 |
|---|---|
| 表示形式 | 20文字 × 4行 |
| コントローラIC | RW1063-0A または互換品 |
| 電源電圧 | Vdd = 5.0V 単一電源（DC-DCなし） |
| ロジック入力しきい値 | Vih ≥ 0.7×Vdd、Vil ≤ 0.6V（Vdd=5V時） |
| I2Cクロック上限 | fSCL 最大 400kHz（Standard/Fast mode、Vdd=2.7V/5V） |
| インターフェースピン | Vss, Vdd, V0, SDA, SCL, BL+, BL- |
| コマンドセット | HD44780系相当（RS/R-W/DB0-7 のビット定義。Clear Display, Entry Mode Set, Function Set 等） |

## 3. ハードウェア接続

### 3.1 GPIO割り当て

| 信号 | RPi Pico | 方向 | 用途 |
| :-: | :-: | :-: | --- |
| SDA | GPIO20（Pico物理Pin26）/ I2C0 SDA | 入出力 | ディスプレイ I2C データ |
| SCL | GPIO21（Pico物理Pin27）/ I2C0 SCL | 入出力 | ディスプレイ I2C クロック |

RP2040/RP2350のI2Cペリフェラルは4ピンおきの固定パターンでインスタンスに割り当たるため、GPIO20/21の組はハードウェアI2C0の標準ペアに一致する。

SDA/SCLはいずれもI2C仕様上オープンドレインの双方向線であり（SDAはACKビットをスレーブ側が駆動、SCLはクロックストレッチでスレーブ側がLow保持できる）、`gpio_set_function(pin, GPIO_FUNC_I2C)`によりRP2350/RP2040内蔵I2Cペリフェラルへ制御を渡すだけで、方向切り替えはハードウェア側が自動で行う。ソフトウェアで明示的に出力/入力を設定することはない。

### 3.2 I2Cプロトコル

- I2Cスレーブアドレスは **0x3F**（7bit）。
- コマンド/データの区別は、1バイトの制御バイトをペイロードの先頭に付与して行う。制御バイトは **コマンド送信時 0x00、データ送信時 0x40**。1回のI2Cトランザクションで制御バイト+データ1バイトを送信する。

### 3.3 電気的接続

- LCDモジュールはVdd=5.0V単一電源で、Vih=0.7×Vdd（5V時で約3.5V）。Raspberry Pi PicoのGPIOは3.3V系のため、そのままでは接続できない。3.3V/5V間のレベル変換が必要。
- 試作では**TXS0108E**を使用しているが、最終的には**PCA9306**（NXP、プルアップ非内蔵）を採用予定。以下の実測はTXS0108Eでの試作結果であり、参考情報として扱う。

プルアップ抵抗値の目安として、LCDモジュールのデータシートが規定するバス容量`Cb`（最大400pF）と立ち上がり時間`tr`（最大300ns）から、I2C-busスペック（NXP UM10204）の一般式`tr ≈ 0.8473 × Rp × Cb`と一般的な下限条件（Iol=3mA, Vol=0.4V）を用いて次の値を計算した。

| fSCL | Rp上限（tr制約、Cb=400pF） | Rp下限（3.3V側、Iol=3mA） | Rp下限（5V側、Iol=3mA） |
|---|---|---|---|
| 400kHz（tr_max=300ns） | 約880Ω | 約970Ω | 約1.5kΩ |
| 100kHz（tr_max=1000ns） | 約2.9kΩ | 約970Ω | 約1.5kΩ |

100kHzなら約1.5kΩ〜2.9kΩの範囲が成立し、この計算に基づき2.2kΩを3.3V側・5V側双方に外付けした。SDA/SCLのIol/Vol等、RW1063のI2Cポート固有の駆動能力はデータシートに記載がなく、一般値による概算である点に注意。

実機で試したところ、この2.2kΩのプルアップを付けるとI2C通信がNAK（応答なし）となり、外すと通信できた（`INIT OK`表示・I2Cプローブとも正常）。Pico内蔵プルアップの有無は結果に影響しなかった。理論計算とは逆の結果であり、この計算はTXS0108E経由の本構成には当てはまらなかったことになる。

原因は未確定。TXS0108Eの内部プルアップとの干渉、またはACM2004D-FLW-FBW-IIC側に既にプルアップが実装されている、のいずれか（あるいは両方）が考えられる。後者だとすると、プルアップを内蔵しないPCA9306に切り替えた場合はこの理論計算がそのまま有効になり、5V側の外付けプルアップは不要になる可能性が高い。ただし採用ICの内部構造によって理論値と実機の挙動が食い違うことがあるため、いずれにしても実機での確認は必要。

現行の試作構成（TXS0108E）では外付けプルアップ抵抗を追加していない。GPIO内蔵プルアップも無効のままとしている（`src/platform/display.cpp`の`InitializeI2cBus()`）。

### 3.4 コントラスト調整（V0）

- V0に外部のボリュームにより5Vを分圧した電圧を印加することで、液晶のコントラスト調整ができることになっている。しかし、基板上の抵抗R1がデフォルトで未実装なので、有効にするにはR1をショートさせる必要がある。

## 4. バス動作条件

- **本設計ではfSCLを400kHz（Fast-mode、LCDモジュールの規定上限）とする。** 3.3節の外付けプルアップなし構成のまま実機で400kHz動作を確認済み。

## 5. ソフトウェアとの境界

I2Cバス（I2C0）とディスプレイの所有・初期化は `platform/display.h/cpp` が担い、上位には表示APIだけを公開する。RW1063-0A互換コマンドは `drivers/display/` が扱う。詳細は [architecture.md](architecture.md) と [design_display_menu.md](design_display_menu.md) を参照。
