//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>
#include <cassert>
#include "config.h"
#include "VoiceLimits.h"
#include "VoiceAllocator.h"
#include "NoteVoice.h"
#include "NoteChannel.h"
#include "RhythmChannel.h"
#include "debugger.h"

VoiceAllocator::VoiceAllocator() : failed_count(0) {
    voice_pool.reserve(VoiceLimits::kMaxVoices);
    reclaim_targets.reserve(MIDI_CHANNELS);
}

VoiceAllocator& VoiceAllocator::GetInstance() {
    static VoiceAllocator instance;
    return instance;
}

void VoiceAllocator::AddVoice(Voice* voice) {
    voice_pool.push_back(voice);
}

void VoiceAllocator::DeleteAllVoices() {
    voice_pool.clear();
}

void VoiceAllocator::AddReclaimTarget(int channel, IVoiceReclaimable* reclaim_target) {
    // 契約: 登録は ch 昇順。AllocateVoice() では逆順走査して ch=15 -> 0 を実現する。
    assert(reclaim_target != nullptr);
    assert(channel >= 0 && channel < MIDI_CHANNELS);
    if (!reclaim_targets.empty()) {
        assert(reclaim_targets.back().channel < channel);
    }
    reclaim_targets.push_back({channel, reclaim_target});
}

void VoiceAllocator::ClearReclaimTargets() {
    reclaim_targets.clear();
}

Voice* VoiceAllocator::AllocateVoice(int channel, int mid, bool type) {
    Voice* same_module_candidate = nullptr;  // 同一モジュールの未割り当てVoice
    Voice* other_candidate = nullptr;        // 他モジュールの未割り当てVoice

    // note_voice_poolから未割り当てのVoiceを探す
    for (auto* voice : voice_pool) {
        if (voice->IsFree() && voice->GetType() == type) {
            if (mid == -1 || voice->GetModuleId() == mid) {
                // 同一モジュールの未割り当てVoiceが見つかった (最優先)
                same_module_candidate = voice;
                break;
            }
            if (other_candidate == nullptr) {
                // 他モジュールの未割り当てVoiceを保存 (次点)
                other_candidate = voice;
            }
        }
    }
    // 未割り当てVoiceがあれば返す
    if (same_module_candidate != nullptr) {
        same_module_candidate->SetChannel(channel);
        return same_module_candidate;
    }
    if (other_candidate != nullptr) {
        other_candidate->SetChannel(channel);
        return other_candidate;
    }
    // 未割り当てVoiceがない場合、他のChannelから未使用Voiceを回収する
    // 優先度の低いMIDI Channelから依頼する
    // 契約: 登録順は ch 昇順固定。ここは逆順走査で ch=15 -> 0 の調停順序を保証する。
    for (auto it = reclaim_targets.rbegin(); it != reclaim_targets.rend(); ++it) {
        if (it->channel != channel) {
            auto voice = it->reclaim_target->Reclaim(mid, type);
            if (voice) {
                // 未使用Voiceがあった
                voice->SetChannel(channel);
                return voice;
            }
        }
    }
    // 未使用Voiceがなかった
    ++failed_count;
    return nullptr;
}

/**
 * @brief Voiceを解放する
 * @details MIDI Channelに割り当て済みのVoiceを解放し、全Voiceをリセットする
 */
void VoiceAllocator::Reset() {
    failed_count = 0;
    // Channelに割り当て済みのVoiceを強制解放
    for (auto& info : reclaim_targets) {
        info.reclaim_target->ReclaimAll();
    }
    // 全Voiceをリセット
    for (auto& voice : voice_pool) {
        voice->Reset();
    }
}

void VoiceAllocator::RefreshActiveFmVolume() {
    for (auto* voice : voice_pool) {
        if (voice->GetType() || voice->GetNoteOnCount() == 0) {
            continue;
        }
        static_cast<NoteVoice*>(voice)->RefreshVolume();
    }
}

void VoiceAllocator::ReconcileIdleFmKeys(const std::array<MidiChannel*, MIDI_CHANNELS>& channels) {
    for (Voice* voice : voice_pool) {
        if (voice->GetType()) {
            continue;
        }
        const auto* nv = static_cast<const NoteVoice*>(voice);
        if (!nv->ShouldReconcileSilence()) {
            continue;
        }
        bool sounding = false;
        for (int ch = 0; ch < MIDI_CHANNELS; ++ch) {
            if (ch == RhythmChannel::MIDI_RHYTHM_CHANNEL) {
                continue;
            }
            const auto* nc = static_cast<const NoteChannel*>(channels[ch]);
            if (nc->IsVoiceSounding(voice)) {
                sounding = true;
                break;
            }
        }
        if (!sounding) {
            static_cast<NoteVoice*>(voice)->ForceSilenceHardwareKey();
        }
    }
}

//
// For debug
//
int VoiceAllocator::GetFailedCount() {
    return failed_count;
}

void VoiceAllocator::dump() {
    std::printf("\n=== Voice List ===\n");
    for (auto& voice : voice_pool) {
        voice->dump();
    }
}
