//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "OpnBase.h"

#include <algorithm>

// In opn_piolib C API calls, `::` is used to distinguish them from OpnBase members.

#if ENABLE_FM_TL_TRIM
#include "tone/fm_tl_trim.inc"

static volatile bool s_fm_tl_trim_enabled = true;
#endif

OpnBase::OpnBase(const fm_device_t *dev, int id)
    : dev(dev),
      timerA_k((float)dev->master_clock_hz / 72000.0f),
      timerB_k((float)dev->master_clock_hz / 1152000.0f),
      ch3_mode(0),   // Set FM CH3 normal mode
      timer_mode(0), // Reset timer settings
      id(id) {
}

void OpnBase::init() {
    ch3_mode   = 0;  // Set FM CH3 normal mode
    timer_mode = 0;  // Reset timer settings

    ::write_reg(dev, 0x2d, 0, 0x00);  // Set Prescaler 1/6
    ::write_reg(dev, 0x27, 0, 0x30);  // Normal mode, Reset Timer and IRQ and flags

    ::write_reg(dev, 0x07, 0, 0xff);  // SSG noise/tone off
    ::write_reg(dev, 0x08, 0, 0x00);  // SSG Channel A volume 0
    ::write_reg(dev, 0x09, 0, 0x00);  // SSG Channel B volume 0
    ::write_reg(dev, 0x0a, 0, 0x00);  // SSG Channel C volume 0

    // default for OPN
    for (int ch = 0; ch < 3; ch++) {
        fm_turnoff_key(ch);        // Turn Off Key
        fm_set_fnumber(ch, 0, 0);  // Block/F-Number
        for (int op = 0; op < 4; op++) {
            fm_set_total_level(ch, op, 0x7f);  // Total Level
        }
    }
}

int OpnBase::fm_get_channels() {
    return 3;  // default for OPN
}

/////////////////////////////////////////////////////////
// FM Synthesis
/////////////////////////////////////////////////////////
void OpnBase::fm_set_algorithm(uint8_t ch, uint8_t fb, uint8_t alg) {
    uint8_t a1 = 0;
    if (ch >= 3) {
        ch -= 3;
        a1 = 1;
    }
    ::write_reg(dev, 0xb0 + ch, a1, (fb & 7) << 3 | alg & 0x07);
}

void OpnBase::fm_set_tone(uint8_t ch, int no) {
    const int tone_count = static_cast<int>(sizeof(fm_tone_table) / sizeof(fm_tone_table[0]));
    if (no < 0 || no >= tone_count) {
        no = 0;
    }
    fm_set_tone(ch, &fm_tone_table[no][0]);
}

void OpnBase::fm_set_tone(uint8_t ch, const uint8_t* tone) {
    uint8_t a1 = 0;
    if (ch >= 3) {
        ch -= 3;
        a1 = 1;
    }
    int i;
    uint8_t adrs = 0x30 + ch;
    for (i = 0; i < sizeof(fm_tone_table[0]) - 1; i++) {
        ::write_reg(dev, adrs, a1, tone[i]);
        adrs += 4;
    }
    ::write_reg(dev, adrs + 0x10, a1, tone[i]);
}

void OpnBase::fm_set_pitch(uint8_t ch, uint8_t p, uint8_t oct, int16_t diff) {
    if (p <= MAXNUM_FM_PITCH && oct <= MAXNUM_OCT) {
        int16_t fnum = fm_pitch_table[p] + diff;
        if (fnum < 0x0000) fnum = 0x000;
        if (fnum > 0x07ff) fnum = 0x7ff;
        ::fm_set_freq(dev, ch, oct, (uint16_t)fnum);
    }
}

void OpnBase::fm_turnon_key(uint8_t ch, uint8_t op) {
    if (ch >= 3) {
        ch = (ch + 1) & 0x07;
    }
    ::write_reg(dev, 0x28, 0, ch | (op << 4));
}

void OpnBase::fm_turnoff_key(uint8_t ch) {
    if (ch >= 3) {
        ch = (ch + 1) & 0x07;
    }
    ::write_reg(dev, 0x28, 0, ch);
}

void OpnBase::fm_set_detune_multiple(uint8_t ch, uint8_t op, uint8_t dt, uint8_t ml) {
    uint8_t a1 = 0;
    if (ch >= 3) {
        ch -= 3;
        a1 = 1;
    }
    uint8_t dat = ((dt & 0x7) << 4) | (ml & 0x0f);
    switch (op & 3) {
    case 0:
        ::write_reg(dev, 0x30 + ch, a1, dat);
        break;
    case 1:
        ::write_reg(dev, 0x38 + ch, a1, dat);
        break;
    case 2:
        ::write_reg(dev, 0x34 + ch, a1, dat);
        break;
    case 3:
        ::write_reg(dev, 0x3c + ch, a1, dat);
        break;
    default:
        break;
    }
}

void OpnBase::fm_set_total_level(uint8_t ch, uint8_t op, uint8_t tl) {
    uint8_t a1 = 0;
    if (ch >= 3) {
        ch -= 3;
        a1 = 1;
    }
    switch (op & 3) {
    case 0:
        ::write_reg(dev, 0x40 + ch, a1, tl);
        break;
    case 1:
        ::write_reg(dev, 0x48 + ch, a1, tl);
        break;
    case 2:
        ::write_reg(dev, 0x44 + ch, a1, tl);
        break;
    case 3:
        ::write_reg(dev, 0x4c + ch, a1, tl);
        break;
    default:
        break;
    }
}

#if ENABLE_FM_TL_TRIM
bool OpnBase::IsTLTrimEnabled() {
    return s_fm_tl_trim_enabled;
}

void OpnBase::SetTLTrimEnabled(bool enabled) {
    s_fm_tl_trim_enabled = enabled;
}
#endif

void OpnBase::fm_set_volume(uint8_t ch, uint8_t no, uint8_t vl) {
    const auto tone_count = sizeof(fm_tone_table) / sizeof(fm_tone_table[0]);
    if (no >= tone_count) {
        return;
    }

    const uint8_t* tone = &fm_tone_table[no][0];
#if ENABLE_FM_TL_TRIM
    const int8_t tl_trim = IsTLTrimEnabled() ? fm_tl_trim[no] : 0;
#else
    const int8_t tl_trim = 0;
#endif
    const auto set_carrier_level = [&](uint8_t op) {
        // tone_table.inc stores TL in register write order: S1, S3, S2, S4.
        static constexpr uint8_t tl_index[4] = {4, 6, 5, 7};
        const int16_t tl = static_cast<int16_t>(tone[tl_index[op & 3]] & 0x7f) + vl + tl_trim;
        fm_set_total_level(ch, op, static_cast<uint8_t>(tl < 0 ? 0 : (tl > 0x7f ? 0x7f : tl)));
    };

    int alg = tone[28] & 0x07;
    switch (alg) {
    case 4:  // Carrier: 2,4
        set_carrier_level(1);
        [[fallthrough]];
    case 0:  // Carrier: 4
    case 1:  // Carrier: 4
    case 2:  // Carrier: 4
    case 3:  // Carrier: 4
        set_carrier_level(3);
        break;
    case 7:  // Carrier: 1,2,3,4
        set_carrier_level(0);
        [[fallthrough]];
    case 5:  // Carrier: 2,3,4
    case 6:  // Carrier: 2,3,4
        set_carrier_level(1);
        set_carrier_level(2);
        set_carrier_level(3);
        break;
    default:
        break;
    }
}

void OpnBase::fm_set_envelope(uint8_t ch, uint8_t op, const fm_env& ev) {
    uint8_t a1 = 0;
    if (ch >= 3) {
        ch -= 3;
        a1 = 1;
    }
    switch (op & 3) {
    case 0:
        ch += 0x0;
        break;
    case 1:
        ch += 0x8;
        break;
    case 2:
        ch += 0x4;
        break;
    case 3:
        ch += 0xc;
        break;
    default:
        break;
    }
    ::write_reg(dev, 0x50 + ch, a1, (ev.ks << 6) | (ev.ar & 0x1f));  // KS/AR
    ::write_reg(dev, 0x60 + ch, a1, ev.dr & 0x1f);                   // DR
    ::write_reg(dev, 0x70 + ch, a1, ev.sr & 0x1f);                   // SR
    ::write_reg(dev, 0x80 + ch, a1, (ev.sl << 4) | (ev.rr & 0x0f));  // SL/RR
}

void OpnBase::fm_set_ssg_envelope(uint8_t ch, uint8_t op, uint8_t type) {
    uint8_t a1 = 0;
    if (ch >= 3) {
        ch -= 3;
        a1 = 1;
    }
    switch (op & 3) {
    case 0:
        ch += 0x0;
        break;
    case 1:
        ch += 0x8;
        break;
    case 2:
        ch += 0x4;
        break;
    case 3:
        ch += 0xc;
        break;
    default:
        break;
    }
    ::write_reg(dev, 0x90 + ch, a1, type & 0x0f);
}

void OpnBase::fm_set_fnumber(uint8_t ch, uint8_t fnum2, uint8_t fnum1) {
    uint8_t  block = (fnum2 >> 3) & 0x7;
    uint16_t fnum  = ((uint16_t)(fnum2 & 0x7) << 8) | fnum1;
    ::fm_set_freq(dev, ch, block, fnum);
}

void OpnBase::fm_set_fnumber_ch3(uint8_t op, uint8_t fnum2, uint8_t fnum1) {
    if (ch3_mode == 0) {
        return;
    }
    uint8_t  block = (fnum2 >> 3) & 0x7;
    uint16_t fnum  = ((uint16_t)(fnum2 & 0x7) << 8) | fnum1;
    ::fm_set_freq_ch3(dev, op, block, fnum);
}

/////////////////////////////////////////////////////////
// SSG
/////////////////////////////////////////////////////////
void OpnBase::ssg_set_pitch(uint8_t ch, uint8_t p, uint8_t oct) {
    if (p <= MAXNUM_SSG_PITCH && oct <= MAXNUM_OCT) {
        uint8_t adrs  = (ch % 3) * 2;
        uint16_t data = ssg_pitch_table[p] >> oct;
        ::write_reg(dev, adrs + 0x00, 0, data & 0xff);
        ::write_reg(dev, adrs + 0x01, 0, data >> 8);
    }
}

void OpnBase::ssg_set_volume(uint8_t ch, uint8_t vol) {
    uint8_t adrs = ch % 3;
    uint8_t data = vol > 0x0f ? 0x10 : vol;
    ::write_reg(dev, adrs + 0x08, 0, data);
}

void OpnBase::ssg_set_noise(uint8_t noise) {
    ::write_reg(dev, 0x06, 0, noise & 0x1f);
}

void OpnBase::ssg_turnon_key(uint8_t ch, bool noise) {
    uint8_t data = ::read_reg(dev, 0x07, 0);
    const uint8_t tone_mask = static_cast<uint8_t>(0x01u << (ch % 3));
    const uint8_t noise_mask = static_cast<uint8_t>(0x08u << (ch % 3));
    if (noise) {
        data &= static_cast<uint8_t>(~(tone_mask | noise_mask));
        ::write_reg(dev, 0x07, 0, data);
    } else {
        data &= static_cast<uint8_t>(~tone_mask);
        ::write_reg(dev, 0x07, 0, data);
    }
    return;
}

void OpnBase::ssg_turnoff_key(uint8_t ch, bool noise) {
    uint8_t data = ::read_reg(dev, 0x07, 0);
    const uint8_t tone_mask = static_cast<uint8_t>(0x01u << (ch % 3));
    const uint8_t noise_mask = static_cast<uint8_t>(0x08u << (ch % 3));
    if (noise) {
        data |= static_cast<uint8_t>(tone_mask | noise_mask);
        ::write_reg(dev, 0x07, 0, data);
    } else {
        data |= tone_mask;
        ::write_reg(dev, 0x07, 0, data);
    }
    return;
}

void OpnBase::ssg_set_envelope(uint16_t period, uint8_t pattern) {
    ::write_reg(dev, 0x0b, 0, period & 0xff);
    ::write_reg(dev, 0x0c, 0, period >> 8);
    ::write_reg(dev, 0x0d, 0, pattern & 0x0f);
}

/////////////////////////////////////////////////////////
// TIMER
/////////////////////////////////////////////////////////
void OpnBase::set_timer_a(uint16_t value) {
    ::write_reg(dev, 0x25, 0, value & 0x3);
    ::write_reg(dev, 0x24, 0, (value >> 2) & 0xff);
}

void OpnBase::set_timer_a_ms(float time) {
    int value = 1024 - (uint16_t)(timerA_k * time);
    if (value < 0) {
        value = 0;
    }
    set_timer_a(value);
}

void OpnBase::set_timer_b(uint8_t value) {
    ::write_reg(dev, 0x26, 0, value);
}

void OpnBase::set_timer_b_ms(float time) {
    // timerB_k * time can exceed uint8_t range (0-255); clamp before the
    // narrowing cast to avoid float->uint8_t UB (see GitHub issue #30).
    // raw is bounded to [0,255], so 256-raw is always in [1,256]; no
    // negative-value clamp is needed here (unlike set_timer_a_ms).
    uint8_t raw = (uint8_t)std::clamp(timerB_k * time, 0.0f, 255.0f);
    set_timer_b(256 - raw);
}

void OpnBase::set_timer_mode(uint8_t mode) {
    timer_mode = mode & 0x3f;
    ::write_reg(dev, 0x27, 0, ch3_mode | timer_mode);
}

void OpnBase::set_fmch3_mode(uint8_t mode) {
    if (mode < 3) {
        ch3_mode = mode << 6;
        ::write_reg(dev, 0x27, 0, ch3_mode | timer_mode);
    }
}

/////////////////////////////////////////////////////////
// I/O PORT
/////////////////////////////////////////////////////////
void OpnBase::set_port_direction(bool pa, bool pb) {
    uint8_t data = ::read_reg(dev, 0x07, 0) & 0x3f;
    // D6=IOA, D7=IOB: 1=output, 0=input
    if (pa) {
        data |= 0x40;
    } else {
        data &= 0xbf;
    }
    if (pb) {
        data |= 0x80;
    } else {
        data &= 0x7f;
    }
    ::write_reg(dev, 0x07, 0, data);
}

void OpnBase::write_port_a(uint8_t data) {
    ::write_reg(dev, 0x0e, 0, data);
}

void OpnBase::write_port_b(uint8_t data) {
    ::write_reg(dev, 0x0f, 0, data);
}

uint8_t OpnBase::read_port_a() {
    return ::read_reg(dev, 0x0e, 0);
}

uint8_t OpnBase::read_port_b() {
    return ::read_reg(dev, 0x0f, 0);
}

/////////////////////////////////////////////////////////
// Status
/////////////////////////////////////////////////////////
uint8_t OpnBase::read_status(int a1) {
    return ::read_status(dev, (uint8_t)a1);
}
