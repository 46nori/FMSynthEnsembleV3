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
 * @brief リズム音源の実装（IRhythm フィーチャ）
 */
class OpnRhythm final : public IRhythm {
public:
    OpnRhythm(const fm_device_t* dev, int module_id) : dev_(dev), module_id_(module_id) {}

    int module_id() const override { return module_id_; }

    void rtm_turnon_key(int rtm) override {
        const int inst = rtm & 0x3f;
        // Key ON: exactly one instrument at a time (single power-of-2 bit required).
        if (inst == 0 || (inst & (inst - 1)) != 0) {
            return;
        }
        write_reg(dev_, 0x10, 0, inst);
    }

    void rtm_damp_key(int rtm) override {
        const int inst = rtm & 0x3f;
        if (inst == 0) {
            return;
        }
        // damp は複数楽器同時 (AllNoteOff 等) を許可
        write_reg(dev_, 0x10, 0, inst | 0x80);
    }

    void rtm_set_total_level(uint8_t tl) override {
        write_reg(dev_, 0x11, 0, tl);
    }

    void rtm_set_inst_level(int rtm, uint8_t tl, uint8_t lr) override {
        tl = lr | (tl & 0x1f);
        switch (rtm) {
        case 0x01:  // BD
            write_reg(dev_, 0x18, 0, tl);
            break;
        case 0x02:  // SD
            write_reg(dev_, 0x19, 0, tl);
            break;
        case 0x04:  // TOP
            write_reg(dev_, 0x1a, 0, tl);
            break;
        case 0x08:  // HH
            write_reg(dev_, 0x1b, 0, tl);
            break;
        case 0x10:  // TOM
            write_reg(dev_, 0x1c, 0, tl);
            break;
        case 0x20:  // RIM
            write_reg(dev_, 0x1d, 0, tl);
            break;
        default:
            break;
        }
    }

private:
    const fm_device_t* dev_;
    int                module_id_;
};
