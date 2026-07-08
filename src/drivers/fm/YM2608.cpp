//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "YM2608.h"

YM2608::YM2608(const fm_device_t *dev, int id) : OpnBase(dev, id) {
}

YM2608::~YM2608() {
}

void YM2608::rtm_turnon_key(int rtm) {
    const int inst = rtm & 0x3f;
    // 複数ビット (0x3f 等) の誤 Key On を防ぐ — 1 種類のみ
    if (inst == 0 || (inst & (inst - 1)) != 0) {
        return;
    }
    write_reg(dev, 0x10, 0, inst);
}

void YM2608::rtm_damp_key(int rtm) {
    const int inst = rtm & 0x3f;
    if (inst == 0) {
        return;
    }
    // damp は複数楽器同時 (AllNoteOff 等) を許可
    write_reg(dev, 0x10, 0, inst | 0x80);
}

void YM2608::rtm_set_total_level(uint8_t tl) {
    write_reg(dev, 0x11, 0, tl);
}

void YM2608::rtm_set_inst_level(int rtm, uint8_t tl, uint8_t lr) {
    tl = lr | (tl & 0x1f);
    switch (rtm) {
    case BD:
        write_reg(dev, 0x18, 0, tl);
        break;
    case SD:
        write_reg(dev, 0x19, 0, tl);
        break;
    case TOP:
        write_reg(dev, 0x1a, 0, tl);
        break;
    case HH:
        write_reg(dev, 0x1b, 0, tl);
        break;
    case TOM:
        write_reg(dev, 0x1c, 0, tl);
        break;
    case RIM:
        write_reg(dev, 0x1d, 0, tl);
        break;
    default:
        break;
    }
}

void YM2608::init() {
    // Init CH0-2
    OpnBase::init();

    // Reset LFO state (OPNA-specific, not managed by OpnBase)
    for (int ch = 0; ch < kFmChannels; ++ch) {
        LFO_pms[ch] = 0;
        LFO_ams[ch] = 0;
    }
    fm_turnoff_LFO();

    // OPNA mode, Enable TB IRQ
    write_reg(dev, 0x29, 0, 0x82);

    // Init CH3-5
    for (int ch = 3; ch < 6; ch++) {
        fm_turnoff_key(ch);        // Turn Off Key
        fm_set_fnumber(ch, 0, 0);  // Block/F-Number
        for (int op = 0; op < 4; op++) {
            fm_set_total_level(ch, op, 0x7f);  // Total Level(mute)
        }
    }

    // Mute Rhythm volume
    rtm_set_total_level(0x00);              // RTL(mute)
    rtm_set_inst_level(RtmInst::BD, 0x00);  // IL(mute)
    rtm_set_inst_level(RtmInst::SD, 0x00);
    rtm_set_inst_level(RtmInst::TOP, 0x00);
    rtm_set_inst_level(RtmInst::HH, 0x00);
    rtm_set_inst_level(RtmInst::TOM, 0x00);
    rtm_set_inst_level(RtmInst::RIM, 0x00);
}

void YM2608::fm_turnon_LFO(uint8_t freq) {
    write_reg(dev, 0x22, 0, 0x08 | freq & 0x7);
}

void YM2608::fm_turnoff_LFO() {
    write_reg(dev, 0x22, 0, 0x00);
}

void YM2608::fm_set_LFO_PMS(uint8_t ch, uint8_t pms, uint8_t lr) {
    if (ch >= kFmChannels) {
        return;
    }
    LFO_pms[ch] = pms & 7;
    write_lfo_control(ch, lr);
}

void YM2608::fm_set_LFO_AMS(uint8_t ch, uint8_t op, uint8_t ams, uint8_t lr) {
    if (ch >= kFmChannels) {
        return;
    }
    LFO_ams[ch] = (ams & 3) << 4;
    write_lfo_control(ch, lr);
    // TODO: Refer DecayRate from tone table
    //write_reg(dev, 0x60 + ch, 0, 0x80 | Decay);
}

void YM2608::fm_set_output_lr(uint8_t ch, uint8_t lr) {
    if (ch >= kFmChannels) {
        return;
    }
    write_lfo_control(ch, lr);
}

void YM2608::write_lfo_control(uint8_t ch, uint8_t lr) {
    uint8_t a1 = 0;
    if (ch >= 3) {
        ch -= 3;
        a1 = 1;
    }
    const uint8_t index = static_cast<uint8_t>(a1 * 3 + ch);
    write_reg(dev, 0xb4 + ch, a1, lr | LFO_ams[index] | LFO_pms[index]);
}
