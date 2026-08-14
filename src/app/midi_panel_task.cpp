//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>

#include "midi_panel_task.h"
#include "FreeRTOS.h"
#include "MidiMessage.h"
#include "debugger.h"
#include "midi_ipc.h"
#include "task.h"
#include "task_config.h"

void MidiPanelTask(void* param) {
    auto* ctx = static_cast<MidiPanelTaskContext*>(param);
    TickType_t lastWake = xTaskGetTickCount();
    bool prevMidiReset = false;
    uint32_t lastSeenResetPulseSeq = gResetPulseSeq;

    for (;;) {
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(MIDI_PANEL_PERIOD_MS));

        if (!Debugger::gMidiPanelMode || !ctx->panel->IsConnected()) {
            continue;
        }

        // MIDI Reset 処理完了通知を監視し、LED点滅をトリガーする
        const uint32_t resetPulseSeq = gResetPulseSeq;
        if (resetPulseSeq != lastSeenResetPulseSeq) {
            lastSeenResetPulseSeq = resetPulseSeq;
            ctx->panel->FlashAllLeds();
        }

        // 1周期分のパネル処理を実行する
        ctx->panel->Tick(gLastNoteOnBitmap);

        // パネルのトグル状態をグローバル変数に反映する
        gPanelChannelBitmap = ctx->panel->GetChannelEnableBitmap();

        // MIDI Reset ボタンがトリガされたら、MIDI Control Event を送信する
        const bool midiReset = ctx->panel->IsMidiReset();
        if (midiReset && !prevMidiReset) {
            MidiControlEvent ctl{};
            ctl.type = MidiControlType::Reset;
            ctl.channel = 0;
            ctl.timestamp_us = 0;
            (void)MidiIpcSendMidiControl(ctl);
            std::printf("MIDI Reset\n");
        }
        prevMidiReset = midiReset;
    }
}
