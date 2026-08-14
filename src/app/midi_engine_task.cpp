//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>

#include "midi_engine_task.h"
#include "RhythmChannel.h"
#include "NoteChannel.h"
#include "OpnBase.h"
#include "VoiceAllocator.h"
#include "config.h"
#include "debugger.h"
#include "midi_ipc.h"
#include "MidiMessage.h"
#include "task_config.h"

#include "FreeRTOS.h"
#include "pico/time.h"
#include "task.h"

namespace {

uint32_t VibratoPeriodUs() {
    return static_cast<uint32_t>(VIBRATO_PERIOD_MS) * 1000u;
}

/** @brief now_us >= target_us（uint32 ラップを考慮） */
bool TimeReached(uint32_t target_us, uint32_t now_us) {
    return static_cast<uint32_t>(now_us - target_us) < 0x80000000u;
}

// CH10 パネル LED: リズム vel>0 のヒットを短時間保持（FM の IsActive モデルは使わない）
constexpr uint16_t kCh10LedBit =
    static_cast<uint16_t>(1u << RhythmChannel::MIDI_RHYTHM_CHANNEL);
constexpr uint32_t kCh10LedHoldUs =
    MIDI_PANEL_PERIOD_MS * 4u * 2u * 1000u;  // 2× LED スキャンフレーム

uint16_t engine_note_on_bits = 0;
uint32_t ch10_led_hold_until_us = 0;

bool Ch10LedHoldActive(uint32_t now_us) {
    if (ch10_led_hold_until_us == 0) {
        return false;
    }
    return static_cast<uint32_t>(ch10_led_hold_until_us - now_us) < 0x80000000u;
}

void ApplyPanelNoteOnBitmap(uint32_t now_us) {
    uint16_t bits = engine_note_on_bits;
    if ((gPanelChannelBitmap & kCh10LedBit) != 0 && Ch10LedHoldActive(now_us)) {
        bits = static_cast<uint16_t>(bits | kCh10LedBit);
    } else {
        bits = static_cast<uint16_t>(bits & ~kCh10LedBit);
    }
    gLastNoteOnBitmap = static_cast<uint16_t>(bits & gPanelChannelBitmap);
}

void OnChannelEnableChanged(uint16_t new_bits, uint16_t old_bits) {
    if ((old_bits & static_cast<uint16_t>(~new_bits) & kCh10LedBit) != 0) {
        ch10_led_hold_until_us = 0;
    }
}

void ResetPanelNoteOnState() {
    engine_note_on_bits = 0;
    ch10_led_hold_until_us = 0;
    gLastNoteOnBitmap = 0;
    ++gResetPulseSeq;
}

void OnRhythmPanelLedEvent(const MidiEvent& evt, uint32_t now_us) {
    if (evt.channel != RhythmChannel::MIDI_RHYTHM_CHANNEL) {
        return;
    }
    if (evt.type == MidiEventType::NoteOn && evt.data2 > 0) {
        const uint32_t hit_us = evt.timestamp_us != 0 ? evt.timestamp_us : now_us;
        ch10_led_hold_until_us = hit_us + kCh10LedHoldUs;
    } else if ((evt.type == MidiEventType::ControlChange ||
                evt.type == MidiEventType::ChannelMode) &&
               (evt.data1 == 120 || evt.data1 == 123)) {
        ch10_led_hold_until_us = 0;
    }
}

void ExecMidiEvent(MidiEngineTaskContext* ctx, const MidiEvent& evt, uint32_t now_us) {
#if ENABLE_MIDI_TIMING_STATS
    const uint32_t dequeue_us = static_cast<uint32_t>(time_us_64());
    const UBaseType_t queue_depth = uxQueueMessagesWaiting(gMidiQueue) + 1;
#endif
    const uint16_t ch_mask =
        static_cast<uint16_t>(1u << static_cast<unsigned>(evt.channel));
    const bool ch_enabled =
        (ctx->processor->GetChannelEnableBits() & ch_mask) != 0;

    engine_note_on_bits = ctx->processor->Exec(evt);

    if (ch_enabled) {
        OnRhythmPanelLedEvent(evt, now_us);
    }
    ApplyPanelNoteOnBitmap(now_us);
#if ENABLE_MIDI_TIMING_STATS
    MidiIpcRecordMidiEventTiming(evt, queue_depth, dequeue_us,
                                 static_cast<uint32_t>(time_us_64()));
#endif
}

void RunVibrato(MidiEngineTaskContext* ctx, uint32_t phase_ticks) {
    const uint16_t enable_bits = ctx->processor->GetChannelEnableBits();
    for (int ch = 0; ch < MIDI_CHANNELS; ++ch) {
        if (ch == RhythmChannel::MIDI_RHYTHM_CHANNEL) {
            continue;
        }
        if ((enable_bits & (1u << ch)) == 0) {
            continue;
        }
        (*ctx->channels)[ch]->TickVibrato(phase_ticks);
    }
}

void ServiceVibratoIfDue(MidiEngineTaskContext* ctx, uint32_t& next_vibrato_us) {
    const uint32_t now_us = static_cast<uint32_t>(time_us_64());
    if (!TimeReached(next_vibrato_us, now_us)) {
        return;
    }

    const uint32_t period = VibratoPeriodUs();
    RunVibrato(ctx, 1u);
#if ENABLE_MIDI_TIMING_STATS
    MidiIpcRecordVibratoTiming(static_cast<uint32_t>(time_us_64()) - now_us);
#endif
    // 軽い遅れは周期を維持、大きい遅れのみ再同期（LFO のうねりを保つ）
    if ((now_us - next_vibrato_us) >= period) {
        next_vibrato_us = now_us + period;
    } else {
        next_vibrato_us += period;
    }
}

size_t DrainMidiQueue(MidiEngineTaskContext* ctx, size_t max_batch, uint32_t now_us) {
    MidiEvent evt{};
    size_t    count = 0;
    while (count < max_batch && xQueueReceive(gMidiQueue, &evt, 0) == pdTRUE) {
        const uint32_t event_now_us = evt.timestamp_us != 0 ? evt.timestamp_us : now_us;
        ExecMidiEvent(ctx, evt, event_now_us);
        ++count;
    }
    return count;
}

void RefreshAllNoteChannelPitch(MidiEngineTaskContext* ctx) {
    for (int ch = 0; ch < MIDI_CHANNELS; ++ch) {
        if (ch == RhythmChannel::MIDI_RHYTHM_CHANNEL) {
            continue;
        }
        (*ctx->channels)[ch]->RefreshPitch();
    }
}

void DumpProgram(MidiEngineTaskContext* ctx) {
    std::printf("\nCH   ");
    for (int i = 1; i <= 16; i++) {
        std::printf("%5d ", i);
    }
    std::printf("\nBANK ");
    for (int i = 0; i < MIDI_CHANNELS; i++) {
        const uint32_t bk = (*ctx->channels)[i]->GetProgram();
        std::printf("%5u ", (bk >> 16) & 0xffffu);
    }
    std::printf("\nPG   ");
    for (int i = 0; i < MIDI_CHANNELS; i++) {
        const uint32_t bk = (*ctx->channels)[i]->GetProgram();
        std::printf("%5u ", bk & 0x7fu);
    }
    std::printf("\n");
}

void handle_control_event(const MidiControlEvent& ctl, MidiEngineTaskContext* ctx) {
    switch (ctl.type) {
    case MidiControlType::Reset:
        ctx->processor->Reset();
        ResetPanelNoteOnState();
        break;
    case MidiControlType::DebugDumpChannel:
        if (ctl.channel == 0xff) {
            for (auto* ch : *ctx->channels) {
                ch->dump();
            }
        } else if (ctl.channel < MIDI_CHANNELS) {
            (*ctx->channels)[ctl.channel]->dump();
        }
        break;
    case MidiControlType::DebugDumpVoice:
        VoiceAllocator::GetInstance().dump();
        break;
    case MidiControlType::DebugDumpProgram:
        DumpProgram(ctx);
        break;
    case MidiControlType::DebugStats:
        Debugger::PrintMidiStats(*ctx->channels);
        break;
    case MidiControlType::DebugVibratoOverride:
        if (ctl.channel <= static_cast<uint8_t>(VibOverride::Auto)) {
            g_vib_override = static_cast<VibOverride>(ctl.channel);
        }
        RefreshAllNoteChannelPitch(ctx);
        break;
    case MidiControlType::DebugTlTrim:
        OpnBase::SetTLTrimEnabled(ctl.channel != 0);
        for (auto* ch : *ctx->channels) {
            ch->RefreshActiveFmVolume();
        }
        break;
    case MidiControlType::DebugRhythmMix: {
        g_rhythm_level_offset = static_cast<int8_t>(ctl.channel);
        auto* rc = static_cast<RhythmChannel*>(
            (*ctx->channels)[RhythmChannel::MIDI_RHYTHM_CHANNEL]);
        rc->RefreshRhythmLevels();
        break;
    }
    }
}

void HandleControlAndReset(MidiEngineTaskContext* ctx) {
    // load + store(false) だと、その間に Core0 が再度 store(true) した
    // 後発 Reset を消してしまう。取得とクリアを exchange でアトミックにする。
    if (gPendingReset.exchange(false, std::memory_order_acq_rel)) {
        ctx->processor->Reset();
        ResetPanelNoteOnState();
        return;
    }

    MidiControlEvent ctl{};
    if (xQueueReceive(gMidiControlQueue, &ctl, 0) == pdTRUE) {
        handle_control_event(ctl, ctx);
    }
}

bool HasPendingMidiWork() {
    return uxQueueMessagesWaiting(gMidiQueue) > 0;
}

TickType_t MsToTicksCeil(uint32_t ms) {
    const TickType_t ticks = pdMS_TO_TICKS(ms);
    return ticks > 0 ? ticks : 1;
}

}  // namespace

void MidiEngineTask(void* param) {
    auto* ctx = static_cast<MidiEngineTaskContext*>(param);

    MidiEvent  wait_evt{};
    uint16_t   prevChannelBitmap = 0xffff;
    uint32_t   next_vibrato_us    = static_cast<uint32_t>(time_us_64()) + VibratoPeriodUs();

    ctx->processor->SetChannelEnable(gPanelChannelBitmap);
    prevChannelBitmap = gPanelChannelBitmap;

    for (;;) {
        const uint32_t now_us = static_cast<uint32_t>(time_us_64());

        const uint16_t channelBitmap = gPanelChannelBitmap;
        if (channelBitmap != prevChannelBitmap) {
            OnChannelEnableChanged(channelBitmap, prevChannelBitmap);
            ctx->processor->SetChannelEnable(channelBitmap);
            prevChannelBitmap = channelBitmap;
            ApplyPanelNoteOnBitmap(now_us);
        }

        (void)DrainMidiQueue(ctx, MIDI_EVENT_BATCH_MAX, now_us);
        HandleControlAndReset(ctx);
        ServiceVibratoIfDue(ctx, next_vibrato_us);

        if (HasPendingMidiWork()) {
            ApplyPanelNoteOnBitmap(now_us);
            continue;
        }

        uint32_t wait_now_us = now_us;
        uint32_t wait_ms     = 1u;
        if (!TimeReached(next_vibrato_us, wait_now_us)) {
            wait_ms = (next_vibrato_us - wait_now_us + 999u) / 1000u;
            if (wait_ms < 1u) {
                wait_ms = 1u;
            }
            if (wait_ms > static_cast<uint32_t>(VIBRATO_PERIOD_MS)) {
                wait_ms = static_cast<uint32_t>(VIBRATO_PERIOD_MS);
            }
        }

        if (xQueueReceive(gMidiQueue, &wait_evt, MsToTicksCeil(wait_ms)) == pdTRUE) {
            const uint32_t event_now_us =
                wait_evt.timestamp_us != 0 ? wait_evt.timestamp_us : now_us;
            ExecMidiEvent(ctx, wait_evt, event_now_us);
            (void)DrainMidiQueue(ctx, MIDI_EVENT_BATCH_MAX - 1,
                                 static_cast<uint32_t>(time_us_64()));
        }

        ApplyPanelNoteOnBitmap(static_cast<uint32_t>(time_us_64()));
    }
}
