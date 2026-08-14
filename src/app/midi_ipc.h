//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <atomic>
#if ENABLE_MIDI_TIMING_STATS
#include <cstddef>
#endif
#include <cstdint>
#include "FreeRTOS.h"
#include "queue.h"
#include "MidiMessage.h"

/** @brief Core1 で処理する MIDI イベント（到着順の単一 FIFO） */
extern QueueHandle_t gMidiQueue;
extern QueueHandle_t gMidiControlQueue;

extern volatile uint16_t gPanelChannelBitmap;   // パネルのチャンネルスイッチ状態 (bit=1: ON, 0: OFF)
extern volatile uint16_t gLastNoteOnBitmap;     // 各チャンネルの発音中ビットマップ (bit=1: 発音中)

// Reset が gMidiControlQueue 満杯時に消失するのを防ぐフォールバックフラグ。
// MidiIpcSendMidiControl() が Reset のキュー送信に失敗した場合のみ true にセットされ、
// MidiEngineTask がキューをドレインした後に確認してクリアする。
extern std::atomic<bool> gPendingReset;

#if ENABLE_MIDI_TIMING_STATS
constexpr size_t kMidiTimingOutlierCount = 8;

struct MidiTimingSample {
    uint32_t      queue_delay_us;
    uint32_t      execution_us;
    uint16_t      queue_depth;
    MidiEventType type;
    uint8_t       channel;
    uint8_t       data1;
    uint8_t       data2;
};
#endif

struct MidiIpcStats {
    uint32_t midi_queue_drop_count;
    uint32_t midi_control_queue_drop_count;
    uint32_t midi_reset_queue_drop_count;
#if ENABLE_MIDI_TIMING_STATS
    uint16_t midi_queue_high_water_mark;
    uint32_t midi_note_queue_delay_max_us;
    uint32_t midi_note_off_queue_delay_max_us;
    uint32_t midi_effect_queue_delay_max_us;
    uint32_t midi_note_execution_max_us;
    uint32_t midi_effect_execution_max_us;
    uint32_t midi_vibrato_execution_max_us;
    MidiTimingSample delay_outliers[kMidiTimingOutlierCount];
    MidiTimingSample execution_outliers[kMidiTimingOutlierCount];
#endif
};

/**
 * @brief MIDI 用タスク間通信 (MIDIイベントキューとコントロールイベントキュー) の初期化
 * @return 初期化成功なら true、失敗なら false
 * @details MidiEngineTask と UsbMidiTask の起動前に呼び出しておくこと。
 *          失敗した場合はキューが nullptr のままになるので、以降のキュー使用はすべて失敗する。
 */
bool MidiIpcInitialize();

/**
 * @brief Core1 で処理する MIDI イベントを到着順の単一キューへ送信
 */
bool MidiIpcSendMidiEvent(const MidiEvent& event);

#if ENABLE_MIDI_TIMING_STATS
/**
 * @brief Core1 でのキュー滞留時間とイベント実行時間を記録
 * @details MidiEngineTask の単一 Consumer からのみ呼び出す。
 */
void MidiIpcRecordMidiEventTiming(const MidiEvent& event, UBaseType_t queue_depth,
                                  uint32_t dequeue_us, uint32_t completed_us);

/**
 * @brief Core1 のビブラート1周期分の実行時間を記録
 * @details MidiEngineTask の単一 Consumer からのみ呼び出す。
 */
void MidiIpcRecordVibratoTiming(uint32_t execution_us);
#endif

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
