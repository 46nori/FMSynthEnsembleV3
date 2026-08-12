//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

/**
 * @brief FM音源チップ種別
 */
enum class ChipKind : uint8_t {
    YM2203,
    YM2608,
    YMF288,
};

/**
 * @brief リズム打楽器種別
 */
enum RtmInst : int {
    BD   = 0x01,  ///< Bass Drum
    SD   = 0x02,  ///< Snare Drum
    TOP  = 0x04,  ///< Top Cymbal
    HH   = 0x08,  ///< Hi-Hat
    TOM  = 0x10,  ///< Tom
    RIM  = 0x20,  ///< Rim Shot
    NONE = 0xff,  ///< マッピングなし（percussion_map 番兵用）
};

/**
 * @brief リズム音源フィーチャ
 * @details rhythm() が nullptr でないモジュールのみリズム操作可能。
  */
class IRhythm {
public:
    virtual ~IRhythm() = default;

    /** @brief モジュールID（デバッグ用） */
    virtual int module_id() const = 0;

    /**
     * @brief リズムキー ON
     * @param [in] rtm : RIM | TOM | HH | TOP | SD | BD (1 種類のみ)
     */
    virtual void rtm_turnon_key(int rtm) = 0;

    /**
     * @brief リズムキー Damp
     * @param [in] rtm : RIM | TOM | HH | TOP | SD | BD (複数可)
     */
    virtual void rtm_damp_key(int rtm) = 0;

    /**
     * @brief リズム全体音量（RTL）
     * @param [in] tl : 0-63
     */
    virtual void rtm_set_total_level(uint8_t tl) = 0;

    /**
     * @brief 打楽器ごとのレベル（IL）
     * @param [in] rtm : 打楽器種別（1 種類）
     * @param [in] tl  : 0-31
     * @param [in] lr  : 0xc0=Both, 0x80=L, 0x40=R
     */
    virtual void rtm_set_inst_level(int rtm, uint8_t tl, uint8_t lr = 0xc0) = 0;
};

/**
 * @brief LFO フィーチャ
 * @details LFO 非搭載チップ（YM2203）は OpnBase の no-op 転送で吸収する。
 */
class ILfo {
public:
    virtual ~ILfo() = default;

    /** @brief LFO ON */
    virtual void TurnOn(uint8_t freq) = 0;

    /** @brief LFO OFF */
    virtual void TurnOff() = 0;

    /**
     * @brief チャンネルの Phase Modulation Sensitivity
     * @param [in] ch  : Channel number (0-)
     * @param [in] pms : 0-7
     * @param [in] lr  : 0xc0=Both, 0x80=L, 0x40=R
     */
    virtual void SetPMS(uint8_t ch, uint8_t pms, uint8_t lr) = 0;

    /**
     * @brief チャンネルの Amplitude Modulation Sensitivity
     * @param [in] ch  : Channel number (0-)
     * @param [in] op  : 未使用（将来のオペレータ別AMS拡張用に予約）
     * @param [in] ams : 0-3
     * @param [in] lr  : 0xc0=Both, 0x80=L, 0x40=R
     */
    virtual void SetAMS(uint8_t ch, uint8_t op, uint8_t ams, uint8_t lr) = 0;

    /** @brief 出力 L/R 設定 */
    virtual void SetOutputLR(uint8_t ch, uint8_t lr) = 0;

    /** @brief PMS/AMS 状態をクリアして LFO を OFF にする */
    virtual void Reset() = 0;
};

/**
 * @brief SSG I/O ポートフィーチャ
 * @details io_port() が nullptr なら I/O ポートなし
 */
class IIoPort {
public:
    virtual ~IIoPort() = default;

    /**
     * @brief I/O ポート入出力方向設定
     * @param [in] pa : true=OUT, false=IN (PORT A)
     * @param [in] pb : true=OUT, false=IN (PORT B)
     */
    virtual void set_port_direction(bool pa, bool pb) = 0;

    /** @brief PORT A へ書き込む */
    virtual void write_port_a(uint8_t data) = 0;

    /** @brief PORT B へ書き込む */
    virtual void write_port_b(uint8_t data) = 0;

    /** @brief PORT A を読み出す */
    virtual uint8_t read_port_a() = 0;

    /** @brief PORT B を読み出す */
    virtual uint8_t read_port_b() = 0;
};
