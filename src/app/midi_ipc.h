//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <atomic>
#include <cstdint>
#include "FreeRTOS.h"
#include "queue.h"
#include "MidiMessage.h"

/** @brief CC / PB 等（ベストエフォート即時処理） */
extern QueueHandle_t gMidiEventQueue;
/** @brief NoteOn / NoteOff（時刻スケジュール） */
extern QueueHandle_t gMidiNoteQueue;
extern QueueHandle_t gMidiControlQueue;

extern volatile uint16_t gPanelChannelBitmap;   // パネルのチャンネルスイッチ状態 (bit=1: ON, 0: OFF)
extern volatile uint16_t gLastNoteOnBitmap;     // 各チャンネルの発音中ビットマップ (bit=1: 発音中)

// Reset が gMidiControlQueue 満杯時に消失するのを防ぐフォールバックフラグ。
// MidiIpcSendMidiControl() が Reset のキュー送信に失敗した場合のみ true にセットされ、
// MidiEngineTask がキューをドレインした後に確認してクリアする。
extern std::atomic<bool> gPendingReset;

struct MidiIpcStats {
    uint32_t midi_event_queue_drop_count;
    uint32_t midi_note_queue_drop_count;
    uint32_t midi_control_queue_drop_count;
    uint32_t midi_reset_queue_drop_count;
    /** NoteOff 優先のため満杯時に追い出した NoteOn 数 */
    uint32_t midi_note_on_evict_count;
    /** キュー満杯時に pending ビットマップへ退避した NoteOff 数 */
    uint32_t midi_note_off_fallback_count;
};

/**
 * @brief MIDI 用タスク間通信 (MIDIイベントキューとコントロールイベントキュー) の初期化
 * @return 初期化成功なら true、失敗なら false
 * @details MidiEngineTask と UsbMidiTask の起動前に呼び出しておくこと。
 *          失敗した場合はキューが nullptr のままになるので、以降のキュー使用はすべて失敗する。
 */
bool MidiIpcInitialize();

/**
 * @brief エフェクト用 MIDI イベントキューに送信（NoteOn/Off 以外）
 */
bool MidiIpcSendMidiEvent(const MidiEvent& event);

/**
 * @brief NoteOn/Off 用キューに送信（timestamp_us は受信側で設定済みであること）
 * @details NoteOff は満杯時も Drop せず、NoteOn 追い出しまたは pending 退避で必ず届ける。
 */
bool MidiIpcSendMidiNoteEvent(const MidiEvent& event);

using MidiPendingNoteOffFn = void (*)(uint8_t channel, uint8_t key, void* ctx);

/**
 * @brief pending 退避された NoteOff をすべて処理する（gMidiNoteQueue が空のときのみ呼ぶ）
 * @return 処理した NoteOff 数
 */
size_t MidiIpcDrainPendingNoteOffs(MidiPendingNoteOffFn fn, void* ctx);

/**
 * @brief MIDIコントロールイベントキューに MIDIコントロールイベントを送信する
 * @param event 送信する MIDIコントロールイベント
 * @return 送信成功なら true、キューが満杯などで送信できない場合は false
 */
bool MidiIpcSendMidiControl(const MidiControlEvent& event);

/**
 * @brief MIDI IPC の統計情報を取得する
 * @return 統計情報を格納した MidiIpcStats 構造体
 */
MidiIpcStats MidiIpcGetStats();
