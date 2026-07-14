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

    for (;;) {
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(MIDI_PANEL_PERIOD_MS));

        if (!Debugger::gMidiPanelMode || !ctx->panel->IsConnected()) {
            continue;
        }

        ctx->panel->Tick(gLastNoteOnBitmap);

        gPanelChannelBitmap = ctx->panel->GetChannelEnableBitmap();

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
