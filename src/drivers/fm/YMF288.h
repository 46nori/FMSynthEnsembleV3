//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include "OpnBase.h"

/**
 * @brief YMF288 (OPN3-L) class
 */
class YMF288 : public OpnBase {
public:
    YMF288(const fm_device_t *dev, int id);
    YMF288() = delete;
    virtual ~YMF288() {}

    virtual int fm_get_channels() override { return kFmChannels; }

    /**
     * @brief Initialize
     * @details Turn off key of FM and SSG
     */
    virtual void init() override;

private:
    static constexpr int kFmChannels = 6;
};
