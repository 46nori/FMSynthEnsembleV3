//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include "OpnBase.h"

/**
 * @brief YM2203 class
 */
class YM2203 : public OpnBase {
public:
    YM2203(const fm_device_t *dev, int id) : OpnBase(dev, id) {}
    YM2203() = delete;
    virtual ~YM2203() {}

    virtual int fm_get_channels() override { return 3; }
};
