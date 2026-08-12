//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "usb_midi_task.h"
#include <cstdint>
#include "FreeRTOS.h"
#include "task.h"
#include "midi_ipc.h"
#include "tusb.h"
#include "config.h"
#include "debugger.h"
#include "MidiStreamAssembler.h"

#include "pico/time.h"

namespace {

// バイトストリーム組立（runningStatus管理・SysEx組立・暗黙終了）は
// MidiStreamAssembler(src/midi/)に切り出し済み。ここではIPC送信・
// Debugger呼び出しなどpico-sdk/FreeRTOS依存の副作用だけを担う。
class UsbMidiStreamSink : public IMidiStreamSink {
public:
    void OnMidiEvent(const MidiEvent& event) override {
        MidiEvent evt = event;
        if (MidiEventIsNote(evt)) {
            evt.timestamp_us = static_cast<uint32_t>(time_us_64());
            (void)MidiIpcSendMidiNoteEvent(evt);
        } else {
            (void)MidiIpcSendMidiEvent(evt);
        }
    }

    void OnProfileReset() override {
        MidiControlEvent ctl{};
        ctl.type = MidiControlType::Reset;
        ctl.channel = 0;
        ctl.timestamp_us = 0;
        (void)MidiIpcSendMidiControl(ctl);
    }

    void OnVendorSysEx(const uint8_t* raw, uint16_t len) override {
        Debugger::HandleSysEx(raw, len);
    }
};

}  // namespace

void UsbMidiTask(void* /*param*/) {
    static UsbMidiStreamSink sink;
    static MidiStreamAssembler assembler(sink);
    Debugger::gMidiMode = true;         // MIDI処理を有効化

    for (;;) {
#if USB_MIDI_IRQ_DRIVEN
        // FreeRTOS OSAL時はUSBイベントを最大1ms待機してから処理へ進む。
        // OPT_OS_PICO時の従来動作は下の tud_task() 側で維持する。
        tud_task_ext(1, false);
#else
        tud_task();
#endif
        if (!Debugger::gMidiMode) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (tud_midi_n_available(0, 0)) {
            uint8_t buffer[32] = {0};
            const int len = tud_midi_n_stream_read(0, 0, buffer, sizeof(buffer));
            if (len > 0) {
                for (int i = 0; i < len; ++i) {
                    assembler.PushByte(buffer[i]);
                }
            }
            // データが残っていれば次のループで即処理
        } else {
#if USB_MIDI_IRQ_DRIVEN
            // IRQ駆動モードでは tud_task_ext() が待機済みなので追加の譲渡は不要
#else
            // RXFIFOが空なので BLOCKED 状態に入り、低優先度タスクが CPU を得られるようにする。
            // taskYIELD() は同優先度以上にしか譲渡しないため MidiPanelTask がタスクスタベーションする。
            vTaskDelay(pdMS_TO_TICKS(1));
#endif
        }
    }
}
