//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "YM2608.h"
#include "OpnRhythm.h"
#include "OpnSsgIoPort.h"

YM2608::YM2608(const fm_device_t *dev, int id) : OpnBase(dev, id) {
    kind_           = ChipKind::YM2608;
    csm_capable_    = true;
    io_feature_     = std::make_unique<OpnSsgIoPort>(dev);
    rhythm_feature_ = std::make_unique<OpnRhythm>(dev, id);
}

void YM2608::init() {
    write_reg(dev, 0x2d, 0, 0x00);  // Set Prescaler 1/6

    // Reset LFO state (OPNA-specific, not managed by OpnBase)
    for (int ch = 0; ch < kFmChannels; ++ch) {
        LFO_pms[ch] = 0;
        LFO_ams[ch] = 0;
    }
    fm_turnoff_LFO();

    OpnBase::init();

    // OPNA mode, Enable TB IRQ
    write_reg(dev, 0x29, 0, 0x82);

    // Mute Rhythm volume
    rhythm_feature_->rtm_set_total_level(0x00);                   // RTL(mute)
    rhythm_feature_->rtm_set_inst_level(RtmInst::BD, 0x00);  // IL(mute)
    rhythm_feature_->rtm_set_inst_level(RtmInst::SD, 0x00);
    rhythm_feature_->rtm_set_inst_level(RtmInst::TOP, 0x00);
    rhythm_feature_->rtm_set_inst_level(RtmInst::HH, 0x00);
    rhythm_feature_->rtm_set_inst_level(RtmInst::TOM, 0x00);
    rhythm_feature_->rtm_set_inst_level(RtmInst::RIM, 0x00);
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
