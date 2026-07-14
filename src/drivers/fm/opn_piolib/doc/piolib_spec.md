# OPN 制御用 PIO ライブラリ設計仕様書

## 1. 概要

Raspberry Pi Pico 系マイコン（RP2040 / RP2350）の PIO を用いて、YM2608 および YM2203 をマルチスレッド環境から安全に制御する C 言語ライブラリである。

### 1.1 目的

1. write / read のバス操作を PIO が自律実行する。
2. 複数スレッド・複数コアから同一共有バスへアクセスしても、トランザクション境界を破らない。
3. YM2608 / YM2203 のタイミング要件を、PIO 定格クロック範囲で満たす。
4. API 契約・初期化責務・不正引数の扱いを明確にする。

### 1.2 前提ドキュメント

- [システム仕様](../../../../../doc/system_spec.md)
- [FM 音源の仕様](../../../../../doc/spec_opn.md)

### 1.3 公開 API 一覧

#### 初期化・終了

| プロトタイプ | 役割 |
| --- | --- |
| `int fm_bus_init(fm_bus_t *bus, PIO pio, uint sm, uint32_t pio_hz)` | バス初期化。成功 `0`、スピンロック枯渇 `-1`、PIO メモリ不足 `-2`。 |
| `void fm_bus_deinit(fm_bus_t *bus)` | バス解放。SM 停止・プログラムアンロード・スピンロック返却。 |
| `int fm_device_init(fm_device_t *dev, fm_bus_t *bus, fm_chip_id_t chip_id, fm_device_type_t device_type, fm_master_clock_hz_t master_clock_hz)` | デバイス初期化。`wait_table` を前計算。 |

#### レジスタアクセス

| プロトタイプ | 役割 |
| --- | --- |
| `void write_reg(const fm_device_t *dev, uint8_t addr, uint8_t a1, uint8_t data)` | レジスタ書き込み（addr + data、W1/W2 自動）。 |
| `uint8_t read_status(const fm_device_t *dev, uint8_t a1)` | ステータス読み出し（`a1`: 0=Status0, 1=Status1）。 |
| `uint8_t read_reg(const fm_device_t *dev, uint8_t addr, uint8_t a1)` | レジスタ読み出し（W1=0 アドレス限定、[10.3 節](#103-read_reg-対象)）。 |

#### 高レベル API

| プロトタイプ | 役割 |
| --- | --- |
| `void fm_set_freq(const fm_device_t *dev, uint8_t ch, uint8_t block, uint16_t fnum)` | CH0–5 周波数を 1 回のロック内で 2 レジスタ書き込み。 |
| `void fm_set_freq_ch3(const fm_device_t *dev, uint8_t slot, uint8_t block, uint16_t fnum)` | CH3 効果音/CSM 用スロット別周波数（A1=0 固定）。 |

## 2. 設計方針

### 2.1 基本方針

- バス操作はすべて **1 本の PIO プログラム `fm_bus` と 1 つのステートマシン（SM）** で実行する。
- 共有バスは **ハードウェアスピンロック 1 個** で直列化する。
- 公開 API はスレッドコンテキスト専用とし、割り込みハンドラからは呼ばない。
- `device_type` と `master_clock_hz` は `fm_device_t` に保持し、公開 API の引数を減らす。
- 生の PIO 操作は内部プリミティブ（`fm_*_raw`）に閉じ、公開ヘッダには載せない。

### 2.2 アトミック性

| API | 保証する単位 |
| --- | --- |
| `write_reg()` | addr write + W1 + data write + W2 |
| `read_status()` | ステータス read サイクル全体 |
| `read_reg()` | addr write + データ read サイクル全体 |
| `fm_set_freq()` / `fm_set_freq_ch3()` | 2 レジスタ連続書き込み（4 FIFO ワード） |

PIO がバスサイクルを最後まで実行するため、CPU がストローブの途中へ割り込むことはない。排他はスピンロックで FIFO 投入順序を直列化する。

### 2.3 単一 SM 設計の要点

- SM は **常時有効** のまま運用する。read 時も SM を無効化したり別 SM に切り替えたりしない。
- アイドルは `main_entry` の `pull block side 3` で待機する。この間 **CS / A0 / A1 / D は前回の `out pins` 値を維持**し、ストローブのみ `/WR=H`, `/RD=H`（side 3）とする。
- read 開始は CPU が `pio_sm_exec` で `reg_entry` または `status_entry` へ分岐する。**`pio_encode_jmp()` を side_set なしで使ってはならない**（side 0 = `/WR=L`, `/RD=L` となり CS=L のままバス異常になる）。実装は `side 3` 付き jmp のみを用いる。

## 3. 対応デバイスと制約

[システム仕様](../../../../../doc/system_spec.md) の GPIO 配線・CS デコード・マスタークロックと整合する。

| 項目 | 制約 | システム仕様との対応 |
| --- | --- | --- |
| チップ | `FM_DEVICE_YM2203`, `FM_DEVICE_YM2608` | YM2203 @ 4 MHz、YM2608 @ 8 MHz |
| `chip_id` | 0–3 | CS1/CS0 の 2bit → FM 音源 #0–#3（`system_spec.md` の CS 表） |
| GPIO | `FM_GPIO_D0`–`FM_GPIO_RD`（2–15） | FM 音源 LSI 接続表と同一 |
| PIO 配置 | 呼び出し側が `pio` / `sm` を指定 | 本プロジェクトは `pio0`, SM0（`Platform::SetupFmModules`） |
| `pio_hz` | 150 MHz 以下 | — |
| `master_clock_hz` | 実機の φM。チップ種別とは独立（例: YM2608 を 4 MHz で運用も可） |
| YM2203 | `a1 == 1` 禁止 |
| `read_reg()` | SSG（A1=0, addr 0x00–0x0f）、ADPCM（A1=1, addr 0x08/0x0f）、YM2608 識別（A1=0, addr 0xff）のみ |

## 4. アーキテクチャ

### 4.1 レイヤー構成

公開 API はすべて `opn_piolib.h` に宣言される。実装はファイルごとに分かれるが、**呼び出し関係**としては高レベル API（`fm_opn.c`）が内部プリミティブ（`fm_bus.c`）を使う。

```text
[アプリケーション]
        |
[公開 API]  opn_piolib.h
  ・初期化     fm_bus_init / fm_device_init / fm_bus_deinit
  ・バス API   write_reg / read_status / read_reg
  ・高レベル   fm_set_freq / fm_set_freq_ch3
        |
   +----+----+
   |         |
[fm_opn.c]  [fm_bus.c] ──────> [fm_bus.pio]
 周波数設定    レジスタアクセス       PIO シーケンス
 (高レベル)    + fm_*_raw
   |              |
   +--------------+  fm_set_freq* は fm_write_reg_raw を呼ぶ
        |
[ハードウェア]  YM2608 / YM2203
```

| 層 | ファイル | 公開シンボル（例） | 内部のみ |
| --- | --- | --- | --- |
| ヘッダ | `opn_piolib.h` | 上記すべて | — |
| 高レベル実装 | `fm_opn.c` | `fm_set_freq*` | — |
| バス実装 | `fm_bus.c` | `write_reg`, `read_*`, `fm_bus_init` 等 | `fm_write_reg_raw`, `fm_read_*_raw` |
| PIO | `fm_bus.pio` | — | SM プログラム本体 |
| 内部ヘッダ | `fm_bus_internal.h` | — | FIFO ワード構築、スピンロック、`fm_bus_wait_write_idle` |

### 4.2 PIO プログラム構成

プログラム名: **`fm_bus`**（28 命令 / 32 上限内）

| ラベル | 用途 | FIFO |
| --- | --- | --- |
| `main_entry` | **書き込み**（`write_reg` 等）および **アイドル**（`pull block` 待ち） | TX: 2 ワード/トランザクション |
| `status_entry` | **ステータス read**（CPU が jmp で遷移） | TX: 1 ワード、RX: 1 バイト |
| `reg_entry` | **レジスタ read**（addr write + data read、CPU が jmp で遷移） | TX: 2 ワード、RX: 1 バイト |
| `read_strobe` | read 専用: `/RD` Low・サンプル | status/reg から合流 |

**書き込み**は SM が常に `main_entry` の `.wrap` ループで処理する（CPU によるエントリ切替は不要）。**読み出し**だけ CPU が `pio_sm_exec`（side 3 付き jmp）で `status_entry` または `reg_entry` へ分岐し、終了後は `read_strobe` 経由で `main_entry` に戻る。

```text
                 fm_bus (1 SM)
                      |
            +---------+---------+
            |                   |
      main_entry            (CPU jmp, side 3)
   write + idle pull              |
   (.wrap ループ)          +------+------+
            ^              |             |
            |         status_entry   reg_entry
            |         (read 1 word)  (read 2 words)
            |              |             |
            |              +------+------+
            |                     |
            |               read_strobe
            |               (/RD, in pins)
            |                     |
            +---------------------+
```

- **write 経路**: `main_entry` → addr ワード処理（`out pins` + `/WR` Low 57cyc + W 待ち）→ data ワード処理 → 再び `pull`（アイドル）。
- **read 経路**: idle 中に CPU が `status_entry` / `reg_entry` へ jmp → バス設定 → `read_strobe` → `main_entry` へ復帰。

### 4.3 バス共有モデル

- 物理バスは 1 組。全 `fm_device_t` が同一 `fm_bus_t` を共有する。
- `chip_id` ごとに CS0/CS1 パターンを FIFO ワードに埋め込む。
- 異チップ間のトランザクション順序はスピンロックで保証する。

## 5. GPIO 設計

### 5.1 ピンアサイン（`fm_bus_internal.h`）

```c
#define FM_GPIO_D0   2u    /* D0–D7: GPIO2–9 */
#define FM_GPIO_A0  10u
#define FM_GPIO_A1  11u
#define FM_GPIO_CS0 12u
#define FM_GPIO_CS1 13u
#define FM_GPIO_WR  14u
#define FM_GPIO_RD  15u
```

`opn_piolib.h` には載せない（PIO と密結合のため）。

### 5.2 信号と制御主体

| 信号 | GPIO | 制御 |
| --- | --- | --- |
| D0–D7 | 2–9 | PIO `out pins` / `in pins`；read データ相前に CPU で入力化 |
| A0, A1 | 10–11 | PIO `out pins` |
| CS0, CS1 | 12–13 | PIO `out pins` |
| /WR, /RD | 14–15 | PIO `side_set`（2 bit） |

### 5.3 side_set 割当

| side | /WR | /RD | 用途 |
| ---: | :---: | :---: | --- |
| 3 | H | H | アイドル、セットアップ、W 待ち |
| 2 | L | H | 書き込みストローブ |
| 1 | H | L | 読み出しストローブ |

## 6. データ型

公開型は `opn_piolib.h` を正とする。

```c
typedef struct fm_bus {
    PIO      pio;
    uint     sm;           /* バス用 SM 番号 */
    uint32_t pio_hz;
    uint     offset_bus;   /* pio_add_program の戻り値 */
    uint     spinlock_num;
    uint32_t init_magic;   /* 初期化成功時のみ非ゼロ */
} fm_bus_t;

typedef struct {
    fm_chip_id_t         chip_id;
    fm_device_type_t     device_type;
    fm_master_clock_hz_t master_clock_hz;
    fm_wait_table_t      wait_table;  /* w0, w17, w47, w83, w576 */
    fm_bus_t            *bus;
} fm_device_t;
```

## 7. タイミング設計

### 7.1 書き込み `/WR` Low 幅

全 write アクセスで **57 PIO サイクル** の `/WR` Low を採用する（ADPCM 380 ns 要件が最も厳しい）。

`side_set 2` 使用時、delay は 3 bit（最大 `[7]`）のため、次のループで 57 サイクルを構成する。

- `nop side 2 [7]` × 7 回 → 各 8 サイクル、計 56
- `nop side 2` × 1 回 → 1 サイクル

150 MHz 時: 57 / 150 MHz ≈ 380 ns。

`main_entry` と `reg_entry` の addr 書き込みは **同一の WR ループ**（`set x, 6` + 上記）を共有する。タイミング変更時は両方を揃えて修正する。

### 7.2 セットアップ・ホールド

- `out pins, 12` の次に `nop side 3` でデータセットアップを確保。
- `out x, 20 side 3 [2]` により `/WR` High 後のホールドを確保（5 サイクル構成）。

### 7.3 W1 / W2 換算

$$
count = \max\left(\left\lfloor N_{\phi M} \times \frac{f_{PIO}}{f_{\phi M}} \right\rfloor - 1,\ 0\right)
$$

`fm_device_init()` で `{0, 17, 47, 83, 576}` を前計算し、`write_reg()` は表引きのみ行う。

### 7.4 読み出し Tacc

| 対象 | 最小時間 | `read_*` での ns |
| --- | --- | --- |
| ステータス | 250 ns | 250 |
| SSG `read_reg` | 400 ns | 400（A1=0） |
| ADPCM `read_reg` | 380 ns | 380（A1=1） |

$$
\mathrm{Tacc\_count} = \left\lceil T_{\mathrm{min}} \times f_{PIO} / 10^9 \right\rceil
$$

実装は `fm_tacc_count()`（64 bit 整数の ceil）。

## 8. PIO プログラム `fm_bus`

正本: `src/drivers/fm/opn_piolib/src/fm_bus.pio`（pioasm 生成: `fm_bus.pio.h`）。

### 8.1 書き込み FIFO ワード（共通レイアウト）

LSB-first で `out pins, 12` に流れる。

| ビット | 内容 |
| --- | --- |
| 7:0 | D0–D7（addr または data） |
| 8 | A0 |
| 9 | A1 |
| 11:10 | CS0–CS1（= `chip_id`） |
| 31:12 | `W_count`（20 bit） |

```c
static inline uint32_t fm_make_write_word(uint8_t byte, uint8_t chip_id,
                                          uint8_t a0, uint8_t a1,
                                          uint32_t w_count);
```

### 8.2 `main_entry`（通常書き込み）

```pio
.wrap_target
    pull block          side 3
    out  pins, 12       side 3
    nop                 side 3
    set  x, 6           side 3
wr_low:
    nop                 side 2 [7]
    jmp  x--, wr_low    side 2
    nop                 side 2
    out  x, 20          side 3 [2]    ; W_count → X、/WR=H
w_loop:
    jmp  x--, w_loop    side 3
.wrap
```

`write_reg()` は addr ワード（A0=0）と data ワード（A0=1）を連続投入する。

### 8.3 ステータス read（`status_entry`）

**1 FIFO ワード**（status 専用レイアウト）。OSR 消費順: `out pindirs, 8` → `out pins, 12` → `out x, 12`。

| ビット | 内容 |
| --- | --- |
| 7:0 | D0–D7 方向マスク（0 = 入力） |
| 19:8 | A0=0, A1, CS（D=0） |
| 31:20 | `Tacc_count`（12 bit） |

```c
static inline uint32_t fm_make_read_status_word(uint8_t chip_id, uint8_t a1,
                                                uint32_t tacc_count);
```

```pio
status_entry:
    pull block          side 3
    out  pindirs, 8     side 3
    out  pins, 12       side 3
    jmp  read_strobe    side 3
```

### 8.4 レジスタ read（`reg_entry`）

**2 FIFO ワード**。

| ワード | 内容 | 構築 |
| --- | --- | --- |
| Word 1 | アドレス書き込み（A0=0） | `fm_make_read_reg_word1()` = `fm_make_write_word(addr, …, w_count=0)` |
| Word 2 | データ read 相（A0=1） | `fm_make_read_reg_word2()` = `fm_make_write_word(0, chip_id, a0=1, …, tacc)` |

Word 1 は `main_entry` と同型の `/WR` 57 サイクル。W1=0 のアドレス専用（`read_reg` 対象はすべて W1=0）。

Word 2 は **書き込みワードと同じ bits[11:0] レイアウト**で `out pins, 12` する（A0=1, CS を正しく駆動するため）。`out pindirs` は PIO では行わず、**CPU が Word 1 完了後に D0–D7 を入力化**してから Word 2 を投入する。

```pio
reg_entry:
    pull block          side 3          ; Word 1
    out  pins, 12       side 3
    nop                 side 3
    set  x, 6           side 3
reg_wr_low:
    nop                 side 2 [7]
    jmp  x--, reg_wr_low side 2
    nop                 side 2
    pull block          side 3          ; Word 2
    out  pins, 12       side 3
    jmp  read_strobe    side 3
```

**CPU 側シーケンス**（`fm_read_reg_raw`）:

1. `fm_bus_begin_read()` — idle 待ち + `reg_entry` へ side 3 jmp
2. Word 1 投入
3. `fm_bus_wait_write_idle()` — アドレス相完了（SM が Word 2 の `pull` で停止）
4. `pio_sm_set_consecutive_pindirs(D0–D7, input)`
5. Word 2 投入
6. RX 回収、`fm_bus_restore_d_output()`、idle 待ち

Word 2 の `Tacc` は bits[31:12] に格納するが、`read_strobe` の `out x, 12` は **下位 12 bit 相当**のみ X に載せる。サポートする `pio_hz` と SSG/ADPCM の Tacc 要件では十分である。

### 8.5 `read_strobe`（共通）

```pio
read_strobe:
    out  x, 12          side 1          ; /RD=L, Tacc ロード
tacc_loop:
    jmp  x--, tacc_loop side 1
    in   pins, 8        side 1
    push block          side 3          ; /RD=H
    jmp  main_entry     side 3
```

### 8.6 SM 設定

```c
sm_config_set_out_pins(&cfg, FM_GPIO_D0, 12);
sm_config_set_in_pins(&cfg, FM_GPIO_D0);
sm_config_set_sideset_pins(&cfg, FM_GPIO_WR);
sm_config_set_out_shift(&cfg, true, false, 32);   /* LSB-first */
sm_config_set_in_shift(&cfg, false, false, 8);
sm_config_set_clkdiv(&cfg, (float)clock_get_hz(clk_sys) / (float)pio_hz);
```

初期 PC: `main_entry`。FIFO: TX/RX は SDK デフォルト（`push block` / `pull block` 使用）。

## 9. 排他制御と idle 判定

### 9.1 スピンロック

- 1 バスにつきハードウェアスピンロック 1 個。
- 公開 API は `fm_bus_lock()` 取得後に raw を呼び、解放前に read 完了と D 出力復帰まで行う。

### 9.2 `fm_bus_wait_write_idle()`

`FDEBUG.TXSTALL` を使用する。TX FIFO が空かつ SM が `pull block` で停止している状態を「アイドル」とする。

- `fm_write_reg_raw()`: 投入前・完了後に呼ぶ。
- `fm_bus_begin_read()`: read 開始前に呼ぶ。
- `fm_read_reg_raw()`: Word 1 投入後、Word 2 投入前に呼ぶ（アドレス相完了の同期）。

FIFO が空でも W 待ちループ中はアイドルではないため、FIFO レベルだけの判定は使わない。

### 9.3 内部 raw プリミティブ

| 関数 | 契約 |
| --- | --- |
| `fm_write_reg_raw(bus, addr_word, data_word)` | ロック保持。前後で idle 待ち。2 ワード投入。 |
| `fm_read_status_raw(...)` | ロック保持。`fm_bus_begin_read` → 1 ワード → RX → D 復帰 → idle。 |
| `fm_read_reg_raw(...)` | ロック保持。[8.4 節](#84-レジスタ-readreg_entry)の 2 ワード + CPU pindirs 手順。 |

## 10. API 仕様

### 10.1 `fm_bus_init()`

```c
int fm_bus_init(fm_bus_t *bus, PIO pio, uint sm, uint32_t pio_hz);
```

処理概要:

1. `fm_bus_t` ゼロ初期化、`pio` / `sm` / `pio_hz` 保存。
2. スピンロック確保（失敗 `-1`）。
3. `fm_bus` プログラムを `pio_add_program`（失敗 `-2`、スピンロック返却）。
4. GPIO2–15 を SIO 初期化。**`/WR`, `/RD` のみ High**。D/A/CS は `InitGpio` 等が設定した SIO 状態を `fm_bus_capture_sio_pins()` で取り込み、PIO 移譲後に `pio_sm_set_pins_with_mask` で復元する（起動直後に CS を dock3=11 にしない）。
5. SM を `main_entry` で初期化・有効化。`init_magic` を設定して `0` を返す。

`fm_bus_deinit()` は `init_magic` 不一致時 no-op。SM 無効化・プログラム除去・スピンロック返却。

### 10.2 `write_reg()` の W1 / W2 判定

W1:

| A1 | アドレス | W1 [φM] |
| ---: | --- | ---: |
| 0 | 0x00–0x0d | 0 |
| 0 | 0x0e–0x0f | 17 |
| 1 | 0x00–0x10 | 0 |
| 0 | 0x10–0x1d | 17 |
| 0/1 | 0x21–0xb6 | 17 |

W2:

| A1 | アドレス | W2 [φM] |
| ---: | --- | ---: |
| 0 | 0x00–0x0f | 0 |
| 1 | 0x00–0x10 | 0 |
| 0 | 0x10 | 576 |
| 0 | 0x11–0x1d | 83 |
| 0/1 | 0x21–0x9e | 83 |
| 0/1 | 0xa0–0xb6 | 47 |

I/O PortA/B（A1=0, 0x0e/0x0f）はデータシート上SSG扱い（W1=W2=0）だが、実機ではアドレスライト直後（W1=0）のデータライトを行うと、直前に書き込んだFMレジスタの値（音程・音量・音色・Key on/off等、いずれも同条件で再現）が正しく反映されなくなることが確認されたため、FM相当のW1（17サイクル）を常時適用する。W2は0のままで問題ないこと、読み出しは待ち0のままで問題ないことも実機で確認済み。

### 10.3 `read_reg()` 対象

| A1 | addr | 対象 |
| --- | --- | --- |
| 0 | 0x00–0x0f | SSG |
| 1 | 0x08, 0x0f | ADPCM / PCM データ |
| 0 | 0xff | YM2608 デバイス識別 |

### 10.4 高レベル API

`fm_set_freq()`: ch 0–2 は A1=0、ch 3–5 は A1=1。Block/F-Num2 → F-Num1 の順で 4 ワードを 1 ロック内で投入。

| ch | A1 | F-Num1 | Block/F-Num2 |
| ---: | ---: | ---: | ---: |
| 0–2 | 0 | 0xa0–0xa2 | 0xa4–0xa6 |
| 3–5 | 1 | 0xa0–0xa2 | 0xa4–0xa6 |

`fm_set_freq_ch3()`: slot 0–3 → S1–S4、A1=0 固定。レジスタ表は `fm_opn.c` 参照。

## 11. 初期化の責務分界

### 11.1 本ライブラリ

- FM バス PIO / GPIO / スピンロック
- `wait_table` 前計算
- レジスタ read/write のタイミング

### 11.2 アプリケーション

- 電源・リセット・`InitGpio`（CS 初期パターン含む）
- YM2608 の SCH（CH4–6 有効化）等のチップ固有初期化
- プリスケーラ `0x2f` 等（想定 8 MHz / 4 MHz では `fm_device_init` 内では書かない）

## 12. エラー処理

### 12.1 `fm_device_init()` — 戻り値

`-1`: `chip_id > 3`、`device_type` 不正、`master_clock_hz == 0`。

### 12.2 その他公開 API — `assert`

`dev` / `bus` NULL、addr/a1 範囲、YM2203 での `a1==1`、`read_reg` の対象外 addr 等。リリースで `NDEBUG` 時は無効化され得る。

### 12.3 推奨契約（未検証の例）

- `bus == NULL` を渡さない
- `pio_hz <= 150000000`

## 13. 実装上の注意

1. W1/W2 は静的待ちで確保する。BUSY 監視での代替は FM 音源部レジスタに限り可能だが（リズム部等には使えない）、本ライブラリでは採用していない。
2. アイドル中も CS は保持されるが `/WR=H`, `/RD=H` のため誤動作しない。
3. `pio_sm_exec` は必ず side 3 付き jmp を使う（`fm_bus_encode_jmp_side3`）。
4. `read_reg` の Word 2 は write レイアウト + CPU による D 入力化が成否の前提である。
5. 割り込みハンドラからの API 呼び出し禁止（ブロッキング FIFO 待ちのため）。

## 14. まとめ

共有 FM バスは **PIO プログラム `fm_bus` 1 本と SM 1 個**、および **スピンロック 1 個**で管理する。write / read はいずれも同一 SM が GPIO2–15 を駆動し、アイドル時は CS とアドレス線を保持したままストローブのみ非アクティブにする。read は SM 切替ではなくエントリポイント分岐と FIFO ワード形式の違いで実現し、レジスタ read のデータ相では CPU と PIO が D 方向と A0/CS 駆動を分担する。
