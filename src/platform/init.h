//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "OpnBase.h"

/**
 * @brief RaspberryPi Pico/Pico2 プラットフォーム初期化
 * @details GPIO・FM LSI・USBデバイス・ストレージなどのプラットフォーム全体を初期化する
 */
namespace Platform {

enum class Error {
    None = 0,
    BusInitFailed,
    NoModuleFound,
};

/**
 * @brief FM システム全体を保持する構造体
 * @details fm_bus_t の寿命を保証するためヒープ確保し unique_ptr で返す。
 */
struct FmSystem {
    fm_bus_t bus;
    std::array<fm_device_t, 4> devices{};
    std::array<std::unique_ptr<OpnBase>, 4> module_storage{};
    std::array<OpnBase*, 4> modules{};
};

/**
 * @brief プラットフォーム全体の初期化
 * @details
 *   - stdio (UART + USB) の初期化
 *   - GPIO 初期化・FM LSI リセット
 *   - FM バス (PIO0) 初期化
 *   - NJU72343 ボリュームコントローラ (PIO1) 初期化
 *   - SD カード (SPI0) 初期化・マウント
 *   - TinyUSB MIDI デバイス初期化
 */
void Initialize();

/**
 * @brief FM 音源モジュールの検出・バス初期化・インスタンス生成を行う
 * @param out_error 失敗時のエラー種別を書き込む (nullptr可)
 * @return 初期化済みの FmSystem (unique_ptr)
 * @note 失敗時は nullptr を返す
 */
std::unique_ptr<FmSystem> SetupFmModules(Error* out_error = nullptr);

}  // namespace Platform
