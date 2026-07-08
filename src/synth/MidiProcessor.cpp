//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>
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
        switch (evt.data1) {
        case 1:  // Modulation
            channel->SetModulation(evt.data2);
            break;
        case 7:  // Volume
            channel->SetVolume(evt.data2);
            break;
        case 11:  // Expression
            channel->SetExpression(evt.data2);
            break;
        case 64:  // Hold1
            channel->Hold1(evt.data2);
            refresh_note_on_bit(mask, channel);
            break;
        case 98:  // NRPN LSB
            channel->NRPN_LSB(evt.data2);
            break;
        case 99:  // NRPN MSB
            channel->NRPN_MSB(evt.data2);
            break;
        case 100:  // RPN LSB
            channel->RPN_LSB(evt.data2);
            break;
        case 101:  // RPN MSB
            channel->RPN_MSB(evt.data2);
            break;
        case 6:  // Data entry MSB
            channel->DataEntry_MSB(evt.data2);
            break;
        case 38:  // Data entry LSB
            channel->DataEntry_LSB(evt.data2);
            break;
        case 10:  // Pan
            channel->SetPan(evt.data2);
            break;
        case 0:  // Bank select MSB
            channel->BankSelect_MSB(evt.data2);
            break;
        case 32:  // Bank select LSB
            channel->BankSelect_LSB(evt.data2);
            break;
        case 120:  // All Sound Off
            channel->AllNoteOff();
            refresh_note_on_bit(mask, channel);
            break;
        case 121:  // Reset all controller
            channel->ResetAllController();
            break;
        case 123:  // All Note Off
            channel->AllNoteOff();
            refresh_note_on_bit(mask, channel);
            break;
        }
        DPRINTF(5, "CC: #%d/%d", evt.data1, evt.data2);
        break;
    case MidiEventType::PolyAftertouch:
        DPRINTF(5, "PolyPress: key=%d velocity=%d", evt.data1, evt.data2);
        break;
    case MidiEventType::ChannelAftertouch:
        DPRINTF(5, "ChPress: %d", evt.data1);
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
