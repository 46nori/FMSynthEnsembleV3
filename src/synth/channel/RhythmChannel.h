//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include <array>
#include <vector>
#include "MidiChannel.h"
#include "IVoiceReclaimable.h"
#include "config.h"

class OpnBase;

/** @brief リズム RTL/IL 追加減衰 (step)。`rmix` で変更、初期値は RHYTHM_LEVEL_OFFSET */
extern volatile int8_t g_rhythm_level_offset;

/**
 * @brief Rhythm Channel class (CH=10)
 */
class RhythmChannel : public MidiChannel, public IVoiceReclaimable {
private:
    static constexpr int kRtmInstSlots = 6;

    std::vector<OpnBase*> modules;              // 使用可能なモジュール(YM2608)
    int16_t last_exclusive_note[6] = {-1, -1, -1, -1, -1, -1};
    uint8_t last_exclusive_module[6] = {0};     // 排他グループごとの直前発音モジュール
    // 各 YM2608 上で最後に鳴らした RtmInst (BD=0x01 等)。別楽器の減衰混入・再トリガ防止用
    std::array<int16_t, 4> last_rtm_on_module = {-1, -1, -1, -1};
    // 打楽器種別ごとに初回割当した modules[] の添字（接続台数に応じて動的、固定マップなし）
    std::array<int8_t, kRtmInstSlots> inst_module_ = {-1, -1, -1, -1, -1, -1};
    uint8_t next_assign_module_ = 0;

public:
    static constexpr int MIDI_RHYTHM_CHANNEL = 9;  // MIDI CH=10

    /**
     * @brief コンストラクタ
     * @param modules FM音源モジュールの配列
     */
    RhythmChannel(std::array<OpnBase*, 4>& input_modules);
    RhythmChannel() = delete;

    /**
     * @brief デストラクタ
     */
    virtual ~RhythmChannel();

    /**
     * @brief MIDIチャンネルのリセット
     */
    virtual void Reset() override;

    /**
     * @brief 全ノートをOFFする
     */
    virtual void AllNoteOff() override;

    /**
     * @brief MIDI Volumeをセット
     * @param vol MIDI Volume (0 - 127)
     */
    void SetVolume(int vol) override;

    /**
     * @brief MIDI Expressionをセット
     * @param val MIDI Expression (0 - 127)
     */
    void SetExpression(int val) override;

    /**
     * @brief Note On
     * @param key MIDI Note No.
     * @param velocity MIDI Velocity
     * @return -1:Fail, 0:NoteOff, 1:NoteOn
     * @details 排他グループ内の別ノート On 時のみ前音を damp。velocity=0 も Dump しない（GM）
     */
    int NoteOn(int key, int velocity) override;

    /**
     * @brief Note Off
     * @param key MIDI Note No.
     * @return 0:NoteOff
     * @details GM 準拠で音響無効（減衰任せ）。CC#120/#123 の AllNoteOff のみ全 damp
     */
    int NoteOff(int key) override;

    /**
     * @brief 当該チャンネルに割り当てられたVoiceのうち未使用のものを解放する
     * @return FM音源のVoiceは使用しないので常にnullptrを返す
     */
    Voice* Reclaim(int mid, bool type) override;

    /**
     * @brief 当該チャンネルに割り当てられたVoiceをすべて解放する
     */
    void ReclaimAll() override;

    /**
     * @brief CC#121 Reset All Controller
     */
    void ResetAllController() override;

    /**
     * @brief 現在の volume / expression と g_rhythm_level_offset を RTL に再適用する
     */
    void RefreshRhythmLevels();

    // Debug
    void dump() override;

private:
    /**
     * @brief RTL/ILの初期化
     * @param rtl RTL Volume (0 - 127)
     * @param il  IL  Volume (0 - 127)
     */
    void init_volume(uint8_t rtl, uint8_t il);

    void ResetRouting();

    /** @brief 打楽器種別に対応するモジュール添字を返す（未割当なら次の空きを割当） */
    uint8_t ResolvePlayModule(int rtm_inst);
};
