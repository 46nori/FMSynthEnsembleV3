//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "YM2608.h"
#include "OpnLfo.h"
#include "OpnRhythm.h"
#include "OpnSsgIoPort.h"

YM2608::YM2608(const fm_device_t *dev, int id) : OpnBase(dev, id) {
    kind_           = ChipKind::YM2608;
    csm_capable_    = true;
    io_feature_     = std::make_unique<OpnSsgIoPort>(dev);
    rhythm_feature_ = std::make_unique<OpnRhythm>(dev, id);
    lfo_feature_    = std::make_unique<OpnLfo>(dev, kFmChannels);
}

void YM2608::init() {
    write_reg(dev, 0x2d, 0, 0x00);  // Set Prescaler 1/6

    // Reset LFO state (OPNA-specific, not managed by OpnBase)
    lfo_feature_->Reset();

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
