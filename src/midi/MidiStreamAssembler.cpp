//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "MidiStreamAssembler.h"

#include "MidiParser.h"
#include "MidiRoutingPolicy.h"

void MidiStreamAssembler::ResetSysEx() {
    inSysEx_ = false;
    sysExOverflow_ = false;
    sysExLength_ = 0;
}

void MidiStreamAssembler::HandleCompleteSysEx() {
    if (sysExLength_ == 0 || sysExOverflow_) {
        return;
    }

    if (MidiRoutingPolicy::IsProfileResetSysEx(sysEx_, sysExLength_)) {
        sink_.OnProfileReset();
        return;
    }

    if (MidiRoutingPolicy::DecideForSysEx(sysEx_, sysExLength_) == MidiRouteDecision::HandleOnCore0) {
        sink_.OnVendorSysEx(sysEx_, sysExLength_);
    }
}

void MidiStreamAssembler::EnqueueEventIfNeeded(const uint8_t* raw, uint8_t len) {
    MidiEvent evt{};
    if (!MidiParser::TryParseEvent(raw, len, evt)) {
        return;
    }
    if (MidiRoutingPolicy::DecideForEvent(evt) != MidiRouteDecision::ForwardToEngine) {
        return;
    }
    sink_.OnMidiEvent(evt);
}

void MidiStreamAssembler::HandleDataByte(uint8_t value) {
    if (runningStatus_ == 0 || expectedLength_ < 2) {
        return;
    }

    msg_[msgLength_++] = value;
    if (msgLength_ < expectedLength_) {
        return;
    }

    EnqueueEventIfNeeded(msg_, expectedLength_);

    // Keep running status for the next data bytes.
    msg_[0] = runningStatus_;
    msgLength_ = 1;
}

void MidiStreamAssembler::HandleStatusByte(uint8_t status) {
    if (MidiParser::IsRealtimeStatus(status)) {
        return;
    }

    if (status == 0xf0) {
        inSysEx_ = true;
        sysExOverflow_ = false;
        sysExLength_ = 0;
        sysEx_[sysExLength_++] = status;
        runningStatus_ = 0;
        msgLength_ = 0;
        expectedLength_ = 0;
        return;
    }

    if (status >= 0xf0) {
        runningStatus_ = 0;
        msgLength_ = 0;
        expectedLength_ = 0;
        return;
    }

    runningStatus_ = status;
    expectedLength_ = MidiParser::MessageSizeForStatus(status);
    msg_[0] = status;
    msgLength_ = 1;
}

void MidiStreamAssembler::PushByte(uint8_t value) {
    // Realtime messageはSysEx中でも独立に挿入され得るため、
    // SysExバッファには積まずここで破棄する。
    if ((value & 0x80) && MidiParser::IsRealtimeStatus(value)) {
        return;
    }

    if (inSysEx_) {
        // MIDI 1.0 仕様: SysEx 受信中に非リアルタイムステータス (0x80–0xF6) が来た場合、
        // SysEx を暗黙終了させ、受信バイトを新規メッセージの先頭として処理する。
        if ((value & 0x80) && value != 0xf7) {
            ResetSysEx();
            HandleStatusByte(value);
            return;
        }
        if (sysExLength_ < kMaxSysExLength) {
            sysEx_[sysExLength_++] = value;
        } else {
            sysExOverflow_ = true;
        }

        if (value == 0xf7) {
            HandleCompleteSysEx();
            ResetSysEx();
        }
        return;
    }

    if (value & 0x80) {
        HandleStatusByte(value);
    } else {
        HandleDataByte(value);
    }
}
