//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cassert>
#include "MidiFactory.h"
#include "NoteChannel.h"
#include "NoteVoice.h"
#include "RhythmChannel.h"
#include "config.h"

MidiFactory::MidiFactory(std::array<OpnBase*, 4>& modules) : modules_(modules) {
    VoiceAllocator& allocator = VoiceAllocator::GetInstance();

#if ENABLE_CSM != 0
    static_assert(CSM_N_MAX >= 1 && CSM_N_MAX <= 16, "CSM_N_MAX must be in range 1..16.");
    static_assert(CSM_N <= CSM_N_MAX, "CSM_N must not exceed CSM_N_MAX.");
    constexpr int kCsmReservedModules = (CSM_N_MAX - 1) / 4 + 1;
    int csm_reserved_modules = 0;
#endif

    // VoiceAllocatorにFM音源モジュールのチャンネルを登録
    // 音楽用
    int voice_id = 0;
    for (auto* module : modules_) {
        if (module) {
            module->init();
            for (int ch = 0; ch < module->fm_get_channels(); ++ch) {
#if ENABLE_CSM != 0
                if (ch == 2 && csm_reserved_modules < kCsmReservedModules) {
                    ++csm_reserved_modules;
                    continue;
                }
#endif
                assert(note_voice_count_ < VoiceLimits::kMaxNoteVoices);
                auto* voice = new (&note_voice_storage_[note_voice_count_++])
                    NoteVoice(*module, ch, voice_id++);
                allocator.AddVoice(voice);
            }
        }
    }

#if ENABLE_CSM != 0
    // CSM音声合成用
    csm_voice_ = new (&csm_voice_storage_) CsmVoice(modules_, voice_id++);
    csm_voice_->Init();
    allocator.AddVoice(csm_voice_);
#endif

    // MIDIチャンネルのインスタンスを生成
    // 契約: VoiceAllocatorへの登録順は ch 昇順で固定。
    //      AllocateVoice() は逆順走査し、調停順序 ch=15 -> 0 を保証する。
    for (int i = 0; i < (int)channels_.size(); i++) {
        if (i == RhythmChannel::MIDI_RHYTHM_CHANNEL) {
            // リズムチャンネル
            RhythmChannel* rc = new RhythmChannel(modules_);
            channels_[i]      = rc;
            // VoiceAllocatorに回収対象を登録
            allocator.AddReclaimTarget(i, rc);
        } else {
            // ノートチャンネル
            NoteChannel* nc = new NoteChannel(i);
            channels_[i]    = nc;
            // VoiceAllocatorに回収対象を登録
            allocator.AddReclaimTarget(i, nc);
        }
    }
}

MidiFactory::~MidiFactory() {
    VoiceAllocator::GetInstance().DeleteAllVoices();
    VoiceAllocator::GetInstance().ClearReclaimTargets();

#if ENABLE_CSM != 0
    if (csm_voice_ != nullptr) {
        csm_voice_->~CsmVoice();
        csm_voice_ = nullptr;
    }
#endif

    for (int i = 0; i < note_voice_count_; ++i) {
        auto* voice = reinterpret_cast<NoteVoice*>(&note_voice_storage_[i]);
        voice->~NoteVoice();
    }

    for (auto* channel : channels_) {
        delete channel;
    }
}

std::array<MidiChannel*, MIDI_CHANNELS>& MidiFactory::GetChannels() {
    return channels_;
}

CsmVoice* MidiFactory::GetCsmVoice() {
    return csm_voice_;
}
