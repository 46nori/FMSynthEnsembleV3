//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include "OpnBase.h"
#include "Voice.h"

/**
 * @brief Voice class
 */
class NoteVoice : public Voice {
private:
    OpnBase& module;      // FM module
    const uint8_t fm_ch;  // FM moduleのChannel No.

public:
    /**
     * @brief コンストラクタ
     * @param module FM音源モジュール
     * @param ch     FM音源モジュールのチャンネル番号
     * @param id     Voice ID (for debug)
     */
    NoteVoice(OpnBase& module, uint8_t ch, int id);
    NoteVoice() = delete;

    /**
     * @brief デストラクタ
     */
    virtual ~NoteVoice();

    /**
     * @brief Voice内部状態をリセットする
     */
    void Reset() override;

    /**
     * @brief Module IDを返す
     * @return Module ID
     */
    int GetModuleId() override;

    /**
     * @brief MIDI Program(音色)のセット
     * @param no MIDI Program No. (0-127)
     * @details 現在のProgram値から更新された場合に限り、音色パラメータをセットする
     */
    void SetProgram(int32_t no) override;

    /**
     * @brief MIDI Volumeのセット
     * @param vol        MIDI Volume (0-127)
     * @param trem_atten トレモロ減衰（TL ステップ、符号付き）
     * @details vol と trem_atten の組が現在の設定から変化した場合に限り、音量をセットする
     */
    void SetVolume(int vol, int16_t trem_atten) override;

    /**
     * @brief 現在のMIDI Volume・トレモロ設定を再適用する
     * @details TL TrimのON/OFF切替時などに使用する
     */
    void RefreshVolume() override;

    /**
     * @brief Note On
     * @param note       MIDI Note No.
     * @param bk_program MIDI Bank/Program No.
     * @param volume     MIDI Volume (0-127)
     * @param effect     チャンネルエフェクト（KeyOn 前のピッチに使用）
     * @param lr         Output Both(0xc0), Left(0x80), Right(0x40)
     * @param vib_cents  ビブラート偏差（セント、符号付き）
     * @param trem_atten トレモロ減衰（TL ステップ、符号付き）
     * @details FM音源の発音を開始する。KeyOn 前に ApplyPitch(effect, vib_cents) で
     *          PB・coarse tune・ビブラートを設定する。
     */
    void NoteOn(int note, int32_t bk_program, int volume, ChannelEffects& effect,
                uint8_t lr, int16_t vib_cents, int16_t trem_atten) override;

    /**
     * @brief Note Off
     * @details Note FM音源の発音を停止する
     */
    void NoteOff() override;

    /**
     * @brief 発音中ノートの再トリガ
     * @param note       MIDI Note No.
     * @param bk_program MIDI Bank/Program No.
     * @param volume     MIDI Volume (0-127)
     * @param effect     チャンネルエフェクト（KeyOn 前のピッチに使用）
     * @param lr         Output Both(0xc0), Left(0x80), Right(0x40)
     * @param vib_cents  ビブラート偏差（セント、符号付き）
     * @param trem_atten トレモロ減衰（TL ステップ、符号付き）
     * @details FM キー制御レジスタへ KeyOff(0)→即時 KeyOn(1) を連続書込みし、
     *          KeyOn 立ち上がりエッジを人工的に再生成して各 OP のエンベロープを Attack から再開させる。
     *          KeyOn 前に ApplyPitch(effect, vib_cents) で PB・coarse tune・ビブラートを設定する。
     */
    bool TryRetrigger(int note, int32_t bk_program, int volume, ChannelEffects& effect,
                      uint8_t lr, int16_t vib_cents, int16_t trem_atten) override;

    /**
     * @brief 現在の key を基準に PB・coarse tune・ビブラートを合成してピッチを設定する
     * @param fx        チャンネルエフェクト
     *                  fx.pbv PitchBend値 (-8192-8191)
     *                  fx.pbs PitchBend Sensitivity (0-127)
     *                  fx.coarse_tune Coarse tuning (ENABLE_COARSE_TUNE 時)
     * @param vib_cents ビブラート偏差（セント、符号付き。NoteChannel が算出）
     * @param allow_vib_dedup true のとき、直前と同じ vib_cents なら FM 書き込みを省略する
     * @details PitchBend を指定しない場合は fx.pbv=0 とする
     */
    void ApplyPitch(const ChannelEffects& fx, int16_t vib_cents,
                    bool allow_vib_dedup) override;

    /**
     * @brief パン（LR 出力）のみ設定する
     * @param lr  Output Both(0xc0), Left(0x80), Right(0x40)
     * @details ビブラート深さとは無関係。以前の SetModulation にあった HW LFO/PMS は使用しない。
     */
    void SetPan(uint8_t lr) override;

    // Debug
    void dump() override;

private:
    int16_t  last_fm_vib_cents_;    // allow_vib_dedup 用（INT16_MIN=未設定）
    int16_t  last_fm_trem_atten_;   // SetVolume の dedup 用（INT16_MIN=未設定）
};
