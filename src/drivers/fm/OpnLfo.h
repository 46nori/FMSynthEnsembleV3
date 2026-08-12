//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "OpnFeatures.h"
#include "opn_piolib.h"

/**
 * @brief LFO の実装（ILfo フィーチャ）
 * @details YM2608 / YMF288 でレジスタ配置が共通のため一本化する。
 */
class OpnLfo final : public ILfo {
public:
    OpnLfo(const fm_device_t* dev, int channels) : dev_(dev), channels_(channels) {}

    void TurnOn(uint8_t freq) override {
        write_reg(dev_, 0x22, 0, 0x08 | (freq & 0x7));
    }

    void TurnOff() override {
        write_reg(dev_, 0x22, 0, 0x00);
    }

    void SetPMS(uint8_t ch, uint8_t pms, uint8_t lr) override {
        if (ch >= channels_) {
            return;
        }
        pms_[ch] = pms & 7;
        WriteControl(ch, lr);
    }

    void SetAMS(uint8_t ch, uint8_t op, uint8_t ams, uint8_t lr) override {
        (void)op;
        if (ch >= channels_) {
            return;
        }
        ams_[ch] = (ams & 3) << 4;
        WriteControl(ch, lr);
    }

    void SetOutputLR(uint8_t ch, uint8_t lr) override {
        if (ch >= channels_) {
            return;
        }
        WriteControl(ch, lr);
    }

    void Reset() override {
        for (int ch = 0; ch < channels_; ++ch) {
            pms_[ch] = 0;
            ams_[ch] = 0;
        }
        TurnOff();
    }

private:
    static constexpr int kMaxChannels = 6;

    void WriteControl(uint8_t ch, uint8_t lr) {
        uint8_t a1 = 0;
        if (ch >= 3) {
            ch -= 3;
            a1 = 1;
        }
        const uint8_t index = static_cast<uint8_t>(a1 * 3 + ch);
        write_reg(dev_, 0xb4 + ch, a1, lr | ams_[index] | pms_[index]);
    }

    const fm_device_t* dev_;
    int                channels_;
    uint8_t            pms_[kMaxChannels] = {};
    uint8_t            ams_[kMaxChannels] = {};
};
