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

    void fm_turnon_LFO(uint8_t freq) override;
    void fm_turnoff_LFO() override;
    void fm_set_LFO_PMS(uint8_t ch, uint8_t pms, uint8_t lr) override;
    void fm_set_LFO_AMS(uint8_t ch, uint8_t op, uint8_t ams, uint8_t lr) override;
    virtual void fm_set_output_lr(uint8_t ch, uint8_t lr) override;

private:
    static constexpr int kFmChannels = 6;

    uint8_t LFO_pms[kFmChannels] = {};
    uint8_t LFO_ams[kFmChannels] = {};

    void write_lfo_control(uint8_t ch, uint8_t lr);
};
