//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "YM2203.h"
#include "OpnSsgIoPort.h"

YM2203::YM2203(const fm_device_t *dev, int id) : OpnBase(dev, id) {
    kind_           = ChipKind::YM2203;
    csm_capable_    = true;
    io_feature_     = std::make_unique<OpnSsgIoPort>(dev);
    rhythm_feature_ = nullptr;          // No Rhythm support
}

void YM2203::init() {
    write_reg(dev, 0x2d, 0, 0x00);  // Set Prescaler 1/6
    OpnBase::init();
}
