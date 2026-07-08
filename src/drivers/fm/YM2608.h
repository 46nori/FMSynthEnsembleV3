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
    enum RtmInst {
        BD   = 0x01,
        SD   = 0x02,
        TOP  = 0x04,
        HH   = 0x08,
        TOM  = 0x10,
        RIM  = 0x20,
        NONE = 0xff
    };

public:
    YM2608(const fm_device_t *dev, int id);
    YM2608() = delete;
    virtual ~YM2608();

    virtual int fm_get_channels() override { return 6; }

    /**
     * @brief Turn on Rhytm Key
     * @param [in] rtm : RIM | TOM | HH | TOP | SD | BD
     */
    void rtm_turnon_key(int rtm) override;

    /**
     * @brief Damp Rhytm Key
     * @param [in] rtm : RIM | TOM | HH | TOP | SD | BD
     */
    void rtm_damp_key(int rtm) override;

    /**
     * @brief Set total level of Rhythm
     * @param [in] tl : Total Level (0-63)
     */
    void rtm_set_total_level(uint8_t tl) override;

    /**
     * @brief Set instrument level
     * @param [in] rtm : RIM | TOM | HH | TOP | SD | BD
     * @param [in] tl  : Instrument Level (0-31)
     * @param [in] lr  : Output Both(0xc0), Left(0x80), Right(0x40)
    */
    void rtm_set_inst_level(int rtm, uint8_t tl, uint8_t lr = 0xc0) override;

    /**
     * @brief Initialize
     * @details Trun off key of FM and SSG
     */
    virtual void init() override;

    void fm_turnon_LFO(uint8_t freq) override;
    void fm_turnoff_LFO() override;
    void fm_set_LFO_PMS(uint8_t ch, uint8_t pms, uint8_t lr) override;
    void fm_set_LFO_AMS(uint8_t ch, uint8_t op, uint8_t ams, uint8_t lr) override;
    virtual void fm_set_output_lr(uint8_t ch, uint8_t lr) override;

private:
    static constexpr uint8_t kFmChannels = 6;

    uint8_t LFO_pms[kFmChannels] = {};
    uint8_t LFO_ams[kFmChannels] = {};

    void write_lfo_control(uint8_t ch, uint8_t lr);
};
