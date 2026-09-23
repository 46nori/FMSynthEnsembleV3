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
        YMF288,
    };

    struct VolumeValue {
        bool muted;
        int16_t db_x2;  // dB * 2. Example: -3.0dB => -6, +0.5dB => 1.
    };

    static constexpr size_t kChipCount    = 2;  // Number of NJU72343 chips
    static constexpr size_t kChannelCount = 8;  // Number of input channels(A-H)
    static constexpr size_t kDockCount    = 4;  // Number of docks(0-3)

    // Volume range, per the NJU72343 chip spec.
    static constexpr float kMinDb  = -95.0f;
    static constexpr float kMaxDb  = 31.5f;
    static constexpr float kStepDb = 0.5f;

    /**
     * @brief NJU72343 chip addresses, indexed by chip index (0/1).
     * @details 2-wire serial addresses (`CHIP_ADR0` / `CHIP_ADR1`). Exposed so
     *          callers (debugger) can select a chip without depending on
     *          `extern/NJU72343-library` directly.
     */
    static constexpr std::array<uint8_t, kChipCount> kChipAddr = {NJU72343::CHIP_ADR0,
                                                                  NJU72343::CHIP_ADR1};
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
     * @details Unavailable dock inputs and YMF288 SSG inputs remain muted.
     *          YMF288 SSG is driven to GND level on the module itself (not an
     *          open mixer input), so muting it merely avoids amplifying a
     *          known-silent signal. YM2203 FM-R defaults to carrying the same
     *          signal as FM-L (module-configurable to GND) and is treated as
     *          available like FM-L.
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
     * @brief Set one NJU72343 channel in dB.
     * @details Used both internally (group operations) and directly by callers that need
     *          per-channel control (e.g. the LCD Volume screen). Values are rounded to the
     *          nearest 0.5dB step and clamped to [kMinDb, kMaxDb]. Unavailable channels
     *          remain muted. Unknown chip addresses and out-of-range channels are ignored.
     */
    void SetChannelVolumeDb(uint8_t chip_addr, uint8_t channel, float db);

    /**
     * @brief Mute one NJU72343 channel.
     * @details Unknown chip addresses and out-of-range channels are ignored.
     */
    void SetChannelMute(uint8_t chip_addr, uint8_t channel);

    /**
     * @brief Get the last volume value sent for one channel.
     * @details This is a shadow value, not read back from NJU72343. Returns a muted value
     *          for an unknown chip address or out-of-range channel.
     */
    VolumeValue GetChannelVolume(uint8_t chip_addr, uint8_t channel) const;

    /**
     * @brief Whether a channel currently carries a real signal.
     * @details `false` for an unconnected dock's FM/SSG inputs, or a YMF288 dock's SSG input
     *          (driven to GND on the module itself, not an open mixer input).
     *          LineMix/LineSample are always `true`. Returns `false` for an unknown chip/channel.
     */
    bool IsChannelAvailable(uint8_t chip_addr, uint8_t channel) const;

    /**
     * @brief Set one NJU72343 channel using the raw register value.
     * @details Intended for debugger/prototyping use where the optimal value is still being explored.
     *          Unknown chip addresses and out-of-range channels are ignored.
     */
    void SetVolumeRaw(uint8_t chip_addr, uint8_t channel, uint8_t value);

    /**
     * @brief Enable or disable Zero Cross Detection on both NJU72343 chips.
     * @details Debugger/verification use. The A1/B1/G1/H1 selector bits in reg 0x09 are
     *          kept fixed to their input-1 side.
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
    void UpdateShadowFromRaw(uint8_t chip_addr, uint8_t channel, uint8_t value);

    NJU72343 nju_;
    bool initialized_ = false;
    DockModuleTypes dock_module_types_{};
    VolumeTable volume_table_{};
};

}  // namespace Platform
