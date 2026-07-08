//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include <array>
#include "MidiMessage.h"
#include "MidiFactory.h"

/**
 * @brief MidiProcessor class
 */
class MidiProcessor {
private:
    std::array<MidiChannel*, MIDI_CHANNELS>& channels;
    uint16_t channel_enable_bits;  // 有効チャンネルのビットマップ (bit=1: 有効, 0: 無効)
    uint16_t note_on_bits;         // NoteOn中チャンネルのビットマップ (bit=1: 発音中)

public:
    /**
     * @brief コンストラクタ
     * @param channels MIDIチャンネルの配列
     */
    MidiProcessor(std::array<MidiChannel*, MIDI_CHANNELS>& channels);
    MidiProcessor() = delete;

    /**
     * @brief デストラクタ
     */
    ~MidiProcessor();

    /**
     * @brief チャンネル有効ビットを設定する
     * @details 新たに無効になったチャンネルは AllNoteOff を実行する
     * @param bits  チャンネル有効ビットマップ (bit=1: 有効, 0: 無効)
     */
    void SetChannelEnable(uint16_t bits);

    /**
     * @brief MIDIメッセージ処理
     * @return MIDI ChannelのNoteOn状態のビットマップ
     */
    uint16_t Exec(const MidiEvent& evt);

    /**
     * @brief 全MIDIチャンネルのリセット
     */
    void Reset();

    /** @brief チャンネル有効ビットマップ (bit=1: 有効) */
    uint16_t GetChannelEnableBits() const {
        return channel_enable_bits;
    }

private:
    /**
     * @brief 指定チャンネルの NoteOn ビットをチャンネルの実際の発音状態に合わせて更新する
     * @param bit  対象チャンネルのビット (1u << ch)
     * @param ch   対象チャンネル
     */
    void refresh_note_on_bit(uint16_t bit, MidiChannel* ch);
};
