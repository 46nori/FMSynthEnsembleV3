//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "MidiParser.h"

namespace {

uint8_t message_size_for_status(uint8_t status) {
    const uint8_t type = static_cast<uint8_t>((status >> 4) & 0x0f);
    if (type == 0x0c || type == 0x0d) {
        return 2;
    }
    if (type >= 0x08 && type <= 0x0e) {
        return 3;
    }
    return 0;
}

MidiEventType event_type_for_status(uint8_t status, uint8_t data1) {
    const uint8_t type = static_cast<uint8_t>((status >> 4) & 0x0f);
    switch (type) {
    case 0x08:
        return MidiEventType::NoteOff;
    case 0x09:
        return MidiEventType::NoteOn;
    case 0x0a:
        return MidiEventType::PolyAftertouch;
    case 0x0b:
        return (data1 >= 120 && data1 <= 127) ? MidiEventType::ChannelMode :
                                                MidiEventType::ControlChange;
    case 0x0c:
        return MidiEventType::ProgramChange;
    case 0x0d:
        return MidiEventType::ChannelAftertouch;
    case 0x0e:
        return MidiEventType::PitchBend;
    default:
        return MidiEventType::ControlChange;
    }
}

}  // namespace

bool MidiParser::TryParseEvent(const uint8_t* raw, uint8_t len, MidiEvent& out) {
    if (raw == nullptr || len == 0) {
        return false;
    }

    const uint8_t status = raw[0];
    if (status < 0x80 || status >= 0xf0) {
        return false;
    }

    const uint8_t size = message_size_for_status(status);
    if (size == 0 || len < size) {
        return false;
    }

    out.type         = event_type_for_status(status, raw[1]);
    out.channel      = static_cast<uint8_t>(status & 0x0f);
    out.data1        = raw[1];
    out.data2        = (size == 3) ? raw[2] : 0;
    out.size         = size;
    out.timestamp_us = 0;
    return true;
}

bool MidiParser::IsSysEx(const uint8_t* raw, uint8_t len) {
    return raw != nullptr && len > 0 && raw[0] == 0xf0;
}

bool MidiParser::IsRealtimeStatus(uint8_t status) {
    return status >= 0xf8;
}
