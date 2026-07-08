//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "IMidiPanelDriver.h"

/**
 * @brief 未接続時の MIDI Panel スタブドライバ
 * @details IsAvailable() は false。GetSwitchBitmap() は 0xffff（全 CH 有効）を返す。
 */
class NullMidiPanelDriver final : public IMidiPanelDriver {
public:
    bool IsAvailable() const override { return false; }
    void Initialize() override {}
    void SetLedBitmap(uint16_t /*led_bitmap*/) override {}
    uint16_t GetSwitchBitmap() const override { return 0xffff; }
    void Tick() override {}
    bool IsMidiReset() const override { return false; }
};
