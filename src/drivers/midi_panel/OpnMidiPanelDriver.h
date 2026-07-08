//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

#include "IMidiPanelDriver.h"

class OpnBase;

/**
 * @brief OPN PortA/B 経由の PanelSubsystem 具象ドライバ
 * @details マトリックススキャン・ソフトトグル・PB bit7 LED モード（A/B）を担当する。
 */
class OpnMidiPanelDriver final : public IMidiPanelDriver {
public:
    /**
     * @brief コンストラクタ
     * @param [in] opn PortA/B を持つ FM モジュール
     */
    explicit OpnMidiPanelDriver(OpnBase& opn);

    bool IsAvailable() const override { return true; }

    /**
     * @brief PortA/B 方向設定と初期スキャン
     * @details 4 回 Tick() して初期押下状態を確定する。
     */
    void Initialize() override;

    void SetLedBitmap(uint16_t led_bitmap) override;
    uint16_t GetSwitchBitmap() const override { return switch_bitmap_; }
    void Tick() override;
    bool IsMidiReset() const override;

private:
    /** @brief マトリックス読取り・トグル用の調整パラメータ */
    struct HardwareConfig {
        uint16_t debounce_ms;      ///< 押下時のチャタリング除去時間 [ms]
        uint16_t toggle_hold_ms;   ///< トグル反転に必要な押下継続時間 [ms]
        uint16_t long_press_ms;    ///< 長押し成立に必要な押下継続時間 [ms]
        uint16_t settle_us;        ///< 列切替後のPB電位の安定待ち [µs]
    };

    /** @brief 1CH分の押下・デバウンス・トグル状態 */
    struct ChannelInputState {
        bool latched;               ///< ソフトトグル後 ON/OFF
        bool stable_pressed;        ///< デバウンス後の押下中
        bool last_raw;              ///< 直近の押下状態
        uint32_t raw_change_ms;     ///< 押下状態が変化した時刻 [ms]
        uint32_t press_start_ms;    ///< stable_pressed が true になった時刻 [ms]
    };

    void UpdateChannelInput(int ch_index, bool raw_pressed, uint32_t now_ms);
    void RebuildSwitchBitmap();

    OpnBase& opn_;
    HardwareConfig config_;
    uint16_t host_led_bitmap_;   ///< SetLedBitmap で受け取る（モード B 用）
    uint16_t switch_bitmap_;     ///< トグル後 CH ON/OFF
    uint16_t long_press_bitmap_; ///< 長押し中 CH（bit i = CH(i+1)）
    uint8_t scan_column_;        ///< 現在スキャン中の列 0..3
    ChannelInputState channels_[16];
};
