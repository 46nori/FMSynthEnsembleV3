//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "MidiPanelController.h"

/** @brief MidiPanelTask に渡すコンテキスト */
struct MidiPanelTaskContext {
    MidiPanelController* panel;  ///< パネルコントローラ（非所有）
};

/**
 * @brief Panel ハードウェアの固定周期スキャンと状態共有
 * @param [in] param MidiPanelTaskContext へのポインタ
 * @details MIDI_PANEL_PERIOD_MS 周期で Tick / gPanelChannelBitmap 更新 / Reset IPC を行う。
 */
void MidiPanelTask(void* param);
