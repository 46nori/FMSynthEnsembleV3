//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>
#include <vector>
#include "CsmVoice.h"
#include "IVoiceReclaimable.h"
#include "config.h"

/**
 * @brief VoiceAllocator class
 * @details シングルトンクラス
 */
class VoiceAllocator {
private:
    std::vector<IVoiceReclaimableInfo> reclaim_targets;  // チャンネル別の回収対象リスト
    std::vector<Voice*> voice_pool;       // Voiceのリスト
    int failed_count;                     // DEBUG: Allocation fail count

    VoiceAllocator();
    ~VoiceAllocator() = default;

public:
    VoiceAllocator(const VoiceAllocator&)            = delete;
    VoiceAllocator& operator=(const VoiceAllocator&) = delete;
    VoiceAllocator(VoiceAllocator&&)                 = delete;
    VoiceAllocator& operator=(VoiceAllocator&&)      = delete;

    /**
     * @brief VoiceAllocatorのインスタンスを取得する
     * @return VoiceAllocatorのインスタンス
     * @details メンバー関数は、このインスタンス経由で呼び出す
     */
    static VoiceAllocator& GetInstance();

    /**
     * @brief Voiceを登録する
     * @param voice    登録するNoteVoiceのインスタンスへのポインタ
     */
    void AddVoice(Voice* voice);

    /**
     * @brief 登録したVoiceを全て削除する
     */
    void DeleteAllVoices();

    /**
     * @brief MIDI ChannelのIVoiceReclaimableを登録する
     * @param channel 登録するMIDI Channel No.
     * @param reclaim_target 登録するIVoiceReclaimableのインスタンスへのポインタ
     */
        void AddReclaimTarget(int channel, IVoiceReclaimable* reclaim_target);

    /**
     * @brief 登録したReclaimTargetを全て削除する
     */
    void ClearReclaimTargets();

    /**
     * @brief MIDI ChannelにVoiceを割り当てる
     * @param channel MIDI Channel No.
     * @param mid     module id
     * @param type    Voice Type true:CsmVoice, false:NoteVoice
     * @return Voiceのインスタンスへのポインタ
     * @details 
     * voice_poolに未割り当てのVoiceがあればそれを返す。
     * midと一致するVoiceを優先的に割り当てることで、同一Channel内では
     * なるべく同じmoduleが使われるように仕向ける。
     * ない場合はMIDI ChannelのReclaimTarget経由で未使用のVoiceを回収する。
     * 回収できなかった場合はnullptrを返す。
     */
    Voice* AllocateVoice(int channel, int mid, bool type);

    /**
     * @brief Voiceを解放する
     * @details MIDI Channelに割り当て済みのVoiceを解放し、全Voiceをリセットする
     */
    void Reset();

    //
    // For debug
    //
    int GetFailedCount();
    void dump();
};
