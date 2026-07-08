//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

enum class MidiEventType : uint8_t {
    NoteOff,
    NoteOn,
    PolyAftertouch,
    ControlChange,
    ProgramChange,
    ChannelAftertouch,
    PitchBend,
    ChannelMode,
};

struct MidiEvent {
    MidiEventType   type;
    uint8_t         channel;
    uint8_t         data1;
    uint8_t         data2;
    uint8_t         size;
    uint32_t        timestamp_us;
};

inline bool MidiEventIsNote(MidiEventType type) {
    return type == MidiEventType::NoteOn || type == MidiEventType::NoteOff;
}

inline bool MidiEventIsNote(const MidiEvent& evt) {
    return MidiEventIsNote(evt.type);
}

/** @brief NoteOff または NoteOn velocity=0 */
inline bool MidiEventIsNoteOff(const MidiEvent& evt) {
    return evt.type == MidiEventType::NoteOff
        || (evt.type == MidiEventType::NoteOn && evt.data2 == 0);
}

enum class MidiControlType : uint8_t {
    Reset,
    DebugDumpChannel,
    DebugDumpVoice,
    DebugStats,
    DebugVibratoOverride,
};

struct MidiControlEvent {
    MidiControlType type;
    uint8_t         channel;
    uint8_t         reserved0;
    uint8_t         reserved1;
    uint32_t        timestamp_us;
};
