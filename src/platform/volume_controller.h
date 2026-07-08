//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

#include "nju72343.h"

namespace Platform {

/**
 * @brief Board-specific electronic volume controller.
 * @details Owns the single NJU72343 control path and its PIO resources.
 */
class VolumeController {
public:
    enum class DockModuleType : uint8_t {
        None = 0,
        YM2203,
        YM2608,
    };

    struct VolumeValue {
        bool muted;
        int16_t db_x2;  // dB * 2. Example: -3.0dB => -6, +0.5dB => 1.
    };

    static constexpr size_t kChipCount    = 2;  // Number of NJU72343 chips
    static constexpr size_t kChannelCount = 8;  // Number of input channels(A-H)
    static constexpr size_t kDockCount    = 4;  // Number of docks(0-3)
    using VolumeTable = std::array<std::array<VolumeValue, kChannelCount>, kChipCount>;
    using DockModuleTypes = std::array<DockModuleType, kDockCount>;

    VolumeController(const VolumeController&)            = delete;
    VolumeController& operator=(const VolumeController&) = delete;
    VolumeController(VolumeController&&)                 = delete;
    VolumeController& operator=(VolumeController&&)      = delete;

    static VolumeController& GetInstance();

    /**
     * @brief Initialize the NJU72343 control interface and mute all channels.
     */
    void InitializeEarlyMute();

    /**
     * @brief Register all detected FM module types.
     */
    void SetDockModuleTypes(const DockModuleTypes& types);

    /**
     * @brief Mute FM/SSG channels while keeping unavailable inputs muted.
     */
    void MuteFmSsg();

    /**
     * @brief Mute LineMix and LineSample channels.
     */
    void MuteLineIn();

    /**
     * @brief Mute LineMix channels (G on both chips).
     */
    void MuteLineMix();

    /**
     * @brief Mute LineSample channels (H on both chips).
     */
    void MuteLineSample();

    /**
     * @brief Set all connected FM/SSG channels in dB.
     * @details Unavailable dock inputs and YM2203 FM-R inputs remain muted.
     *          Values are rounded to the nearest 0.5dB step.
     */
    void SetFmSsgVolumeDb(float db);

    /**
     * @brief Set LineMix channels (G) in dB.
     * @details LINE_MIX_L / LINE_MIX_R are set to the same dB.
     *          Values are rounded to the nearest 0.5dB step.
     */
    void SetLineMixVolumeDb(float db);

    /**
     * @brief Set LineSample channels (H) in dB.
     * @details LINE_SAMPLE_L / LINE_SAMPLE_R are set to the same dB.
     *          Values are rounded to the nearest 0.5dB step.
     */
    void SetLineSampleVolumeDb(float db);

    /**
     * @brief Set one NJU72343 channel using the raw register value.
     * @details Intended for debugger/prototyping use where the optimal value is still being explored.
     */
    void SetVolumeRaw(uint8_t chip_addr, uint8_t channel, uint8_t value);

    /**
     * @brief Enable or disable G/H Zero Cross Detection on both NJU72343 chips.
     * @details Debugger/verification use. G1/H1 selector bits in reg 0x09 are preserved.
     */
    void SetZeroCrossDetection(bool enabled);

    /**
     * @brief Get last volume values sent through this controller.
     * @details This is a shadow table, not read back from NJU72343.
     */
    const VolumeTable& GetVolumeTable() const { return volume_table_; }

private:
    VolumeController() = default;
    ~VolumeController() = default;

    void EnsureInitialized();
    void SetChannelMute(uint8_t chip_addr, uint8_t channel);
    void SetChannelVolumeDb(uint8_t chip_addr, uint8_t channel, float db);
    void UpdateShadowFromRaw(uint8_t chip_addr, uint8_t channel, uint8_t value);

    NJU72343 nju_;
    bool initialized_ = false;
    DockModuleTypes dock_module_types_{};
    VolumeTable volume_table_{};
};

}  // namespace Platform
