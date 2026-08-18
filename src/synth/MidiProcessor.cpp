//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>
#include "MidiController.h"
#include "MidiProcessor.h"
#include "debugger.h"
#include "VoiceAllocator.h"

MidiProcessor::MidiProcessor(std::array<MidiChannel*, MIDI_CHANNELS>& channels)
    : channels(channels),
      channel_enable_bits(0xffff),
      note_on_bits(0) {
}

MidiProcessor::~MidiProcessor() {
}

void MidiProcessor::refresh_note_on_bit(uint16_t bit, MidiChannel* ch) {
    // チャンネルの発音状態に応じてNoteOnビットを更新
    if (ch->IsActive()) {
        note_on_bits |= bit;
    } else {
        note_on_bits &= static_cast<uint16_t>(~bit);
    }
}

void MidiProcessor::SetChannelEnable(uint16_t bits) {
    // 直前までONだったが今回OFFになったチャンネルのbitを抽出する
    const uint16_t newly_disabled = channel_enable_bits & static_cast<uint16_t>(~bits);

    // LSBから順にスキャンし、無効化されたチャンネルのNoteをすべてOFFにする
    uint16_t scan = newly_disabled;
    while (scan != 0) {
        // LSBから連続する0の数をカウント => チャンネル番号(0-15)に対応
        const int ch = __builtin_ctz(scan);

        // 検出したチャンネルのNoteをすべてOFFにする
        channels[ch]->AllNoteOff();

        // そのチャンネルのNoteOnビットをOFFに更新
        const uint16_t ch_bit = static_cast<uint16_t>(1u << ch);
        refresh_note_on_bit(ch_bit, channels[ch]);

        // そのチャンネルのbitをクリアし、次のチャンネルへ
        scan &= static_cast<uint16_t>(scan - 1);
    }

    channel_enable_bits = bits;
}

void MidiProcessor::Reset() {
    // 全Voicesリセット(MIDI Channelより先に行う)
    VoiceAllocator::GetInstance().Reset();

    // 全MIDI Channelsリセット
    for (auto& channel : channels) {
        channel->Reset();
    }

    // NoteOn状態のリセット
    note_on_bits = 0;
}

//
//  MIDIイベントのパースと実行
//
uint16_t MidiProcessor::Exec(const MidiEvent& evt) {
    DPRINTF(3, "ch=%2d %02x %02x", evt.channel, evt.data1, evt.data2);
    DPRINTF(3, " |");

    if (!IsSupportedMidiEvent(evt)) {
        DPRINTF(3, "unsupported\n");
        return note_on_bits;
    }

    // 処理対象のチャンネルかチェック
    const uint8_t ch = evt.channel;
    if (ch >= MIDI_CHANNELS) {
        DPRINTF(3, "invalid ch=%d\n", ch);
        return note_on_bits;
    }
    const uint16_t mask = static_cast<uint16_t>(1u << ch);
    if ((channel_enable_bits & mask) == 0) {
        DPRINTF(3, "\n");
        return note_on_bits;
    }

    MidiChannel* channel = channels[ch];

    DPRINTF(3, "CH:%02d | ", ch);

    switch (evt.type) {
    case MidiEventType::NoteOn:
        channel->NoteOn(evt.data1, evt.data2);
        refresh_note_on_bit(mask, channel);
        DPRINTF(3, "ON : k=%3d v=%3d", evt.data1, evt.data2);
        break;
    case MidiEventType::NoteOff:
        channel->NoteOff(evt.data1);
        refresh_note_on_bit(mask, channel);
        DPRINTF(3, "OFF: k=%d v=%d", evt.data1, evt.data2);
        break;
    case MidiEventType::ProgramChange:
        channel->SetProgram(evt.data1);
        DPRINTF(3, "PROG: %d", evt.data1);
        break;
    case MidiEventType::ControlChange:
    case MidiEventType::ChannelMode:
        switch (ClassifyMidiController(evt.data1)) {
        case MidiControllerAction::Modulation:
            channel->SetModulation(evt.data2);
            break;
        case MidiControllerAction::Volume:
            channel->SetVolume(evt.data2);
            break;
        case MidiControllerAction::Expression:
            channel->SetExpression(evt.data2);
            break;
        case MidiControllerAction::Hold1:
            channel->Hold1(evt.data2);
            refresh_note_on_bit(mask, channel);
            break;
        case MidiControllerAction::Tremolo:
            channel->SetTremolo(evt.data2);
            break;
        case MidiControllerAction::NrpnLsb:
            channel->NRPN_LSB(evt.data2);
            break;
        case MidiControllerAction::NrpnMsb:
            channel->NRPN_MSB(evt.data2);
            break;
        case MidiControllerAction::RpnLsb:
            channel->RPN_LSB(evt.data2);
            break;
        case MidiControllerAction::RpnMsb:
            channel->RPN_MSB(evt.data2);
            break;
        case MidiControllerAction::DataEntryMsb:
            channel->DataEntry_MSB(evt.data2);
            break;
        case MidiControllerAction::DataEntryLsb:
            channel->DataEntry_LSB(evt.data2);
            break;
        case MidiControllerAction::Pan:
            channel->SetPan(evt.data2);
            break;
        case MidiControllerAction::BankSelectMsb:
            channel->BankSelect_MSB(evt.data2);
            break;
        case MidiControllerAction::BankSelectLsb:
            channel->BankSelect_LSB(evt.data2);
            break;
        case MidiControllerAction::AllSoundOff:
            channel->AllNoteOff();
            refresh_note_on_bit(mask, channel);
            break;
        case MidiControllerAction::ResetAllControllers:
            channel->ResetAllController();
            break;
        case MidiControllerAction::AllNotesOff:
            channel->AllNoteOff();
            refresh_note_on_bit(mask, channel);
            break;
        case MidiControllerAction::Unsupported:
            break;
        }
        DPRINTF(5, "CC: #%d/%d", evt.data1, evt.data2);
        break;
    case MidiEventType::PitchBend: {
        int16_t val = evt.data1 + 128 * evt.data2 - 8192;
        channel->PitchBend(val);
        DPRINTF(5, "PB: %d", val);
        break;
        }
    default:
        break;
    }
    DPRINTF(3, "\n");
    return note_on_bits;
}
