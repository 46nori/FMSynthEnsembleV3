//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

#include "MidiMessage.h"

enum class MidiControllerAction : uint8_t {
    Unsupported,
    BankSelectMsb,
    Modulation,
    DataEntryMsb,
    Volume,
    Pan,
    Expression,
    BankSelectLsb,
    DataEntryLsb,
    Hold1,
    NrpnLsb,
    NrpnMsb,
    RpnLsb,
    RpnMsb,
    AllSoundOff,
    ResetAllControllers,
    AllNotesOff,
};

constexpr MidiControllerAction ClassifyMidiController(uint8_t controller) {
    switch (controller) {
    case 0:   return MidiControllerAction::BankSelectMsb;
    case 1:   return MidiControllerAction::Modulation;
    case 6:   return MidiControllerAction::DataEntryMsb;
    case 7:   return MidiControllerAction::Volume;
    case 10:  return MidiControllerAction::Pan;
    case 11:  return MidiControllerAction::Expression;
    case 32:  return MidiControllerAction::BankSelectLsb;
    case 38:  return MidiControllerAction::DataEntryLsb;
    case 64:  return MidiControllerAction::Hold1;
    case 98:  return MidiControllerAction::NrpnLsb;
    case 99:  return MidiControllerAction::NrpnMsb;
    case 100: return MidiControllerAction::RpnLsb;
    case 101: return MidiControllerAction::RpnMsb;
    case 120: return MidiControllerAction::AllSoundOff;
    case 121: return MidiControllerAction::ResetAllControllers;
    case 123: return MidiControllerAction::AllNotesOff;
    default:  return MidiControllerAction::Unsupported;
    }
}

constexpr bool IsSupportedMidiEvent(const MidiEvent& event) {
    switch (event.type) {
    case MidiEventType::NoteOff:
    case MidiEventType::NoteOn:
    case MidiEventType::ProgramChange:
    case MidiEventType::PitchBend:
        return true;
    case MidiEventType::ControlChange:
    case MidiEventType::ChannelMode:
        return ClassifyMidiController(event.data1) != MidiControllerAction::Unsupported;
    case MidiEventType::PolyAftertouch:
    case MidiEventType::ChannelAftertouch:
        return false;
    }
    return false;
}
