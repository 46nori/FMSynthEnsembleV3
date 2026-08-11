//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "YMF288.h"
#include "OpnRhythm.h"

YMF288::YMF288(const fm_device_t *dev, int id) : OpnBase(dev, id) {
    kind_           = ChipKind::YMF288;
    csm_capable_    = false;                // No CSM support
    io_feature_     = nullptr;              // No I/O port
    rhythm_feature_ = std::make_unique<OpnRhythm>(dev, id);
}

void YMF288::init() {
    write_reg(dev, 0x20, 0, 0x02);  // Enable native mode (NEW=1); no prescaler register

    // Reset LFO state (YMF288-specific, not managed by OpnBase)
    for (int ch = 0; ch < kFmChannels; ++ch) {
        LFO_pms[ch] = 0;
        LFO_ams[ch] = 0;
    }
    fm_turnoff_LFO();

    OpnBase::init();

    // YMF288 mode, Enable TB IRQ
    write_reg(dev, 0x29, 0, 0x82);

    // Mute Rhythm volume
    rhythm_feature_->rtm_set_total_level(0x00);                    // RTL(mute)
    rhythm_feature_->rtm_set_inst_level(RtmInst::BD,  0x00);  // IL(mute)
    rhythm_feature_->rtm_set_inst_level(RtmInst::SD,  0x00);
    rhythm_feature_->rtm_set_inst_level(RtmInst::TOP, 0x00);
    rhythm_feature_->rtm_set_inst_level(RtmInst::HH,  0x00);
    rhythm_feature_->rtm_set_inst_level(RtmInst::TOM, 0x00);
    rhythm_feature_->rtm_set_inst_level(RtmInst::RIM, 0x00);
}

void YMF288::fm_turnon_LFO(uint8_t freq) {
    write_reg(dev, 0x22, 0, 0x08 | (freq & 0x7));
}

void YMF288::fm_turnoff_LFO() {
    write_reg(dev, 0x22, 0, 0x00);
}

void YMF288::fm_set_LFO_PMS(uint8_t ch, uint8_t pms, uint8_t lr) {
    if (ch >= kFmChannels) {
        return;
    }
    LFO_pms[ch] = pms & 7;
    write_lfo_control(ch, lr);
}

void YMF288::fm_set_LFO_AMS(uint8_t ch, uint8_t op, uint8_t ams, uint8_t lr) {
    if (ch >= kFmChannels) {
        return;
    }
    LFO_ams[ch] = (ams & 3) << 4;
    write_lfo_control(ch, lr);
    (void)op;
}

void YMF288::fm_set_output_lr(uint8_t ch, uint8_t lr) {
    if (ch >= kFmChannels) {
        return;
    }
    write_lfo_control(ch, lr);
}

void YMF288::write_lfo_control(uint8_t ch, uint8_t lr) {
    uint8_t a1 = 0;
    if (ch >= 3) {
        ch -= 3;
        a1 = 1;
    }
    const uint8_t index = static_cast<uint8_t>(a1 * 3 + ch);
    write_reg(dev, 0xb4 + ch, a1, lr | LFO_ams[index] | LFO_pms[index]);
}
