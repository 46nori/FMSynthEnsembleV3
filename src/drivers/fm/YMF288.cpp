//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "YMF288.h"
#include "OpnLfo.h"
#include "OpnRhythm.h"

YMF288::YMF288(const fm_device_t *dev, int id) : OpnBase(dev, id) {
    kind_           = ChipKind::YMF288;
    csm_capable_    = false;                // No CSM support
    io_feature_     = nullptr;              // No I/O port
    rhythm_feature_ = std::make_unique<OpnRhythm>(dev, id);
    lfo_feature_    = std::make_unique<OpnLfo>(dev, kFmChannels);
}

void YMF288::init() {
    write_reg(dev, 0x20, 0, 0x02);  // Enable native mode (NEW=1); no prescaler register

    // Reset LFO state (YMF288-specific, not managed by OpnBase)
    lfo_feature_->Reset();

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
