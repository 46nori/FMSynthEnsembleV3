# FM音源の仕様

FM音源LSI（YM2608/YM2203）のレジスタ仕様・バス制御・タイミング・音量制御を定義する。

- FM音源LSIをRaspberryPi PicoのGPIOに接続して制御する。接続の詳細は[RaspberryPi Picoとの接続](./system_spec.md#raspberrypi-picoとの接続)を参照。
- サポートするFM音源LSIは、YM2608(OPNA)とYM2203(OPN)。
- YM2608はYM2203の上位互換で、基本的に同じ操作でレジスタアクセスが可能。リズム部、FM CH4-6、ADPCM が追加されている。これらの追加機能はYM2608で A1=1 を使う場合に有効になる。

---

## 目次

1. [データシート](#データシート)
2. [制御信号](#制御信号)
3. [データバスコントロール](#データバスコントロール)
4. [書き込み後の待ち時間](#書き込み後の待ち時間)
5. [レジスタ設定のルール](#レジスタ設定のルール)
6. [信号のタイミング](#信号のタイミング)
7. [YM2203 / YM2608 / 未接続 自動識別アルゴリズム](#ym2203--ym2608--未接続-自動識別アルゴリズム)
8. [音量制御](#音量制御)

---

### データシート
- [YM2203(英語版)](./datasheet/YM2203_DataSheet_en.pdf)
- [YM2608(日本語版)](./datasheet/YM2608_DataSheet_jp.pdf)
- [YM2608アプリケーションマニュアル(日本語版)](./datasheet/YM2608_ApplicationManual_jp.pdf)

### 制御信号

| 信号名              | 用途                                                 |
| ---------------- | -------------------------------------------------- |
| /CS              | 入力信号。チップセレクト。0で制御が有効になる。1でデータバスがハイインピーダンスになる。 |
| /RD, /WR, A0, A1 | 入力信号。データバス(D0-D7)のコントロールを行う。                       |
| D0-D7            | 8bitの双方向性データバス。CPUとのデータのやり取りを行う。                   |
| $\phi M$         | マスタークロック。チップに実際に供給するクロック値を用いる。代表例は YM2203(OPN) の 4MHz、YM2608(OPNA) の 8MHz。    |

### データバスコントロール
アドレス指定、データのリード・ライト等のデータバスコントロールは、/CS, /RD, /WR, A0, A1で行う。コントロール方法は本節の「レジスタアドレスの割当て」「レジスタの制御モード」の表を参照。
A1が有効なのはYM2608のみ。YM2203ではA1入力を使わず、常にA1=0として扱う。

- レジスタアドレスの割当て

| A1  | アドレス範囲    | 内容         |
| --- | --------- | ---------- |
| 0   | 0x00-0x0f | SSG        |
| 0   | 0x10-0x1d | リズム        |
| 0   | 0x1e-0x20 | 未定義        |
| 0   | 0x21-0x2f | FM共通部      |
| 0   | 0x30-0xb6 | FM CH1-CH3 |
| 1   | 0x00-0x10 | ADPCM      |
| 1   | 0x11-0x2f | 未定義        |
| 1   | 0x30-0xb6 | FM CH4-CH6 |
| 1   | 0xb7-0xff | 未定義        |

- レジスタの制御モード
	- X: 値を無視 (0,1どちらでも良い)
	- XX: 値を無視 (そのアドレスでも良い)
	- 0: Low Level / 1: High Level

| /CS | /RD | /WR | A1  | A0  | アドレス範囲     | 内容                   |
| --- | --- | --- | --- | --- | ---------- | -------------------- |
| 0   | 1   | 0   | 0   | 0   | 0x00-0x2f  | SSG、FM共通部、リズムのアドレス指定 |
| 0   | 1   | 0   | 0   | 0   | 0x30-0xb6  | FMチャンネル1-3のアドレス指定    |
| 0   | 1   | 0   | 0   | 1   | 0x00-0x2f  | SSG、FM共通部、リズムのデータライト |
| 0   | 1   | 0   | 0   | 1   | 0x30-0xb6  | FMチャンネル1-3のデータライト    |
| 0   | 1   | 0   | 1   | 0   | 0x00-0x10  | ADPCM関係のアドレス指定       |
| 0   | 1   | 0   | 1   | 0   | 0x30-0xb6  | FMチャンネル4-6のアドレス指定    |
| 0   | 1   | 0   | 1   | 1   | 0x00-0x10  | ADPCM関係のデータライト       |
| 0   | 1   | 0   | 1   | 1   | 0x30-0xb6  | FMチャンネル4-6のデータライト    |
| 0   | 0   | 1   | 0   | 0   | XX         | ステータス0のデータリード        |
| 0   | 0   | 1   | 0   | 1   | 0x00-0x0f  | SSGレジスタのデータリード       |
| 0   | 0   | 1   | 0   | 1   | 0xff       | デバイス識別コードリード         |
| 0   | 0   | 1   | 1   | 0   | XX         | ステータス1のデータリード        |
| 0   | 0   | 1   | 1   | 1   | 0x08, 0x0f | ADPCM、PCMデータのリード     |
| 1   | X   | X   | X   | X   | XX         | インアクティブモード           |

#### READ/WRITE DATA (SSG部)

| アドレス | D7-D0                                                             | ビットアサイン                                                                                                       |
| ---- | ----------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| 0x00 | D7-D0: Fine Tune                                                  | Channel-A Tone Period                                                                                         |
| 0x01 | D3-D0: Coarse Tune                                                | Channel-A Tone Period                                                                                         |
| 0x02 | D7-D0: Fine Tune                                                  | Channel-B Tone Period                                                                                         |
| 0x03 | D3-D0: Coarse Tune                                                | Channel-B Tone Period                                                                                         |
| 0x04 | D7-D0: Fine Tune                                                  | Channel-C Tone Period                                                                                         |
| 0x05 | D3-D0: Coarse Tune                                                | Channel-C Tone Period                                                                                         |
| 0x06 | D4-D0: Period Control                                             | Noise Period                                                                                                  |
| 0x07 | D7: IOB IN/OUT<br>D6: IOA IN/OUT<br>D5-D3: /Noise<br>D2-D0: /Tone | IOB IN(0)/OUT(1):I/O PortBの入出力状態<br>IOA IN(0)/OUT(1):I/O PortAの入出力状態<br>/Noise:ノイズの出力チャンネル設定<br>/Tone:トーンジェネレータのCH ON/OFF |
| 0x08 | D4:M<br>D3-D0: Level                                              | Channel-A Amplitude<br>M:モード選択<br>Level:出力レベル                                                                 |
| 0x09 | D4:M<br>D3-D0: Level                                              | Channel-B Amplitude<br>M:モード選択<br>Level:出力レベル                                                                 |
| 0x0a | D4:M<br>D3-D0: Level                                              | Channel-C Amplitude<br>M:モード選択<br>Level:出力レベル                                                                 |
| 0x0b | D7-D0: Fine Tune                                                  | Envelope Period                                                                                               |
| 0x0c | D7-D0: Coarse Tune                                                | Envelope Period                                                                                               |
| 0x0d | D3: CON<br>D2: ATT<br>D1: ALT<br>D0: HLD                          | Envelope Shape Cycle                                                                                          |
| 0x0e | D7-D0: I/O PortA                                                  | I/O PortA                                                                                                     |
| 0x0f | D7-D0: I/O PortB                                                  | I/O PortB                                                                                                     |

#### WRITE DATA (リズム部)

| アドレス                                                                    | D7-D0                       | ビットアサイン                                       |
| ----------------------------------------------------------------------- | --------------------------- | --------------------------------------------- |
| 0x10                                                                    | D7: DM<br>D5-D0: RKON       | DM:Dump<br>RKON:Rhythm Key ON                 |
| 0x11                                                                    | D5-D0: RTL                  | Rhythm Total Level                            |
| 0x12                                                                    | D7-D0: TEST                 | LSIのTEST DATA<br>使用しない                        |
| 0x13-0x17                                                               | 未定義                         | 未定義                                           |
| 0x18: BD<br>0x19: SD<br>0x1a: TOP<br>0x1b: HH<br>0x1c: TOM<br>0x1d: RYM | D7: L<br>D6: R<br>D4-D0: IL | L:L CHに出力<br>R:R CHに出力<br>IL:Instrument Level |

#### WRITE DATA (FM部)

| アドレス      | D7-D0                                                                                                 | ビットアサイン                                                                                                                                                                    |
| --------- | ----------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0x21      | D7-D0: TEST                                                                                           | LSIのTEST DATA<br>使用しない。                                                                                                                                                    |
| 0x22      | D3: ON<br>D2-D0: FREQ.CONT                                                                            | ON:LFO ON<br>FREQ.CONT:LFOの周波数選択                                                                                                                                           |
| 0x24      | D7-D0: TIMER-A                                                                                        | TIMER-Aの上位8ビット                                                                                                                                                             |
| 0x25      | D1-D0: TIMER-A                                                                                        | TIMER-Aの下位2ビット                                                                                                                                                             |
| 0x26      | D7-D0: TIMER-B                                                                                        | TIMER-Bのデータ                                                                                                                                                                |
| 0x27      | D7-D6: MODE<br>D5: RESET B<br>D4: RESET A<br>D3: ENABLE B<br>D2: ENABLE A<br>D1: LOAD B<br>D0: LOAD A | MODE: 3CHのMode<br>TIMER-Bフラグのリセット<br>TIMER-Aフラグのリセット<br>TIMER-Bフラグと/IRQのイネーブル<br>TIMER-Aフラグと/IRQのイネーブル<br>TIMER-Bのスタート<br>TIMER-Aのスタート                                     |
| 0x28      | D7: SLOT4<br>D6: SLOT3<br>D5: SLOT2<br>D4: SLOT1<br>D2-D0: CH                                         | SLOT1-4: 各オペレータのKey ON/OFF<br>CH: チャンネル(0でCH1)                                                                                                                             |
| 0x29      | D7: SCH<br>D4: EN ZERO<br>D3: EN BRDY<br>D2: EN EOS<br>D1: EN TB<br>D0:EN TA                          | SCH:OPNAモード(6音同時発音)<br>EN ZERO:Enable IRQ of ZERO<br>EN BRDY:Enable IRQ of BRDY<br>EN EOS: Enable IRQ of EOS<br>EN TB:Enable IRQ of Timer-B<br>EN TA:Enable IRQ of Timer-A |
| 0x2d-0x2f | 任意の値                                                                                                  | プリスケーラー設定<br>0x2dに書き込み → FM音源:1/6, SSG音源:1/4<br>0x2fに書き込み → FM音源:1/2, SSG音源:1/1<br>0x2dと0x2eに書き込み → FM音源:1/3, SSG音源:1/2                                                    |
| 0x30-0x3e | D6-D4: DT<br>D3-D0: MULTI                                                                             | DT:Detune, MULT:Multiple<br>0x33,0x37, 0x3bのアドレスは無し                                                                                                                        |
| 0x40-0x4e | D6-D0: TL                                                                                             | TL:Total Level<br>0x43,0x47, 0x4bのアドレスは無し                                                                                                                                  |
| 0x50-0x5e | D7-D6: KS<br>D4-D0: AR                                                                                | KS:Key Scale, AR:Attack Rate<br>0x53,0x57, 0x5bのアドレスは無し                                                                                                                    |
| 0x60-0x6e | D7: AMON<br>D4-D0: DR                                                                                 | AMON:振幅変調のON/OFFを各スロット毎に行う<br>DR:Decay Rate<br>0x63,0x67, 0x6bのアドレスは無し                                                                                                     |
| 0x70-0x7e | D4-D0: SR                                                                                             | SR: Sustain Rate<br>0x73,0x77, 0x7bのアドレスは無し                                                                                                                                |
| 0x80-0x8e | D7-D4: SL<br>D3-D0: RR                                                                                | SL:Sustain Level, RR:Release Rate<br>0x83,0x87, 0x8bのアドレスは無し                                                                                                               |
| 0x90-0x9e | D3-D0: SSG-EG                                                                                         | SSG-EG:SSG-Type Envelope Control<br>0x93,0x97, 0x9bのアドレスは無し                                                                                                                |
| 0xa0      | D7-D0: F-Num 1                                                                                        | CH-1のF-Numberの下位8bit                                                                                                                                                       |
| 0xa1      | D7-D0: F-Num 1                                                                                        | CH-2のF-Numberの下位8bit                                                                                                                                                       |
| 0xa2      | D7-D0: F-Num 1                                                                                        | CH-3のF-Numberの下位8bit<br>(ノーマルモードでない場合はCH-3 SLOT4)                                                                                                                          |
| 0xa4      | D5-D3: BLOCK<br>D2-D0: F-Num 2                                                                        | BLOCK:CH-1のBLOCK<br>F-Num 2:CH-1のF-Numberの上位3bit                                                                                                                           |
| 0xa5      | D5-D3: BLOCK<br>D2-D0: F-Num 2                                                                        | BLOCK:CH-2のBLOCK<br>F-Num 2:CH-2のF-Numberの上位3bit                                                                                                                           |
| 0xa6      | D5-D3: BLOCK<br>D2-D0: F-Num 2                                                                        | BLOCK:CH-3のBLOCK <br>F-Num 2:CH-3のF-Numberの上位3bit<br>(ノーマルモードでない場合はCH-3 SLOT4)                                                                                             |
| 0xa8      | D7-D0: F-Num 1                                                                                        | CH-3 SLOT3<br>F-Numberの下位8bit                                                                                                                                              |
| 0xa9      | D7-D0: F-Num 1                                                                                        | CH-3 SLOT1<br>F-Numberの下位8bit                                                                                                                                              |
| 0xaa      | D7-D0: F-Num 1                                                                                        | CH-3 SLOT2<br>F-Numberの下位8bit                                                                                                                                              |
| 0xac      | D5-D3: BLOCK<br>D2-D0: F-Num 2                                                                        | CH-3 SLOT3<br>BLOCK<br>F-Num 2:F-Numberの上位3bit                                                                                                                             |
| 0xad      | D5-D3: BLOCK<br>D2-D0: F-Num 2                                                                        | CH-3 SLOT1<br>BLOCK<br>F-Num 2:F-Numberの上位3bit                                                                                                                             |
| 0xae      | D5-D3: BLOCK<br>D2-D0: F-Num 2                                                                        | CH-3 SLOT2<br>BLOCK<br>F-Num 2:F-Numberの上位3bit                                                                                                                             |
| 0xb0      | D5-D3: FB<br>D2-D0: CONNECT                                                                           | FB:CH-1のSelf-Feedback<br>CONNECT:CH-1のConnection                                                                                                                           |
| 0xb1      | D5-D3: FB<br>D2-D0: CONNECT                                                                           | FB:CH-2のSelf-Feedback<br>CONNECT:CH-2のConnection                                                                                                                           |
| 0xb2      | D5-D3: FB<br>D2-D0: CONNECT                                                                           | FB:CH-3のSelf-Feedback<br>CONNECT:CH-3のConnection                                                                                                                           |
| 0xb4      | D7:L<br>D6:R<br>D5-D4: AMS<br>D2-D0: PMS                                                              | CH-1の設定<br>L: L CHに出力<br>R: R CHに出力<br>AMS:Amplitude Modulation Sensitivity<br>PMS:Phase Modulation  Sensitivity                                                           |
| 0xb5      | D7:L<br>D6:R<br>D5-D4: AMS<br>D2-D0: PMS                                                              | CH-2の設定<br>L: L CHに出力<br>R: R CHに出力<br>AMS:Amplitude Modulation Sensitivity<br>PMS:Phase Modulation  Sensitivity                                                           |
| 0xb6      | D7:L<br>D6:R<br>D5-D4: AMS<br>D2-D0: PMS                                                              | CH-3の設定<br>L: L CHに出力<br>R: R CHに出力<br>AMS:Amplitude Modulation Sensitivity<br>PMS:Phase Modulation  Sensitivity                                                           |

#### WRITE DATA (ADPCM)

| アドレス | D7-D0        |
| ---- | ------------ |
| 0x00 | CONTROL 1    |
| 0x01 | CONTROL 2    |
| 0x02 | START ADR(L) |
| 0x03 | START ADR(H) |
| 0x04 | STOP ADR(L)  |
| 0x05 | STOP ADR(H)  |
| 0x06 | PRESCAL(L)   |
| 0x07 | PRESCAL(H)   |
| 0x08 | ADPCM-DATA   |
| 0x09 | DELTA-N(L)   |
| 0x0a | DELTA-N(H)   |
| 0x0b | EG CTRL      |
| 0x0c | LIMIT ADR(L) |
| 0x0d | LIMIT ADR(H) |
| 0x0e | DAC DATA     |
| 0x0f | (PCM DATA)   |
| 0x10 | FLAG CONTROL |
#### READ DATA

| アドレス | D7-D0  | ビットアサイン                                |
| ---- | ------ | -------------------------------------- |
| 任意の値 | FLAG   | Status 0 (A1=0の時)<br>Status 1 (A1=1の時) |
| 0xff | ID No. | デバイス識別コード                              |

#### 1) アドレス指定モード
データバスコントロールがこのモードの時、データバス上のデータはレジスタのアドレスを指定する。指定したアドレスは、次にアドレス指定が行われるまでは保持されている。したがって、同じアドレスのデータは連続してアクセスするような場合は、アドレス指定は最初の1回だけで良くなり、その後は不要である。

#### 2) データライトモード
アドレス指定後、バスコントロール信号を「データライトモード」にして、データバス上のデータをレジスタに書き込む。
アドレス指定、及びデータライトモードでは、書き込み終了後から次のモードに移るまでに、各音源部所定の待ち時間を設定する必要がある。これはLSIの内部において、音源部毎にデータ処理方法が違うためで、レジスタに正しくデータをセットするためには、必ず待ち時間をセットすること。
待ち時間(サイクル数)を以下のように定義すると、2つのパターンが存在する。各音源部のレジスタ書き込みの待ち時間は、[書き込み後の待ち時間](#書き込み後の待ち時間)を参照すること。
	**W1**: アドレスライト後の待ち時間
	**W2**: データライト後の待ち時間

- パターン1 (異なるアドレスのデータライトを連続して行なう)
	1. アドレス指定
	2. W1
	3. データライト
	4. W2
	5. アドレス指定
	6. W1
	7. データライト
	8. W2
	9. ...
- パターン2 (同じアドレスのデータを連続ライトする)
	1. アドレス指定
	2. W1
	3. データライト
	4. W2
	5. データライト
	6. W2
	7. データライト
	8. W2
	9. ...

#### 3) ステータスリードモード
バスコントロール信号を「ステータスリードモード」にした時、ステータスレジスタに発生するステータス情報がデータバス上に出力される。

#### 4) データリードモード
SSG音源部、及びADPCM音源部のリードが可能なレジスタのデータを「データリードモード」時にデータバス上へ出力する。
FM音源部はライトのみ。リード可能なレジスタはない。

#### 5) インアクティブモード
/CSが0の時、コントロールが有効になる。
/CSが1の時、コントロールは無効になり、データバスD0-D7はハイインピーダンスとなる。

### 書き込み後の待ち時間
サイクル数は、マスタークロック $\phi M$ のサイクル数のことである。 

- アドレスライト後の待ちサイクル(W1)

| A1  | アドレス      | 待ちサイクル数(W1) | 音源部   |
| :-: | --------- | :---------: | ----- |
| 0/1 | 0x21-0xb6 |     17      | FM    |
| 0/1 | 0x00-0x0f |      0      | SSG   |
|  0  | 0x10-0x1d |     17      | リズム   |
|  1  | 0x00-0x10 |      0      | ADPCM |

- データライト後の待ちサイクル(W2)

| A1  | アドレス      | 待ちサイクル数(W2) | 音源部   |
| :-: | --------- | :---------: | ----- |
| 0/1 | 0x21-0x9e |     83      | FM    |
| 0/1 | 0xa0-0xb6 |     47      | FM    |
|  0  | 0x00-0x0f |      0      | SSG   |
|  0  | 0x10      |     576     | リズム   |
|  0  | 0x11-0x1d |     83      | リズム   |
|  1  | 0x00-0x10 |      0      | ADPCM |

## レジスタ設定のルール

- FM関連レジスタはライトのみ。設定値のリードはできない。リードできるのはステータスレジスタだけ。
- SSG関連レジスタはライト/リード可能。
- レジスタアドレス指定後のウエイトは静的に確保するしかない。データライト後のようにステータスレジスタのBUSY(bit7)の監視では確認できない。
- オクターブと音程の設定
	- 「BLOCK/ F-Num1」というアドレス。 BLOCKの3ビットでオクターブ(0~7)を表す。音は「F-Num2+F-Num1 」の計11ビットを使って表す。
	- 必ず「BLOCK/F-Num2 」から「F-Num1」の順に、データを送ること。
- \$29 : SCH Enable
	- YM2608(OPNA)でCH4,5,6を使用するためにはSCHをセットしなければならない。デフォルトではセットされていない。
- \$28 : Key on/off
	- CH4,5,6の指定値が不連続になっている。おそらくD2の追加でOPNとの互換をとったのが原因。

| CH  | D2-D0にセットする値 |
| :-: | :----------: |
|  1  |      0       |
|  2  |      1       |
|  3  |      2       |
|  4  |    **4**     |
|  5  |    **5**     |
|  6  |    **6**     |


- $27 CH3の動作モード
	- YM2608アプリケーションマニュアルの記載が間違っている。OPNと同一仕様なので以下が正しい。

| D7 D6 | モード     |
| :---: | ------- |
|  0 0  | ノーマル    |
|  0 1  | 効果音     |
|  1 0  | CSM音声合成 |

- レジスタアドレスとチャンネル・スロットの関係
オペレータ（スロット）毎の音色パラメータとチャンネル単位に設定するデータを配置したレジスタ。各パラメータに対応するレジスタアドレスを下表に示す。バンク切り替えによるチャンネルの指定は、バスコントロール信号A1で行う。S1-S4はオペレータ(スロット)の意味。

| パラメータ    | CH1/CH4 (S1) | CH1/CH4 (S3) | CH1/CH4 (S2) | CH1/CH4 (S4) | CH2/CH5 (S1) | CH2/CH5 (S3) | CH2/CH5 (S2) | CH2/CH5 (S4) | CH3/CH6 (S1) | CH3/CH6 (S3) | CH3/CH6 (S2) | CH3/CH6 (S4) |
| -------- | :----------: | :----------: | :----------: | :----------: | :----------: | :----------: | :----------: | :----------: | :----------: | :----------: | :----------: | :----------: |
| DT/MULTI |     0x30     |     0x34     |     0x38     |     0x3c     |     0x31     |     0x35     |     0x39     |     0x3d     |     0x32     |     0x36     |     0x3a     |     0x3e     |
| TL       |     0x40     |     0x44     |     0x48     |     0x4c     |     0x41     |     0x45     |     0x49     |     0x4d     |     0x42     |     0x46     |     0x4a     |     0x4e     |
| KS/AR    |     0x50     |     0x54     |     0x58     |     0x5c     |     0x51     |     0x55     |     0x59     |     0x5d     |     0x52     |     0x56     |     0x5a     |     0x5e     |
| AMON/DR  |     0x60     |     0x64     |     0x68     |     0x6c     |     0x61     |     0x65     |     0x69     |     0x6d     |     0x62     |     0x66     |     0x6a     |     0x6e     |
| SR       |     0x70     |     0x74     |     0x78     |     0x7c     |     0x71     |     0x75     |     0x79     |     0x7d     |     0x72     |     0x76     |     0x7a     |     0x7e     |
| SL/RR    |     0x80     |     0x84     |     0x88     |     0x8c     |     0x81     |     0x85     |     0x89     |     0x8d     |     0x82     |     0x86     |     0x8a     |     0x8e     |
| SSG-EG   |     0x90     |     0x94     |     0x98     |     0x9c     |     0x91     |     0x95     |     0x99     |     0x9d     |     0x92     |     0x96     |     0x9a     |     0x9e     |

| パラメータ        | CH1/CH4 | CH2/CH5 | CH3/CH6 |
| ------------ | :-----: | :-----: | :-----: |
| F-Num1       |  0xa0   |  0xa1   |  0xa2   |
| Block/F-Num2 |  0xa4   |  0xa5   |  0xa6   |
| FB/Algorithm |  0xb0   |  0xb1   |  0xb2   |
| L/R, AMS/PMS |  0xb4   |  0xb5   |  0xb6   |

- CH3を効果音モードまたはCSM音声合成モードで使用する場合の周波数設定

| パラメータ        | CH3<br>(S1) | CH3<br>(S3) | CH3<br>(S2) | CH3<br>(S4) |
| ------------ | :---------: | :---------: | :---------: | :---------: |
| F-Num1       |    0xa9     |    0xa8     |    0xaa     |    0xa2     |
| Block/F-Num2 |    0xad     |    0xac     |    0xae     |    0xa6     |

## 信号のタイミング

### FM音源、リズム音源のアクセスの定格

| 項目             | 信号    | 記号        | 条件  | 最小  | 標準  | 最大  | 単位  |
| -------------- | ----- | --------- | --- | --- | --- | --- | --- |
| アドレスセットアップ時間   | A0,A1 | $T_{AS}$  |     | 10  |     |     | ns  |
| アドレスホールド時間     | A0,A1 | $T_{AH}$  |     | 10  |     |     | ns  |
| チップセレクトライト幅    | /CS   | $T_{CSW}$ |     | 200 |     |     | ns  |
| チップセレクトリード幅    | /CS   | $T_{CSR}$ |     | 250 |     |     | ns  |
| ライトパルス幅        | /WR   | $T_{WW}$  |     | 200 |     |     | ns  |
| ライトデータセットアップ時間 | D0-D7 | $T_{WDS}$ |     | 100 |     |     | ns  |
| ライトデータホールド時間   | D0-D7 | $T_{WDH}$ |     | 20  |     |     | ns  |
| リードパルス幅        | /RD   | $T_{RW}$  |     | 250 |     |     | ns  |
| リードデータアクセス時間   | D0-D7 | $T_{ACC}$ |     |     |     | 250 | ns  |
| リードデータホールド時間   | D0-D7 | $T_{RDH}$ |     | 10  |     |     | ns  |

#### FM部、リズム部ライトタイミング
- A0,A1
	- /CSがLレベルになる前 $T_{AS}$ 以上ホールドすること。
	- /CSがHレベルになる後 $T_{AH}$ 以上ホールドすること。
- /WR, /CS
	- /CSのパルス幅は $T_{CSW}$ 以上とする。
	- /WRのパルス幅は $T_{WW}$ 以上とする。
	- $T_{CSW}$, $T_{WW}$, $T_{WDS}$, $T_{WDH}$ は/CS, /WRのいずれかがHレベルになる時を基準とする。
	- 注意: /CSを常時Lで運用する場合は、/WRのパルス幅が実効制約となる。
- D0-D7
	- /CS, /WRのいずれかがHレベルになる時を基準として、前 $T_{WDS}$ 以上, 後 $T_{WDH}$ 以上データをホールドすること。 (SSG部、ADPCM部と仕様が異なるので注意)
#### FM部リードタイミング
- A0, A1
	- /CSがLレベルになる前 $T_{AS}$ 以上ホールドすること。
	- /CSがHレベルになる後 $T_{AH}$ 以上ホールドすること。
- /RD, /CS
	- /CSのパルス幅は $T_{CSR}$ 以上とする。
	- /RDのパルス幅は $T_{RW}$ 以上とする。
	-  $T_{CSR}$, $T_{RW}$, $T_{RDH}$ は/CS, /RDのいずれかがHレベルになる時を基準とする。
- D0-D7
	- /CS, /RDのいずれかが遅くLレベルになる時を基準として、 $T_{ACC}$ 以降にデータバスが確定するため、それ以降に読み取りを行うこと。
	- /CS, /RDのいずれかがHレベルになる時を基準として、以後 $T_{RDH}$ 以上データをホールドすること。 

### SSG音源のアクセスの定格

| 項目             | 信号    | 記号         | 条件  | 最小  | 標準  | 最大  | 単位  |
| -------------- | ----- | ---------- | --- | --- | --- | --- | --- |
| アドレスセットアップ時間   | A0    | $T_{SAS}$  |     | 10  |     |     | ns  |
| アドレスホールド時間     | A0    | $T_{SAH}$  |     | 10  |     |     | ns  |
| チップセレクトライト幅    | /CS   | $T_{SCSW}$ |     | 250 |     |     | ns  |
| チップセレクトリード幅    | /CS   | $T_{SCSR}$ |     | 400 |     |     | ns  |
| ライトパルス幅        | /WR   | $T_{SWW}$  |     | 250 |     |     | ns  |
| ライトデータセットアップ時間 | D0-D7 | $T_{SWDS}$ |     | 0   |     |     | ns  |
| ライトデータホールド時間   | D0-D7 | $T_{SWDH}$ |     | 20  |     |     | ns  |
| リードパルス幅        | /RD   | $T_{SRW}$  |     | 400 |     |     | ns  |
| リードデータアクセス時間   | D0-D7 | $T_{SACC}$ |     |     |     | 400 | ns  |
| リードデータホールド時間   | D0-D7 | $T_{SRDH}$ |     | 10  |     |     | ns  |

#### SSG部ライトタイミング
- A0
	- /CSがLレベルになる前 $T_{SAS}$ 以上ホールドすること。
	- /CSがHレベルになる後 $T_{SAH}$ 以上ホールドすること。
- /WR, /CS
	- /CSのパルス幅は $T_{SCSW}$ 以上とする。
	- /WRのパルス幅は $T_{SWW}$ 以上とする。
	- $T_{SCSW}$, $T_{SWW}$, $T_{SWDH}$ は/CS, /WRのいずれかがHレベルになる時を基準とする。
- D0-D7
	- /CS, /WRのいずれかが遅くLレベルになる時を基準として、 $T_{SWDS}$ 以上前にデータを確定させること。
	- /CS, /WRのいずれかがHレベルになる時を基準として、 $T_{SWDH}$ 以上データをホールドすること。 
#### SSG部リードタイミング
- A0
	- /CSがLレベルになる前 $T_{SAS}$ 以上Hレベルにホールドすること。
	- /CSがHレベルになった後 $T_{SAH}$ 以上Hレベルにホールドすること。
- /RD, /CS
	- /CSのパルス幅は $T_{SCSR}$ 以上とする。
	- /RDのパルス幅は $T_{SRW}$ 以上とする。
	-  $T_{SCSR}$, $T_{SRW}$, $T_{SRDH}$ は/CS, /RDのいずれかがHレベルになる時を基準とする。
- D0-D7
	- /CS, /RDのいずれかが遅くLレベルになる時を基準として、 $T_{SACC}$ 以降にデータバスが確定するため、それ以降に読み取りを行うこと。
	- /CS, /RDのいずれかがHレベルになる時を基準として、以後 $T_{SRDH}$ 以上データをホールドすること。 

### ADPCM音源のアクセスの定格

| 項目             | 信号    | 記号         | 条件  | 最小  | 標準  | 最大  | 単位  |
| -------------- | ----- | ---------- | --- | --- | --- | --- | --- |
| アドレスセットアップ時間   | A0,A1 | $T_{AAS}$  |     | 10  |     |     | ns  |
| アドレスホールド時間     | A0,A1 | $T_{AAH}$  |     | 10  |     |     | ns  |
| チップセレクトライト幅    | /CS   | $T_{ACSW}$ |     | 380 |     |     | ns  |
| チップセレクトリード幅    | /CS   | $T_{ACSR}$ |     | 380 |     |     | ns  |
| ライトパルス幅        | /WR   | $T_{AWW}$  |     | 380 |     |     | ns  |
| ライトデータセットアップ時間 | D0-D7 | $T_{AWDS}$ |     | 10  |     |     | ns  |
| ライトデータホールド時間   | D0-D7 | $T_{AWDH}$ |     | 30  |     |     | ns  |
| リードパルス幅        | /RD   | $T_{ARW}$  |     | 380 |     |     | ns  |
| リードデータアクセス時間   | D0-D7 | $T_{AACC}$ |     |     |     | 380 | ns  |
| リードデータホールド時間   | D0-D7 | $T_{ARDH}$ |     | 10  |     |     | ns  |
#### ADPCM部ライトタイミング
- A0, A1
	- /CSがLレベルになる前 $T_{AAS}$ 以上ホールドすること。
	- /CSがHレベルになる後 $T_{AAH}$ 以上ホールドすること。
- /WR, /CS
	- /CSのパルス幅は $T_{ACSW}$ 以上とする。
	- /WRのパルス幅は $T_{AWW}$ 以上とする。
	- $T_{ACSW}$, $T_{AWW}$, $T_{AWDH}$ は/CS, /WRのいずれかがHレベルになる時を基準とする。
- D0-D7
	- /CS, /WRのいずれかが遅くLレベルになる時を基準として、 $T_{AWDS}$ 以上前にデータを確定させること。
	- /CS, /WRのいずれかがHレベルになる時を基準として、 $T_{AWDH}$ 以上データをホールドすること。 
#### ADPCM部リードタイミング
- A0, A1
	- /CSがLレベルになる前 $T_{AAS}$ 以上ホールドすること。
	- /CSがHレベルになった後 $T_{AAH}$ 以上ホールドすること。
- /RD, /CS
	- /CSのパルス幅は $T_{ACSR}$ 以上とする。
	- /RDのパルス幅は $T_{ARW}$ 以上とする。
	-  $T_{ACSR}$, $T_{ARW}$, $T_{ARDH}$ は/CS, /RDのいずれかがHレベルになる時を基準とする。
- D0-D7
	- /CS, /RDのいずれかが遅くLレベルになる時を基準として、 $T_{AACC}$ 以降にデータバスが確定するため、それ以降に読み取りを行うこと。
	- /CS, /RDのいずれかがHレベルになる時を基準として、以後 $T_{ARDH}$ 以上データをホールドすること。

## YM2203 / YM2608 / 未接続 自動識別アルゴリズム

- **Step 1**: SSGレジスタへのR/Wで「何らかのOPNチップ」が存在するか確認
- **Step 2**: レジスタ `0xFF` のリードでYM2608かYM2203かを判別

### Step 1: チップ接続確認（SSG レジスタ R/W）

YM2203・YM2608 両方に存在するSSGレジスタ（Channel-A Fine Tune: `0x00`）を利用する。

```
1. SSGレジスタ 0x00 に既知のテスト値（例: 0xAA）を書き込む
2. SSGレジスタ 0x00 を読み返す
3. 書いた値と一致しない → 未接続で終了
4. 別のテスト値（例: 0x55）で念のため再確認
5. 一致した → Step 2 へ
```

SSGのR/W可能レジスタは `0x00`〜`0x0D` および `0x0E`, `0x0F`。`0x00`〜`0x0D` が安全。

### Step 2: デバイス識別コードリード

```
1. アドレスポート（A0=0, A1=0）へ 0xFF を書き込む
2. データポート（A0=1, A1=0）から値をリードする
3. 結果が 0x01 → YM2608
4. それ以外  → YM2203
```

### フローチャート

```
          スタート
              │
              ▼
   SSGレジスタ 0x00 に
   0xAA を書き込む
              │
              ▼
   SSGレジスタ 0x00 を
   読み返す
              │
        ┌─────┴─────────┐
     不一致           一致
        │               │
        ▼               ▼
    【未接続】   0x55 で再確認
                        │
                  ┌─────┴───────┐
               不一致           一致
                  │             │
                  ▼             ▼
              【未接続】   レジスタ 0xFF
                           をリード
                                │
                          ┌─────┴───────┐
                        0x01        それ以外
                          │             │
                          ▼             ▼
                      【YM2608】     【YM2203】
```

### 出典

| 根拠 | 内容 |
|------|------|
| Terumasa KODAKA, Takeshi KONO「PC-98 サウンド機能 アンドキュメントI/O」(1994〜1997) https://www.webtech.co.jp/company/doc/undocumented_mem/io_sound.txt | `FFh リード → 不定=YM2203 / 01h=YM2608` と明記 |
| MAME / ymfm 等エミュレータ実装 | `addr==0xFF` で `return 0x01` を実装、上記と一致 |

### 補足事項

- **BUSYフラグの待機**: 各レジスタ書き込み後はステータスポートのBit7（BUSY）が落ちてからリードすること
- **バス不定値の考慮**: 未接続時にStep 2の判定値が偶然 `0x01` になるリスクを、Step 1のSSG確認で排除している

## 音量制御

### FM音源

YM2608/YM2203のFM音源部は音量制御が厄介である。
OPNでは4つのオペレータにキャリアとモジュレータの役割を持たせ、その組み合わせ(アルゴリズム)で1つの音色を表現する。しかし出来上がった音全体に対しての音量を指定することができない。全体音量はキャリアオペレータの振幅が支配するため、下表のようにアルゴリズムに応じて、キャリアオペレータのTotal Level(TL)を変化させることで、音量制御を実現している。

TLは音色を決定するパラメータの一つなので、MIDI音量値をTLの絶対値として書き込むと音色テーブルの基準音量や複数キャリア間のバランスが失われる。現在の実装では、MIDIの有効音量を0.75dB単位の追加減衰量に変換し、音色テーブルに含まれる各キャリアのTLへ加算する。加算後のTLは0x7fで飽和させる。

| アルゴリズム | キャリアオペレータ |
| :----: | --------: |
|   0    |         4 |
|   1    |         4 |
|   2    |         4 |
|   3    |         4 |
|   4    |       2,4 |
|   5    |     2,3,4 |
|   6    |     2,3,4 |
|   7    |   1,2,3,4 |

#### TL

オペレータの音量指定。各bitの重みづけは、出力の最大値を0dBとした時の減衰量であることに注意。

- レジスタアドレス: 0x40-0x4e (0x43, 0x47, 0x4bは除く)
- データ幅: 7bit
  - 0x00: 0dBで最大音量
  - 0x7f: -96dBで最小音量

|         | D6  | D5  | D4  | D3  | D2  | D1  | D0   |
| ------- | --- | --- | --- | --- | --- | --- | ---- |
| 減衰量(dB) | 48  | 24  | 12  | 6   | 3   | 1.5 | 0.75 |

#### MIDI値からTL追加減衰への変換

MIDIのVolume/Expression/Velocityは0-127の線形振幅値として扱い、OPNのdB指定へ変換する。FM通常音色では、以下の有効音量を使う。

- `volume`: CC #7 Volume。未指定値は`-1`
- `expression`: CC #11 Expression。未指定値は`-1`
- `velocity`: NoteOn Velocity。未指定値は`-1`

未指定値は計算時のみ127相当として扱う。ただし`volume`、`expression`、`velocity`がすべて未指定の場合は、音色テーブルのデフォルトTLを維持するため、音量変更を行わない。

有効音量は以下で求める。除算時は整数丸めを行う。

```text
base_volume = volume     < 0 ? 127 : volume
expr        = expression < 0 ? 127 : expression
vel         = velocity   < 0 ? 127 : velocity
effective   = round(base_volume * expr / 127)
effective   = round(effective * vel / 127)
```

有効音量からTLへ加算する減衰ステップは以下で求める。

```text
effective = 0 の場合:
  attenuation_step = 127

effective > 0 の場合:
  attenuation_db   = 20 * log10(127 / effective)
  attenuation_step = round(attenuation_db / 0.75)
```

実際にOPNへ書き込むキャリアTLは以下となる。

```text
carrier_tl = min(tone_table_carrier_tl + attenuation_step, 0x7f)
```

### Rhythm音源

YM2608のRhythm音源は、RTL(Rhythm Total Level)とIL(Instrument Level)の2種類の音量体系を持つ。
FM音源と基準値(0dB)の取り方が逆であることに注意。

#### RTL

リズム音源全体の音量。

- レジスタアドレス: 0x11
- データ幅: 6bit
  - 0x00: -47.25dBで最小音量
  - 0x3f: 0dBで最大音量

|         | D5  | D4  | D3  | D2  | D1  | D0   |
| ------- | --- | --- | --- | --- | --- | ---- |
| 減衰量(dB) | 24  | 12  | 6   | 3   | 1.5 | 0.75 |

#### IL

リズム音源に含まれる六種の打楽器ごとの音量。

- レジスタアドレス: 下表を参照
- データ幅: 5bit
  - 0x00: -23.25dBで最小音量
  - 0x1f: 0dBで最大音量

|         | D4  | D3  | D2  | D1  | D0   |
| ------- | --- | --- | --- | --- | ---- |
| 減衰量(dB) | 12  | 6   | 3   | 1.5 | 0.75 |

六種の打楽器のレジスタ割り当ては以下

| レジスタアドレス | 音色      |
| -------- | ------- |
| 0x18     | バスドラム   |
| 0x19     | スネアドラム  |
| 0x1a     | トップシンバル |
| 0x1b     | ハイハット   |
| 0x1c     | タムタム    |
| 0x1d     | リムショット  |

#### MIDI値からRTL/ILへの変換

Rhythm音源では、チャンネル全体の音量をRTL、発音ごとの強さをILへ割り当てる。

- RTL: CC #7 VolumeとCC #11 Expressionから求めたチャンネル有効音量
- IL: NoteOn Velocity

FM TLとは逆に、RTL/ILは値が大きいほど大きな音量になる。MIDI値からまず減衰量を求め、レジスタの最大値から差し引く。

```text
attenuation_db   = 20 * log10(127 / midi_value)
attenuation_step = round(attenuation_db / 0.75)

RTL = 63 - min(attenuation_step, 63)
IL  = 31 - min(attenuation_step, 31)
```

`midi_value = 0`の場合は、RTL/ILとも0とする。これによりRhythm音源では、`RTL=0x00`および`IL=0x00`が最小音量、`RTL=0x3f`および`IL=0x1f`が0dB最大音量となる。

### 参考：MIDIメッセージとの対応

1. CC #7 : Volume
   指定チャンネルの基準音量を変更する。FM音源では発音中のVoiceにもリアルタイムに適用される。Rhythm音源ではRTLに反映される。

2. CC #11 : Expression
   CC #7で設定された音量を基準として相対的な音量変化を加える。実効音量は`Volume * Expression / 127`で求める。発音中のFM Voiceにもリアルタイムに適用され、Rhythm音源ではRTLに反映される。

3. NoteOn時のVelocity
   FM通常音色では、`Volume * Expression * Velocity / 127 / 127`としてキャリアTLの追加減衰に反映する。Rhythm音源ではILに反映する。

### FMとリズムの音量バランス

FM通常音色（`NoteVoice` / `NoteChannel`）とリズム音源（`RhythmChannel`）は、いずれも MIDI の Volume / Expression / Velocity を **同じ dB ステップ（0.75 dB/step）** の減衰式に変換して OPN に書き込む。それでも **同じ MIDI 設定でもリズムの方が前に出て聞こえる** ことがある。これは変換式の不整合ではなく、両者の音量体系の違いに起因する。

#### 音量差が発生しうる要因

| 観点 | FM通常音色 | リズム音源 |
| ---- | ---------- | ---------- |
| レジスタの意味 | TL は **大きいほど小さく**なる（0x00 = 0 dB） | RTL / IL は **大きいほど大きく**なる（最大値 = 0 dB） |
| 音色ごとの基準 | `tone_table_carrier_tl`（Program ごとに異なる）に MIDI 減衰を **加算** | 打楽器 6 種はハードウェア側の固定音色。Program テーブル相当の基準がない |
| チャンネル音量の適用 | `Volume * Expression * Velocity` を **1 つの有効音量**にまとめて TL 減衰へ | チャンネル音量 → **RTL**、Velocity → **IL** と **分離** |
| 追加補正 | `ENABLE_FM_TL_TRIM` 時、Program 別 `fm_tl_trim[]`（測定ベース）を加算可能 | 上記以外のシステム補正はなし（従来は RTL/IL テーブルのみ） |
| 典型例 | CC #7=100, Expression=127, Velocity=100 → 有効音量 ≈ 79 → 追加減衰 5 step 程度 | CC #7=100 → RTL ≈ 62/63（ほぼ最大）。各ヒットの IL も Velocity に応じて別途設定 |

要約すると、MIDI カーブは揃えても **FM は音色 TL + 減衰**、**リズムは RTL/IL を最大付近から下げる** 構造のため、同じ「Volume=100」でもリズム側がチップ上 0 dB 付近に張り付きやすい。さらに FM だけ `fm_tl_trim` でプログラム単位の平準化がかかる場合、リズムとの差は拡大しうる。

#### 調整ポリシー

本システムでは **リズムを基準より下げる** 方向で FM とのバランスを取る。FM 側の音色 TL や `fm_tl_trim` を一律にいじってリズムに合わせる方式は採用しない（Program ごとの測定補正と競合するため）。

1. **コンパイル時既定** — `src/app/config.h` の `RHYTHM_LEVEL_OFFSET`（step 数、1 step = 0.75 dB）。RTL と IL の両方に、テーブル参照後の値からこの step 数を **減算**する（0 で無補正、大きいほどリズムが小さくなる）。初期値は **6**（約 4.5 dB）。
2. **実行時調整** — デバッガコマンド `rmix [0-31]` で `g_rhythm_level_offset` を変更する。試聴しながら決め、納得の値を `RHYTHM_LEVEL_OFFSET` に反映してビルドし直す運用を推奨する。
3. **反映タイミング**
   - **RTL**（CC #7 / #11 相当）: `rmix` 変更時に `RhythmChannel::RefreshRhythmLevels()` で即時再適用。
   - **IL**（Velocity 相当）: 変更後の **次回 NoteOn 以降** のヒットに適用（発音中ヒットの IL は追跡しない）。
4. **FM 側の既存手段**（リズムバランスとは別目的） — Program 別レベル平準化は `fm_tl_trim` / デバッガ `trim`。アナログ出力段のバランスは `VolumeController`（NJU72343）。これらは **FM 音色の均一化** や **筐体出力** 用であり、FM–リズムの相対バランスの第一手段ではない。
5. **MIDI シーケンス側** — 演奏データのリズム ch（MIDI ch 10）の CC #7 等で下げることも可能だが、本オフセットは **ファームウェア既定のミックス** として全曲に共通適用する。

#### 調整の目安

| `RHYTHM_LEVEL_OFFSET` / `rmix` 値 | リズム側の目安 |
| -------------------------------- | -------------- |
| 0 | オフセットなし（テーブル換算のみ） |
| 6（既定） | 約 4.5 dB 下げ |
| 8 | 約 6 dB 下げ |
| 12 | 約 9 dB 下げ |

リズムがまだ前に出る場合は `rmix` を段階的に増やし、弱すぎる場合は減らす。最終値は試聴環境（ヘッドホン / スピーカー、他モジュール台数）に依存するため、固定の「正解値」は設けない。

#### 実装参照

- テーブル適用とオフセット: `src/synth/channel/RhythmChannel.cpp`（`RhythmLevelWithOffset`, `g_rhythm_level_offset`）
- 既定値: `src/app/config.h`（`RHYTHM_LEVEL_OFFSET`）
- 実行時コマンド: `src/app/debugger_task.cpp`（`rmix`）
