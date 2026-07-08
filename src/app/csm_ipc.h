//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>
#include "FreeRTOS.h"
#include "queue.h"

enum class CsmEventType : uint8_t {
    FrameTick,
    Start,
    Stop,
};

struct CsmEvent {
    CsmEventType type;
    uint32_t generation;
    int note;
    int32_t program;
    int volume;
    uint8_t lr;
};

extern QueueHandle_t gCsmEventQueue;

/**
 * @brief CSM IPC初期化
 * @details CSM IPCを初期化する。
 */
bool CsmIpcInitialize();

/**
 * @brief CSMイベントを受信
 * @details CsmFrameTask からのみ呼ぶ。
 */
bool CsmIpcReceive(CsmEvent& event, TickType_t wait_ticks);

/**
 * @brief Startイベントの処理完了を通知
 * @details CsmFrameTask から Start 処理後に呼び、旧Timer由来tickの破棄期間を終える。
 */
void CsmIpcNotifyStartProcessed(uint32_t generation);

/**
 * @brief FM_IRQ割り込みによるフレーム更新通知
 * @details ISR からのみ呼ぶ（FM IRQ）。FrameTick イベントをキューに投入する。
 */
void CsmSignalFrameTick(void);

/**
 * @brief MidiEngineTask から停止を要求
 * @details MidiEngineTask からの停止（NoteOff / Resetなど）
 */
void CsmSignalStop(void);

/**
 * @brief MidiEngineTask から初回フレーム処理を依頼
 * @details MidiEngineTask からの初回CSMフレーム処理を開始
 */
void CsmSignalStart(int note, int32_t program, int volume, uint8_t lr);
