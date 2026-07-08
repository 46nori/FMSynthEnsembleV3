//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <array>
#include <type_traits>
#include "MidiChannel.h"
#include "NoteVoice.h"
#include "OpnBase.h"
#include "VoiceLimits.h"
#include "config.h"

#if ENABLE_CSM != 0
#include "CsmVoice.h"
#else
class CsmVoice;
#endif

/**
 * @brief MidiFactory class
 */
class MidiFactory {
    using NoteVoiceStorage = std::aligned_storage_t<sizeof(NoteVoice), alignof(NoteVoice)>;
#if ENABLE_CSM != 0
    using CsmVoiceStorage = std::aligned_storage_t<sizeof(CsmVoice), alignof(CsmVoice)>;
#endif

    std::array<OpnBase*, 4>& modules_;
    std::array<MidiChannel*, MIDI_CHANNELS> channels_;
    std::array<NoteVoiceStorage, VoiceLimits::kMaxNoteVoices> note_voice_storage_{};
    int note_voice_count_ = 0;
    CsmVoice* csm_voice_ = nullptr;

#if ENABLE_CSM != 0
    CsmVoiceStorage csm_voice_storage_{};
#endif

public:
    /**
     * @brief MIDIチャンネルとVoiceを生成する
     * @param modules FM音源モジュールの配列
     */
    explicit MidiFactory(std::array<OpnBase*, 4>& modules);
    MidiFactory() = delete;

    /**
     * @brief 生成したチャンネルとVoiceを破棄する
     */
    ~MidiFactory();

    /**
     * @brief 生成済み MIDIチャンネル配列を返す
     * @return MIDIチャンネルの配列への参照
     */
    std::array<MidiChannel*, MIDI_CHANNELS>& GetChannels();

    /** CSM ボイス（CsmFrameTask が参照する単一端末） */
    CsmVoice* GetCsmVoice();
};
