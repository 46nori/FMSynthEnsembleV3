//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include <cstdint>

#include "opn_piolib.h"

// Enable Total level equalization (TL Trim)
#define ENABLE_FM_TL_TRIM 1

/**
 * @brief OpnBase class
 */
class OpnBase {
private:
    uint8_t ch3_mode;        // ch3 mode bits of 0x27
    uint8_t timer_mode;      // lower 6bit of 0x27
    const float timerA_k;    // Constant for Timer A
    const float timerB_k;    // Constant for Timer B

protected:
    const fm_device_t *dev;

public:
    const int id;  // debug

    /**
     * @brief Constructor
     * @param [in] dev   :  Initialized FM device handle
     * @param [in] id    :  Module ID (arbitrary unique ID)
     */
    OpnBase(const fm_device_t *dev, int mid);
    OpnBase() = delete;

    virtual ~OpnBase() {}

    /**
     * @brief Initialize 
     * @details Trun off key of FM and SSG
     */
    virtual void init();

    /**
     * @brief Get number of FM channels
     * @return Number of FM channels
     */
    virtual int fm_get_channels();

    /////////////////////////////////////////////////////////
    // FM
    /////////////////////////////////////////////////////////
    /**
     * @brief Set Self-feedback nd algorithm
     * @param [in] ch  : Channel number (0- )
     * @param [in] fb  : Self-feedback
     * @param [in] alg : Connection algorithm
     */
    void fm_set_algorithm(uint8_t ch, uint8_t fb, uint8_t alg);

    /**
     * @brief Set FM tone by Tone number
     * @param [in] ch : Channel number (0- ) 
     * @param [in] no : Tone number (0-127)
     * @note Note is set to 0 if no is out of range
     */
    void fm_set_tone(uint8_t ch, int no);

    /**
     * @brief Set FM tone by binary data
     * @param [in] ch   : Channel number (0- ) 
     * @param [in] tone : Tone data (29bytes array)
     * @note See fm_tone_table[][] as for the array structure 
     */
    void fm_set_tone(uint8_t ch, const uint8_t* tone);

    /**
     * @brief Set FM pitch
     * @param [in] ch  : Channel number (0- ) 
     * @param [in] p   : Pitch number (0-11)
     * @param [in] oct : Octave (0-7)
     */
    void fm_set_pitch(uint8_t ch, uint8_t p, uint8_t oct, int16_t diff = 0);

    /**
     * @brief Turn on FM key
     * @param [in] ch : Channel number (0- ) 
     * @param [in] op : Turn on operators
     *                :  bit 0 : OP1 (1 by default)
     *                :  bit 1 : OP2 (1 by default)
     *                :  bit 2 : OP3 (1 by default)
     *                :  bit 3 : OP4 (1 by default)
     */
    void fm_turnon_key(uint8_t ch, uint8_t op = 0x0f);

    /**
     * @brief Turn off FM key
     * @param [in] ch : Channel number (0- ) 
     */
    void fm_turnoff_key(uint8_t ch);

    /**
     * @brief Set Detune/Multiple
     * @param [in] ch : Channel number (0- )
     * @param [in] op : Operator (0-3)
     * @param [in] dt : Detune (0-7)
     * @param [in] ml : Multiple (0-15)
     */
    void fm_set_detune_multiple(uint8_t ch, uint8_t op, uint8_t dt, uint8_t ml);

    /**
     * @brief Set total level of Channel's Operator
     * @param [in] ch : Channel number (0- )
     * @param [in] op : Operator (0-3)
     * @param [in] tl : Total Level (0-127)
     */
    void fm_set_total_level(uint8_t ch, uint8_t op, uint8_t tl);

    /**
     * @brief Set channel volume
     * @param [in] ch : Channel number (0- )
     * @param [in] no : Tone number (0-127)
     * @param [in] tl : Additional TL attenuation steps (0-127)
     * @note ENABLE_FM_TL_TRIM が有効な場合、fm_tl_trim[] を適用する
     */
    void fm_set_volume(uint8_t ch, uint8_t no, uint8_t tl);

#if ENABLE_FM_TL_TRIM
    /**
     * @brief Program別TL補正(TL Trim)の実行時ON/OFF状態を返す
     */
    static bool IsTLTrimEnabled();

    /**
     * @brief Program別TL補正(TL Trim)の実行時ON/OFFを設定する
     */
    static void SetTLTrimEnabled(bool enabled);
#else
    static bool IsTLTrimEnabled() { return false; }
    static void SetTLTrimEnabled(bool enabled) { (void)enabled; }
#endif

    /**
     * @brief Set Envelope of Channel's Operator
     * @param [in] ch : Channel number (0- )
     * @param [in] op : Operator (0-3)
     * @param [in] ev : Envelope
     */
    typedef struct {
        uint8_t ks;  // Key Scale
        uint8_t ar;  // Attack Rate
        uint8_t dr;  // Decay Rate
        uint8_t sr;  // Sustain Rate
        uint8_t sl;  // Sustain Level
        uint8_t rr;  // Release Rate
    } fm_env;
    void fm_set_envelope(uint8_t ch, uint8_t op, const fm_env& ev);

    /**
     * @brief Set SSG Envelope of Channel's Operator
     * @param [in] ch   : Channel number (0- )
     * @param [in] op   : Operator (0-3)
     * @param [in] type : SSG type Envelope
     *                  :   bit3 enable / bit2-0 envelope type
     */
    void fm_set_ssg_envelope(uint8_t ch, uint8_t op, uint8_t type);

    /**
     * @brief Set BLOCK and F-NUMBER in Normal mode
     * @param [in] ch : Channel number (0- )
     * @param [in] fnum2 : Block/F-Number2
     * @param [in] fnum1 : F-Number1
     */
    void fm_set_fnumber(uint8_t ch, uint8_t fnum2, uint8_t fnum1);

    /**
     * @brief Set BLOCK and F-NUMBER of Channel-3 in CSM/Sound effect mode
     * @param [in] op    : Operator (0-3) 
     * @param [in] fnum2 : Block/F-Number2
     * @param [in] fnum1 : F-Number1
     */
    void fm_set_fnumber_ch3(uint8_t op, uint8_t fnum2, uint8_t fnum1);

    /////////////////////////////////////////////////////////
    // SSG
    /////////////////////////////////////////////////////////
    /**
     * @brief Set SSG pitch
     * @param [in] ch  : Channel number (0- ) 
     * @param [in] p   : Pitch number (0-11)
     * @param [in] oct : Octave (0-7)
     */
    void ssg_set_pitch(uint8_t ch, uint8_t p, uint8_t oct = 3);

    /**
     * @brief Set SSG volume
     * @param [in] ch  : Channel number (0- ) 
     * @param [in] vol : 0-15 w/o envelope, 16 w/ envelope
     */
    void ssg_set_volume(uint8_t ch, uint8_t vol);

    /**
     * @brief Set SSG noise
     */
    void ssg_set_noise(uint8_t noise);

    /**
     * @brief Turn on SSG key
     * @param [in] ch    : Channel number (0- ) 
     * @param [in] noise : Enable noise if true
     */
    void ssg_turnon_key(uint8_t ch, bool noise = false);

    /**
     * @brief Turn off SSG key
     * @param [in] ch    : Channel number (0- ) 
     * @param [in] noise : Disable noise if true
     */
    void ssg_turnoff_key(uint8_t ch, bool noise = false);

    /**
     * @brief Set SSG envelope
     * @param [in] period  : Envelope period
     * @param [in] pattern : Envelope pattern
     */
    void ssg_set_envelope(uint16_t period, uint8_t pattern);

    /////////////////////////////////////////////////////////
    // TIMER
    /////////////////////////////////////////////////////////
    /**
     * @brief Set Timer A by value
     * @param [in] value : 10bit
     */
    void set_timer_a(uint16_t value);

    /**
     * @brief Set Timer A by time(ms)
     * @param [in] time : milli seconds
     */
    void set_timer_a_ms(float time);

    /**
     * @brief Set Timer B by value
     * @param [in] value : 8bit
     */
    void set_timer_b(uint8_t value);

    /**
     * @brief Set Timer B by time(ms)
     * @param [in] time : milli seconds
     */
    void set_timer_b_ms(float time);

    /**
     * @brief Timer mode
     * @param [in] mode: 
     * @details
     *    bit 0: 1: Start Timer A
     *           0: Stop Timer A
     *    bit 1: 1: Start Timer B
     *           0: Stop Timer A
     *    bit 2: 1: Set Timer A flag and IRQ when overflow
     *           0: Don't care overflow
     *    bit 3: 1: Set Timer B flag and IRQ when overflow
     *           0: Don't care overflow
     *    bit 4: Reset Timer A flag if 1
     *    bit 5: Reset Timer B flag if 1
     *    bi6 6,7 : ignored
     */
    void set_timer_mode(uint8_t mode);

    /**
     * @brief Set FM CH3 mode
     * @param [in] mode : 0..3
     * @details
     *    0: Normal
     *    1: Sound effect
     *    2: CSM (Key on/off of CH3 is controlled by Timer A)
     *    Unchanged if mode > 2
     */
    void set_fmch3_mode(uint8_t mode);

    /////////////////////////////////////////////////////////
    // I/O PORT
    /////////////////////////////////////////////////////////
    /**
     * @brief Set I/O port direction
     * @param [in] pa : PORT A true:OUT, false:IN
     * @param [in] pb : PORT B true:OUT, false:IN
     */
    void set_port_direction(bool pa, bool pb);

    /**
     * Write to I/O port A
     */
    void write_port_a(uint8_t data);

    /**
     * @brief Write to I/O port B
     */
    void write_port_b(uint8_t data);

    /**
     * @brief Read I/O port A
     */
    uint8_t read_port_a();

    /**
     * @brief Read I/O port B
     */
    uint8_t read_port_b();

    /////////////////////////////////////////////////////////
    // Status
    /////////////////////////////////////////////////////////
    /**
     * @brief Read Status
     * @param [in] st: 0 for Status 0, 1 for YM2608 Status 1
     * @return Status
     */
    uint8_t read_status(int a1 = 0);

    /////////////////////////////////////////////////////////
    // YM2608
    /////////////////////////////////////////////////////////
    // LFO
    virtual void fm_turnon_LFO(uint8_t freq) {}
    virtual void fm_turnoff_LFO() {}
    virtual void fm_set_LFO_PMS(uint8_t ch, uint8_t pms, uint8_t lr = 0xc0) {}
    virtual void fm_set_LFO_AMS(uint8_t ch, uint8_t op, uint8_t ams, uint8_t lr = 0xc0) {}
    virtual void fm_set_output_lr(uint8_t ch, uint8_t lr) {}
    // Rhythm
    virtual void rtm_turnon_key(int rtm) {}
    virtual void rtm_damp_key(int rtm) {}
    virtual void rtm_set_total_level(uint8_t tl) {}
    virtual void rtm_set_inst_level(int rtm, uint8_t tl, uint8_t lr = 0xc0) {}

    /* FM pitch table */
    static constexpr uint16_t fm_pitch_table[] = {
        0x0269,  // C
        0x028e,  // C#
        0x02b4,  // D
        0x02de,  // D#
        0x0309,  // E
        0x0338,  // F
        0x0369,  // F#
        0x039c,  // G
        0x03d3,  // G#
        0x040e,  // A
        0x044b,  // A#
        0x048d,  // B
    };

    /* SSG pitch table for octave 0 */
    static constexpr uint16_t ssg_pitch_table[] = {
        0x0ee8,  // C
        0x0e12,  // C#
        0x0d48,  // D
        0x0c88,  // D#
        0x0bd4,  // E
        0x0b2a,  // F
        0x0a8a,  // F#
        0x09f2,  // G
        0x0964,  // G#
        0x08dc,  // A
        0x085e,  // A#
        0x07e6,  // B
    };
    static constexpr int MAXNUM_OCT       = 7;
    static constexpr int MAXNUM_FM_PITCH  = sizeof(fm_pitch_table) / sizeof(uint16_t) - 1;
    static constexpr int MAXNUM_SSG_PITCH = sizeof(ssg_pitch_table) / sizeof(uint16_t) - 1;

/* FM tone parameter table */
#include "tone/tone_table.inc"
    static constexpr int MAXNUM_FM_TONE = sizeof(fm_tone_table) / sizeof(fm_tone_table[0]);
};
