//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "init.h"
#include "isr.h"

#include <cstdio>

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "opn_piolib.h"
#include "YM2608.h"
#include "YM2203.h"
#include "YMF288.h"
#include "volume_controller.h"
#if BUILD_SD_CARD
#include "ff.h"
#include "f_util.h"
#include "sd_card.h"
#include "hw_config.h"  // sd_get_by_num()
#endif

namespace Platform {

namespace {

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

#if PICO_RP2350
constexpr uint32_t kFmBusClock  = 150000000u;   // PIO    clock: 150MHz (RP2350 SysClock)
#else
constexpr uint32_t kFmBusClock  = 120000000u;   // PIO    clock: 120MHz (RP2040: 120/8=15, 120/4=30 整数分周)
#endif
constexpr uint32_t kYmf288Clock =   8000000u;   // YMF288 clock:   8MHz
constexpr uint32_t kYm2608Clock =   8000000u;   // YM2608 clock:   8MHz
constexpr uint32_t kYm2203Clock =   4000000u;   // YM2203 clock:   4MHz
constexpr uint32_t kProbeClock  = kYm2203Clock; // probe at the lower of the two

// GPIO pin assignments (internal use only; FM_IRQ* are public in init.h)
//
//   bit  28    27     26   20      17      16      19      18      22   15
//       V_CLK V_DATA /IRQ /SD_SW /SDCS  SD_MISO SD_MOSI SD_CLK  /IC  /RD
//   DIR  1     1      0    0      1       0       1       1       1    1
//
//   bit  14  13  12  11  10   9   8    7   6   5   4    3   2   1   0
//        /WR CS1 CS0  A1  A0  D7  D6  D5  D4  D3  D2   D1  D0  --  --
//   DIR   1   1   1   1   1   *   *   *   *   *   *    *   *   0   0
//
//   -- : Not used by this driver
//   DIR: 0(INPUT), 1(OUTPUT), *(I/O)
//
constexpr uint kFM_D0    =  2;
constexpr uint kFM_D1    =  3;
constexpr uint kFM_D2    =  4;
constexpr uint kFM_D3    =  5;
constexpr uint kFM_D4    =  6;
constexpr uint kFM_D5    =  7;
constexpr uint kFM_D6    =  8;
constexpr uint kFM_D7    =  9;
constexpr uint kFM_A0    = 10;
constexpr uint kFM_A1    = 11;
constexpr uint kFM_CS0   = 12;
constexpr uint kFM_CS1   = 13;
constexpr uint kFM_WR    = 14;
constexpr uint kFM_RD    = 15;
constexpr uint kFM_IC    = 22;
constexpr uint kFM_IRQ   = FM_IRQ;      // isr.h

// ----------------------------------------------------------------------------
// Hardware initialization helpers (called from Initialize)
// ----------------------------------------------------------------------------

/**
 * @brief GPIO の初期化
 */
void InitGpio() {
    // Init GPIO (GPIO2-15: FM bus, GPIO22: /IC, GPIO26: /IRQ)
    gpio_init_mask(0b0000'0100'0100'0000'1111'1111'1111'1100);

    // Disable pull up/down
    constexpr uint pins[] = {kFM_D0, kFM_D1, kFM_D2, kFM_D3, 
                             kFM_D4, kFM_D5, kFM_D6, kFM_D7,
                             kFM_A0, kFM_A1, kFM_CS0, kFM_CS1,
                             kFM_WR, kFM_RD, kFM_IC,  kFM_IRQ};
    for (size_t i = 0; i < std::size(pins); i++) {
        gpio_disable_pulls(pins[i]);
    }

    // Set direction (GPIO2-15,GPIO22 OUT by default, GPIO26 /IRQ IN)
    gpio_set_dir_masked(0b0000'0100'0100'0000'1111'1111'1111'1100,
                        0b0000'0000'0100'0000'1111'1111'1111'1100);

    // CS0=L, CS1=L, /WR=H, /RD=H, /IC=H, A0=L, A1=L (FM#0 selected, bus inactive)
    gpio_put_masked(0b0000'0000'0100'0000'1111'1100'0000'0000,
                    0b0000'0000'0100'0000'1100'0000'0000'0000);
}

/**
 * @brief FM 音源 LSI のハードウェアリセット
 */
void ResetFmChip() {
    gpio_put(kFM_IC, 0);
    sleep_us(100);  // > 24us(min)@OPNA, >18us(min)@OPN
    gpio_put(kFM_IC, 1);
}

/**
 * @brief NJU72343 ボリュームコントローラ初期化
 */
void InitNJU72343() {
    VolumeController::GetInstance().InitializeEarlyMute();
}

/**
 * @brief TinyUSB MIDI デバイス初期化
 */
void InitTinyUsb() {
    tusb_init();
}

/**
 * @brief SD カード初期化・マウント
 * @details マウント状態を維持するため、FATFS ワークエリアは静的に保持する
 *          （FatFs はマウント中このオブジェクトへのポインタを内部に保持し続ける）。
 */
#if BUILD_SD_CARD
FATFS g_sd_fatfs;

/**
 * @brief f_mount()を強制マウントで（再）実行する
 * @details hw_config.cの配線にCard Detectピンがないため、カード抜去はハードウェア的に
 *          検知できない。抜去後の復帰は、実際にI/Oが
 *          失敗した時点でこの関数を呼び直すリアクティブな再マウントに委ねる。
 *
 *          sd_card_spi_init()（no-OS-FatFS-SD-SDIO-SPI-RPi-Pico）は状態フラグ
 *          STA_NOINITがクリアされたままだと「既に初期化済み」とみなしてカードの
 *          再走査そのものをスキップする。初回マウント成功後は同フラグがクリアされた
 *          ままになり、抜去後に何もこれを再セットしないため、f_mount()を単に
 *          呼び直すだけでは実際のカード再走査が行われず FR_DISK_ERR で失敗する
 *          （実機で確認）。
 *
 *          最初はsd_card_t::deinit()（sd_deinit()）を呼んでSTA_NOINITを立て直す
 *          方式を試したが、sd_deinit()はCard Selectピンをgpio_deinit()して
 *          GPIO_INに戻してしまい、それを元のGPIO_OUTへ戻す処理はsd_spi_ctor()
 *          （sd_init_driver()内で一度きり実行）にしかない。このためdeinit()を
 *          一度でも呼ぶとCSピンの向きが壊れたままになり、以降すべてのマウントが
 *          FR_NOT_READYで失敗するようになった（実機で確認、カード未抜去でも再現）。
 *          GPIOには触れず、sd_card_t::state.m_Statusへ直接STA_NOINITを立てる。
 */
bool MountSdCard() {
    sd_card_t* card = sd_get_by_num(0);
    if (card != nullptr) {
        card->state.m_Status |= STA_NOINIT;
    }

    FRESULT fr = f_mount(&g_sd_fatfs, "0:", 1);
    if (FR_OK != fr) {
        std::printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return false;
    }
    return true;
}

void InitSdCard() {
    sd_init_driver();
    MountSdCard();
}
#endif

// ----------------------------------------------------------------------------
// FM module detection helpers (called from SetupFmModules)
// ----------------------------------------------------------------------------

bool HasRespondingSsg(const fm_device_t* dev) {
    constexpr uint8_t kBitPattern0 = 0xaa;
    constexpr uint8_t kBitPattern1 = 0x55;
    write_reg(dev, 0x00, 0, kBitPattern0);
    uint8_t read0 = read_reg(dev, 0x00, 0);
    write_reg(dev, 0x00, 0, kBitPattern1);
    uint8_t read1 = read_reg(dev, 0x00, 0);
    return read0 == kBitPattern0 && read1 == kBitPattern1;
}

bool IsYmf288(const fm_device_t* dev) {
    // レジスタ 0x20 の D1(NEW)=1 を書いてネイティブモードへ切替を試みる。
    // YM2608/YM2203 では 0x20 は未定義につき書き込みは無害。
    write_reg(dev, 0x20, 0, 0x02);
    sleep_us(10);
    // デバイス ID が 0x02 に変化すれば YMF288
    constexpr int kMaxAttempts = 8;
    constexpr int kRequiredConsecutiveMatches = 2;
    int consecutive_matches = 0;
    for (int i = 0; i < kMaxAttempts; ++i) {
        if (read_reg(dev, 0xff, 0) == 0x02) {
            if (++consecutive_matches >= kRequiredConsecutiveMatches) {
                return true;
            }
        } else {
            consecutive_matches = 0;
        }
        sleep_us(10);
    }
    return false;
}

bool IsYm2608(const fm_device_t* dev) {
    constexpr int kMaxAttempts = 8;
    constexpr int kRequiredConsecutiveMatches = 2;

    int consecutive_matches = 0;
    for (int i = 0; i < kMaxAttempts; ++i) {
        if (read_reg(dev, 0xff, 0) == 0x01) {
            if (++consecutive_matches >= kRequiredConsecutiveMatches) {
                return true;
            }
        } else {
            consecutive_matches = 0;
        }
        sleep_us(10);
    }
    return false;
}

}  // namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

/**
 * @brief プラットフォーム全体の初期化
 */
void Initialize() {
    // NJU72343 ボリュームコントローラ 
    //   GPIO/FMの初期化前の不安定 mute
    InitNJU72343();

    // 標準入出力の初期化 (UART + USB)
    stdio_init_all();

    // GPIO初期化
    InitGpio();

    // FM音源LSIのリセット
    ResetFmChip();

#if BUILD_SD_CARD
    // SD カード
    InitSdCard();
#endif

    // TinyUSB MIDI デバイス
    InitTinyUsb();
}

#if BUILD_SD_CARD
/**
 * @brief SDカードの再マウントを試みる
 * @details カード抜去後の復帰用。SmfSdByteSource/ForEachSmfFile がFatFsのI/Oエラー
 *          （FR_DISK_ERR/FR_NOT_READY）を検出した際、または`mount`コマンドから呼ぶ。
 *          呼び出しはSmfPlayerTaskに一元化する（他タスクから直接呼ばない）。
 * @return 成功すればtrue
 */
bool RemountSdCard() {
    return MountSdCard();
}
#endif

/**
 * @brief FM音源モジュールの検出・初期化・インスタンス生成を行う
 * @return 初期化済みの FmSystem (unique_ptr)
 */
std::unique_ptr<FmSystem> SetupFmModules(Error* out_error) {
    if (out_error != nullptr) {
        *out_error = Error::None;
    }

    auto fs = std::make_unique<FmSystem>();
    VolumeController::DockModuleTypes dock_module_types{};

    // FMバスの初期化 (PIO0)
    if (fm_bus_init(&fs->bus, pio0, 0, kFmBusClock) != 0) {
        std::printf("FM bus init failed.\n");
        if (out_error != nullptr) {
            *out_error = Error::BusInitFailed;
        }
        return nullptr;
    }

    // 各ドックに対してYM2608/YM2203/YMF288の接続を判別
    bool has_any = false;
    for (int dock = 0; dock < static_cast<int>(fs->devices.size()); ++dock) {
        // YM2203/YM2608 両方に安全なクロックでプローブ
        fm_device_init(&fs->devices[dock], &fs->bus, dock, FM_DEVICE_YM2608, kProbeClock);

        if (!HasRespondingSsg(&fs->devices[dock])) {
            // 未接続
            fs->modules[dock] = nullptr;
            dock_module_types[dock] = VolumeController::DockModuleType::None;
            std::printf("Dock%d: None\n", dock);
            continue;
        }
        if (IsYmf288(&fs->devices[dock])) {
            // YMF288 を検出（ネイティブモード確立済み）。
            // タイミングは YM2608 と同等で安全。
            fm_device_init(&fs->devices[dock], &fs->bus, dock, FM_DEVICE_YM2608, kYmf288Clock);
            fs->module_ptr[dock] = std::make_unique<YMF288>(&fs->devices[dock], dock);
            dock_module_types[dock] = VolumeController::DockModuleType::YMF288;
            std::printf("Dock%d: YMF288\n", dock);
        } else if (IsYm2608(&fs->devices[dock])) {
            // YM2608を検出: 本来のクロックで再初期化
            fm_device_init(&fs->devices[dock], &fs->bus, dock, FM_DEVICE_YM2608, kYm2608Clock);
            fs->module_ptr[dock] = std::make_unique<YM2608>(&fs->devices[dock], dock);
            dock_module_types[dock] = VolumeController::DockModuleType::YM2608;
            std::printf("Dock%d: YM2608\n", dock);
        } else {
            // YM2203を検出: 正しいdevice typeで再初期化
            fm_device_init(&fs->devices[dock], &fs->bus, dock, FM_DEVICE_YM2203, kYm2203Clock);
            fs->module_ptr[dock] = std::make_unique<YM2203>(&fs->devices[dock], dock);
            dock_module_types[dock] = VolumeController::DockModuleType::YM2203;
            std::printf("Dock%d: YM2203\n", dock);
        }
        fs->modules[dock] = fs->module_ptr[dock].get();
        has_any = true;
    }
    VolumeController::GetInstance().SetDockModuleTypes(dock_module_types);
    if (!has_any) {
        fm_bus_deinit(&fs->bus);  // PIOプログラムとスピンロックを解放してから返す
        if (out_error != nullptr) {
            *out_error = Error::NoModuleFound;
        }
        return nullptr;
    }

    return fs;
}

}  // namespace Platform
