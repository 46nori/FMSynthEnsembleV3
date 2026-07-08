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
#include "volume_controller.h"
#if BUILD_SD_CARD
#include "ff.h"
#include "f_util.h"
#include "sd_card.h"
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
    // Init GPIO (GPIO2-14: FM bus, GPIO22: /IC, GPIO26: /IRQ)
    gpio_init_mask(0b0000'0100'0100'0000'0111'1111'1111'1100);

    // Disable pull up/down
    constexpr uint pins[] = {kFM_D0, kFM_D1, kFM_D2, kFM_D3, 
                             kFM_D4, kFM_D5, kFM_D6, kFM_D7,
                             kFM_A0, kFM_A1, kFM_CS0, kFM_CS1,
                             kFM_WR, kFM_RD, kFM_IC,  kFM_IRQ};
    for (size_t i = 0; i < std::size(pins); i++) {
        gpio_disable_pulls(pins[i]);
    }

    // Set direction (GPIO2-14,GPIO22 OUT by default, GPIO26 /IRQ IN)
    gpio_set_dir_masked(0b0000'0100'0100'0000'0111'1111'1111'1100,
                        0b0000'0000'0100'0000'0111'1111'1111'1100);

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
 */
#if BUILD_SD_CARD
void InitSdCard() {
    sd_init_driver();

    FATFS fs;
    FRESULT fr = f_mount(&fs, "0:", 1);
    if (FR_OK != fr) {
        std::printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    } else {
        // Write sample
        FIL fil;
        fr = f_open(&fil, "0:/test.txt", FA_OPEN_APPEND | FA_WRITE);
        if (FR_OK != fr) {
            std::printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        } else {
            if (f_printf(&fil, "Hello from FMSynthEnsembleV3!\n") < 0) {
                std::printf("f_printf failed\n");
            }
            fr = f_close(&fil);
            if (FR_OK != fr) {
                std::printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
            } else {
                std::printf("Wrote to 0:/test.txt\n");
            }
        }
        f_unmount("0:");
    }
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

    // 各ドックに対してYM2608/YM2203の接続を判別
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
        if (IsYm2608(&fs->devices[dock])) {
            // YM2608を検出: 本来のクロックで再初期化
            fm_device_init(&fs->devices[dock], &fs->bus, dock, FM_DEVICE_YM2608, kYm2608Clock);
            fs->module_storage[dock] = std::make_unique<YM2608>(&fs->devices[dock], dock);
            dock_module_types[dock] = VolumeController::DockModuleType::YM2608;
            std::printf("Dock%d: YM2608\n", dock);
        } else {
            // YM2203を検出: 正しいdevice typeで再初期化
            fm_device_init(&fs->devices[dock], &fs->bus, dock, FM_DEVICE_YM2203, kYm2203Clock);
            fs->module_storage[dock] = std::make_unique<YM2203>(&fs->devices[dock], dock);
            dock_module_types[dock] = VolumeController::DockModuleType::YM2203;
            std::printf("Dock%d: YM2203\n", dock);
        }
        fs->modules[dock] = fs->module_storage[dock].get();
        has_any = true;
    }
    VolumeController::GetInstance().SetDockModuleTypes(dock_module_types);
    if (!has_any) {
        if (out_error != nullptr) {
            *out_error = Error::NoModuleFound;
        }
        return nullptr;
    }

    return fs;
}

}  // namespace Platform
