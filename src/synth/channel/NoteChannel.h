//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>
#include <vector>
#include "MidiChannel.h"
#include "IVoiceReclaimable.h"
#include "VoiceAllocator.h"

/** @brief チャンネル単位のソフトウェア LFO 位相。レート・深さ・位相は MIDI チャンネル共通 */
struct ChannelLfoState {
    uint32_t phase;      // 上位 8 bit が sin LUT インデックス
    uint32_t phase_inc;  // VIBRATO_PERIOD_MS あたりの位相増分
};

/**
 * @brief NoteChannel class
 */
class NoteChannel : public MidiChannel, public IVoiceReclaimable {
private:
    VoiceAllocator* allocator;
    using VoiceQueue = std::vector<Voice*>;

    VoiceQueue activeQueue;  // NoteON状態のVoiceキュー
    VoiceQueue holdQueue;    // NoteOFF待ち状態のVoiceキュー
    VoiceQueue freeQueue;    // 未使用状態のVoiceキュー
    bool bCsmVoiceMode;      // true:CsmVoice, false:NoteType

    ChannelLfoState lfo_{};
    int16_t         last_vib_cents_sent_;  // TickVibrato で最後に FM へ送った vib_cents

    /** @brief effect.vbrate から lfo_.phase_inc を再計算する */
    void updateLfoPhaseInc();

    bool IsVoiceInAttackDelay(const Voice* voice) const;
    bool IsVoiceInVibratoFmDelay(const Voice* voice) const;
    uint32_t VibratoFmStartDelayMs(const Voice* voice) const;
    bool ShouldAdvanceLfoPhase() const;

    /**
     * @brief freeQueueから未使用のVoiceを取得する
     * @param mid       最近使ったmodule id
     * @param type      true:CsmVoice, false:NoteVoice
     * @param fromFirst true:先頭から検索する / false:末尾から検索する
     * @details 最近使ったmoduleに属するVoiceを優先的に探す。
     */
    Voice* getFreeVoice(int mid, bool type, bool fromFirst);

    /**
     * @brief Voiceをキュー間で移動する
     * @param src 移動元キュー
     * @param it  移動元キューのイテレータ
     * @param dst 移動先キュー
     * @details srcキューのitが指し示すVoice 1つ分を、dstキューに移動する
     */
    void moveVoice(VoiceQueue& src, VoiceQueue::iterator it, VoiceQueue& dst);

    /**
     * @brief Voiceをすべて移動する
     * @param src 移動元キュー
     * @param dst 移動先キュー
     * @details srcキューの内容をすべてdstキューに移動する
     */
    void moveAllVoices(VoiceQueue& src, VoiceQueue& dst);

    /** @brief holdQueue の Voice を KeyOff して freeQueue へ移す */
    void releaseHoldQueue();

    /** @brief Voice を KeyOff して freeQueue へ移す */
    void finishNoteOff(VoiceQueue& queue, VoiceQueue::iterator it);

    /**
     * @brief キュー先頭（最古）から type 一致の Voice を奪い KeyOff して返す（調停用）
     * @details module_id は考慮せず、常にキュー先頭（最古）の Voice を奪取する。
     * @return 奪取した Voice。該当なしなら nullptr
     */
    Voice* stealVoiceFromQueue(VoiceQueue& queue, bool type);

public:
    /**
     * @brief コンストラクタ
     * @param no MIDI Channel No.
     */
    NoteChannel(int no);
    NoteChannel() = delete;

    virtual ~NoteChannel();

    /**
     * @brief このチャンネルが発音中かどうかを返す
     * @return true:発音中, false:無音
     */
    bool IsActive() override {
        return !activeQueue.empty() || !holdQueue.empty();
    }

    /**
     * @brief MIDIチャンネルのリセット
     */
    void Reset() override;

    /**
     * @brief 全ノートをOFFする
     */
    void AllNoteOff() override;

    /**
     * @brief 当該チャンネルに割り当てられたVoiceのうち未使用のものを解放する
     * @return 解放したVoiceへのポインタ
     * @details 未使用のVoiceがない場合はnullptrを返す
     */
    Voice* Reclaim(int mid, bool type) override;

    /**
     * @brief 当該チャンネルに割り当てられたVoiceをすべて解放する
     */
    void ReclaimAll() override;

    /**
     * @brief CC#0 Bank select MSB
     * @details `bk_rx_msb` に記録し `bk_stg_msb` を更新。`bk_program` は CC#32 まで変わらない。
     */
    void BankSelect_MSB(uint8_t val) override;

    /**
     * @brief CC#32 Bank select LSB
     * @details 非 CSM バンクは GM 0:0 に正規化する。
     *          MSB=3（`ENABLE_CSM` 時）は CSM Voice モードへ切替。
     *          program 部は維持。発音への反映は次回 Note On から。
     */
    void BankSelect_LSB(uint8_t val) override;

    /**
     * @brief MIDI Program No.をセット
     * @param no MIDI Program No.
     * @details program 部を常に更新する。非 CSM 時はバンク部を 0:0 に正規化。
     *          Bank MSB bit 31-24, Bank LSB bit 23-16, Program bit 15-0
     */
    void SetProgram(uint8_t no) override;

    /**
     * @brief MIDI Volumeをセット
     * @param vol MIDI Volume
     * @details 現在発音中のVoiceのVolumeをリアルタイムに変更する
     */
    void SetVolume(int vol) override;

    /**
     * @brief MIDI Expressionをセット
     * @param val MIDI Expression
     * @details 現在発音中のVoiceのVolumeをリアルタイムに変更する
     */
    void SetExpression(int val) override;

    /**
     * @brief CC#121 Reset All Controller
     * @details effect/LFO 位相のリセットと、発音中・保持中 Voice への
     *          Volume・Pan・ピッチ（ビブラート含む）の再適用を行う。
     */
    void ResetAllController() override;

    /**
     * @brief Note On
     * @param key MIDI Note No.
     * @param velocity MIDI Velocity
     * @return -1:Fail, 0:NoteOff, 1:NoteOn
     */
    int NoteOn(int key, int velocity) override;

    /**
     * @brief Note Off
     * @param key MIDI Note No.
     * @return -1:Fail, 0:NoteOff
     */
    int NoteOff(int key) override;

    /**
     * @brief Hold1 (damper pedal)
     * @param val ダンパー値
     */
    virtual void Hold1(int val) override;

    /**
     * @brief CC#6 Data entry MSB
     * @param   val Data Entry MSB
     */
    virtual void DataEntry_MSB(uint8_t val) override;

    /**
     * @brief PitchBend
     * @param val PitchBend value (-8192 - 8191)
     * @details pbv/pbs に応じたピッチを active・hold の全 Voice に即座に適用する
     */
    virtual void PitchBend(int16_t val) override;

    /**
     * @brief CC#1 Modulation（ビブラート深さ vbdepth）
     * @param val Modulation depth (0-127)
     * @details vbdepth を更新し、active・hold の全 Voice にピッチを即座に再適用する
     */
    virtual void SetModulation(uint8_t val) override;

    /**
     * @brief CC#10 Pan
     * @param val Pan value
     * @details パンを active・hold の全 Voice に即座に適用する。
     * ハード的な制約で連続表現できないため、センター 64 対称 (W=21) で分割する。
     *    0 - 42 : L
     *   43 - 85 : LR
     *   86 -127 : R
     */
    virtual void SetPan(uint8_t val) override;

    /**
     * @brief 現在の LFO 位相からビブラート偏差（セント）を算出する
     * @return 符号付きセント。vbdepth==0 のとき 0
     */
    int16_t ComputeVibCents() const;

    /**
     * @brief active・hold の全 Voice に ApplyPitch を呼ぶ
     * @param vib_cents チャンネル LFO からのビブラート偏差（セント）
     * @param skip_attack_voices true のとき Attack 遅延中 Voice は FM を触らない（TickVibrato 用）
     */
    void ApplyPitchToVoices(int16_t vib_cents, bool skip_attack_voices = false);

    /** @brief MidiEngineTask から呼ぶ（phase_ticks 周期分の LFO 位相を進める） */
    void TickVibrato(uint32_t phase_ticks) override;

    /** @brief activeQueue / holdQueue のいずれかに Voice がある */
    bool IsVoiceSounding(const Voice* voice) const;

    // Debug
    void dump() override;
};

/**
 * @brief ビブラート強制切替モード（デバッガ `vib` コマンド）
 * @details 0=OFF, 1=ON, 2=AUTO（MIDI CC#1 / NRPN に従う）
 */
enum class VibOverride : uint8_t {
    Off  = 0,
    On   = 1,
    Auto = 2,
};

/** @brief ビブラート強制切替。MidiEngineTask が更新、NoteChannel が参照 */
extern volatile VibOverride g_vib_override;

/**
 * @brief g_vib_override を反映したビブラート深さを返す
 * @param channel_vbdepth チャンネルの vbdepth (0-127)
 * @return 適用後の深さ (0-127)。On かつ vbdepth=0 のとき 64
 */
inline uint8_t EffectiveVbdepth(uint8_t channel_vbdepth) {
    switch (g_vib_override) {
    case VibOverride::Off:
        return 0;
    case VibOverride::On:
        return channel_vbdepth != 0 ? channel_vbdepth : 64;
    default:
        return channel_vbdepth;
    }
}
