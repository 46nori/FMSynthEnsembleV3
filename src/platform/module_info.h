//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <array>
#include <cstdint>

namespace Platform {

/**
 * @brief Dock に検出した FM 音源モジュールの種類
 */
enum class ModuleType : uint8_t {
    None = 0,  ///< 未接続
    YM2203,
    YM2608,
    YMF288,
};

/**
 * @brief Dock ごとのモジュール情報（読み取り専用）
 * @details 上位レイヤ（app）が FM ドライバに依存せずに構成を参照するための情報。
 *          SetupFmModules() が検出時に埋め、以後は変わらない。
 */
struct ModuleInfo {
    ModuleType type = ModuleType::None;
    bool has_rhythm  = false;  ///< リズム音源を持つ（MIDI ch10 を発音できる）
};

using ModuleInfoTable = std::array<ModuleInfo, 4>;

}  // namespace Platform
