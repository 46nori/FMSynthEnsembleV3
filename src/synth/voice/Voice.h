//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include <cstdint>

/**
 * @brief RPN/NRPN設定管理
 */
struct ChannelEffects {
    int16_t pbv;          // Pitch Bend (-8192-8191)
    uint8_t pbs;          // PB sensitivity (default 2)
    uint8_t vbrate;       // 0..127 → vibrato rate
    uint8_t vbdepth;      // 0..127 → vibrato depth
    int8_t  coarse_tune;  // semitones offset (optional)

    void Init() {
        pbv         = 0;
        pbs         = 2;  // デフォルト +/-2半音
        vbrate      = 0;
        vbdepth     = 0;
        coarse_tune = 0;
    }
};

/**
 * @brief Voice class
 */
class Voice {
private:
    int note_on_count;  // NoteOn回数 (Keyオーバーラップ時のカウント用)
    const bool type;    // true:CsmVoice, false:NoteVoice
    int midi_ch;        // 属しているMIDI Channel No.
    uint32_t pitch_attack_start_tick_;  // ビブラート Attack 遅延の基準 (FreeRTOS tick)

protected:
    int32_t bk_program;  // Bank/Program No.
                         //  Bank MSB: bit 31-24 (0-127)
                         //  Bank LSB: bit 23-16 (0-127)
                         //  Program : bit 15- 0 (0-127)
    int volume;          // MIDI Volume (0:min - 127:max)
    int velocity;        // MIDI Velocity (-1: unspecified, 0:min - 127:max)
    int key;             // Note No. (0-127)

public:
    const int id;  // デバッグ用

public:
    /**
     * @brief コンストラクタ
     * @param type   Voice種別 true:CsmVoice, false:NoteVoice
     * @param id     Voice ID (for debug)
     */
    Voice(bool type, int id);
    Voice() = delete;

    /**
     * @brief デストラクタ
     */
    virtual ~Voice();

    /**
     * @brief Voice内部状態をリセットする
     */
    virtual void Reset();

    /**
     * @brief Voice種別を返す
     * @return true:CsmVoice, false:NoteVoice
     */
    bool GetType();

    /**
     * @brief Voiceが未割り当てかどうかを返す
     * @return true:未割り当て, false:割り当て済み
     */
    bool IsFree();

    /**
     * @brief Voiceの割り当て先MIDIチャンネルを返す
     * @return MIDI Channel No.
     */
    int GetChannel();

    /**
     * @brief Voiceを指定したMIDIチャンネルに割り当てる
     * @param channel MIDI Channel No.
     */
    void SetChannel(int channel);

    /**
     * @brief 現在のNote番号を返す
     * @return MIDI Note No.
     */

    int GetKey() const;

    /**
     * @brief VoiceのVelocityをセットする
     * @param val MIDI Velocity
     */
    void SetVelocity(int val);

    /**
     * @brief VoiceのVelocityを返す
     * @return MIDI Velocity
     */
    int GetVelocity();

    /** @brief KeyOn 直後のビブラート遅延タイマを開始する */
    void MarkPitchAttackStart();

    /** @brief MarkPitchAttackStart() で記録した tick */
    uint32_t GetPitchAttackStartTick() const;

    /**
     * @brief Note On のリファレンスカウンタのセット
     * @details Note Offされずに同一NoteのNote Onが発生した場合の管理用カウンタ
     */
    void SetNoteOnCount(int val);

    /**
     * @brief Note On のリファレンスカウンタの取得
     * @return リファレンスカウンタ
     * @details Note Offされずに同一NoteのNote Onが発生した場合の管理用カウンタ
     */
    int GetNoteOnCount();

    /**
     * @brief Note On のリファレンスカウンタのインクリメント
     * @return リファレンスカウンタ
     * @details Note Offされずに同一NoteのNote Onが発生した場合の管理用カウンタ
     */
    int IncrementNoteOnCount();

    /**
     * @brief Note On のリファレンスカウンタのデクリメント
     * @return リファレンスカウンタ
     * @details Note Offされずに同一NoteのNote Onが発生した場合の管理用カウンタ
     */
    int DecrementNoteOnCount();

    /**
     * @brief Module IDを返す
     * @return Module ID
     */
    virtual int GetModuleId() = 0;

    /**
     * @brief MIDI Program(音色)のセット
     * @param no MIDI Program No. (0-127)
     * @details 現在のProgram値から更新された場合に限り、音色パラメータをセットする
     */
    virtual void SetProgram(int32_t no) = 0;

    /**
     * @brief MIDI Volumeのセット
     * @param vol MIDI Volume (0-127)
     * @details 現在のVolume値から更新された場合に限り、音量をセットする
     */
    virtual void SetVolume(int vol) = 0;

    /**
     * @brief Note On
     * @param note       MIDI Note No.
     * @param bk_program MIDI Bank/Program No.
     * @param volume     MIDI Volume (0-127)
     * @param effect     Voice effect
     * @param lr         Output Both(0xc0), Left(0x80), Right(0x40)
     * @details FM音源の発音を開始する。
     *          effectの設定値により、PitchBendやModulationを設定する
     */
    virtual void NoteOn(int note, int32_t bk_program, int volume, ChannelEffects& effect,
                        uint8_t lr) = 0;
    /**
     * @brief Note Off
     * @details Note FM音源の発音を停止する
     */
    virtual void NoteOff() = 0;

    /**
     * @brief PB・coarse tune・ビブラートを合成してピッチを設定する
     * @param fx       チャンネルエフェクト（pbv/pbs/coarse_tune）
     * @param vib_cents  ビブラート偏差（セント、符号付き）
     */
    virtual void ApplyPitch(const ChannelEffects& fx, int16_t vib_cents, bool allow_vib_dedup = false) = 0;

    /**
     * @brief パン（LR 出力）のみ設定する
     * @param lr  Output Both(0xc0), Left(0x80), Right(0x40)
     */
    virtual void SetPan(uint8_t lr) = 0;

    /**
     * @brief 発音中ノートの再トリガを試行する
     * @param note       MIDI Note No.
     * @param bk_program MIDI Bank/Program No.
     * @param volume     MIDI Volume (0-127)
     * @param effect     Voice effect
     * @param lr         Output Both(0xc0), Left(0x80), Right(0x40)
     * @return true:再トリガ成功, false:非対応または失敗
     * @details デフォルト実装は非対応(false)を返す。再トリガ可能な派生クラスでoverrideする。
     */
    virtual bool TryRetrigger(int note, int32_t bk_program, int volume, ChannelEffects& effect,
                              uint8_t lr);

    virtual void dump();
};
