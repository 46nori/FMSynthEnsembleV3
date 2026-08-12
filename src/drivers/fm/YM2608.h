//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include "OpnBase.h"

/**
 * @brief YM2608 class
 */
class YM2608 : public OpnBase {
public:
    YM2608(const fm_device_t *dev, int id);
    YM2608() = delete;
    virtual ~YM2608() {}

    virtual int fm_get_channels() override { return kFmChannels; }

    /**
     * @brief Initialize
     * @details Turn off key of FM and SSG
     */
    virtual void init() override;

private:
    static constexpr int kFmChannels = 6;
};
