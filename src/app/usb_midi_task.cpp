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
#include "MidiParser.h"
#include "MidiRoutingPolicy.h"

#include "pico/time.h"

namespace {

constexpr uint16_t kMaxSysExLength = 256;

struct StreamState {
    bool inSysEx = false;
    bool sysExOverflow = false;
    uint16_t sysExLength = 0;
    uint8_t sysEx[kMaxSysExLength] = {0};

    uint8_t runningStatus = 0;
    uint8_t msg[3] = {0};
    uint8_t msgLength = 0;
    uint8_t expectedLength = 0;
};

uint8_t expected_length_for_status(uint8_t status) {
    const uint8_t high = static_cast<uint8_t>(status & 0xf0);
    if (high == 0xc0 || high == 0xd0) {
        return 2;
    }
    if (high >= 0x80 && high <= 0xe0) {
        return 3;
    }
    return 0;
}

void reset_sysex(StreamState& st) {
    st.inSysEx = false;
    st.sysExOverflow = false;
    st.sysExLength = 0;
}

void handle_complete_sysex(const StreamState& st) {
    if (st.sysExLength == 0 || st.sysExOverflow) {
        return;
    }

    if (MidiRoutingPolicy::IsProfileResetSysEx(st.sysEx, st.sysExLength)) {
        MidiControlEvent ctl{};
        ctl.type = MidiControlType::Reset;
        ctl.channel = 0;
        ctl.timestamp_us = 0;
        (void)MidiIpcSendMidiControl(ctl);
        return;
    }

    if (MidiRoutingPolicy::DecideForSysEx(st.sysEx, st.sysExLength) ==
        MidiRouteDecision::HandleOnCore0) {
        Debugger::HandleSysEx(st.sysEx, st.sysExLength);
    }
}

void enqueue_event_if_needed(const uint8_t* raw, uint8_t len) {
    MidiEvent evt{};
    if (!MidiParser::TryParseEvent(raw, len, evt)) {
        return;
    }
    if (MidiRoutingPolicy::DecideForEvent(evt) != MidiRouteDecision::ForwardToEngine) {
        return;
    }
    if (MidiEventIsNote(evt)) {
        evt.timestamp_us = static_cast<uint32_t>(time_us_64());
        (void)MidiIpcSendMidiNoteEvent(evt);
    } else {
        (void)MidiIpcSendMidiEvent(evt);
    }
}

void handle_data_byte(StreamState& st, uint8_t value) {
    if (st.runningStatus == 0 || st.expectedLength < 2) {
        return;
    }

    st.msg[st.msgLength++] = value;
    if (st.msgLength < st.expectedLength) {
        return;
    }

    enqueue_event_if_needed(st.msg, st.expectedLength);

    // Keep running status for the next data bytes.
    st.msg[0] = st.runningStatus;
    st.msgLength = 1;
}

void handle_status_byte(StreamState& st, uint8_t status) {
    if (MidiParser::IsRealtimeStatus(status)) {
        return;
    }

    if (status == 0xf0) {
        st.inSysEx = true;
        st.sysExOverflow = false;
        st.sysExLength = 0;
        st.sysEx[st.sysExLength++] = status;
        st.runningStatus = 0;
        st.msgLength = 0;
        st.expectedLength = 0;
        return;
    }

    if (status >= 0xf0) {
        st.runningStatus = 0;
        st.msgLength = 0;
        st.expectedLength = 0;
        return;
    }

    st.runningStatus = status;
    st.expectedLength = expected_length_for_status(status);
    st.msg[0] = status;
    st.msgLength = 1;
}

void handle_stream_byte(StreamState& st, uint8_t value) {
    // Realtime messageはSysEx中でも独立に挿入され得るため、
    // SysExバッファには積まずここで破棄する。
    if ((value & 0x80) && MidiParser::IsRealtimeStatus(value)) {
        return;
    }

    if (st.inSysEx) {
        // MIDI 1.0 仕様: SysEx 受信中に非リアルタイムステータス (0x80–0xF6) が来た場合、
        // SysEx を暗黙終了させ、受信バイトを新規メッセージの先頭として処理する。
        if ((value & 0x80) && value != 0xf7) {
            reset_sysex(st);
            handle_status_byte(st, value);
            return;
        }
        if (st.sysExLength < kMaxSysExLength) {
            st.sysEx[st.sysExLength++] = value;
        } else {
            st.sysExOverflow = true;
        }

        if (value == 0xf7) {
            handle_complete_sysex(st);
            reset_sysex(st);
        }
        return;
    }

    if (value & 0x80) {
        handle_status_byte(st, value);
    } else {
        handle_data_byte(st, value);
    }
}

}  // namespace

void UsbMidiTask(void* /*param*/) {
    static StreamState state{};
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
                    handle_stream_byte(state, buffer[i]);
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
