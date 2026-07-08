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
#include "VoiceAllocator.h"
#include "config.h"
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
    const uint16_t ch_mask =
        static_cast<uint16_t>(1u << static_cast<unsigned>(evt.channel));
    const bool ch_enabled =
        (ctx->processor->GetChannelEnableBits() & ch_mask) != 0;

    engine_note_on_bits = ctx->processor->Exec(evt);

    if (ch_enabled) {
        OnRhythmPanelLedEvent(evt, now_us);
    }
    ApplyPanelNoteOnBitmap(now_us);
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
        static_cast<NoteChannel*>((*ctx->channels)[ch])->TickVibrato(phase_ticks);
    }
}

bool ServiceVibratoIfDue(MidiEngineTaskContext* ctx, uint32_t& next_vibrato_us) {
    const uint32_t now_us = static_cast<uint32_t>(time_us_64());
    if (!TimeReached(next_vibrato_us, now_us)) {
        return false;
    }

    const uint32_t period = VibratoPeriodUs();
    RunVibrato(ctx, 1u);
    // 軽い遅れは周期を維持、大きい遅れのみ再同期（LFO のうねりを保つ）
    if ((now_us - next_vibrato_us) >= period) {
        next_vibrato_us = now_us + period;
    } else {
        next_vibrato_us += period;
    }
    return true;
}

constexpr uint32_t kIdleFmReconcilePeriodUs = 50000u;

size_t DrainNoteQueue(MidiEngineTaskContext* ctx, size_t max_batch, uint32_t now_us) {
    MidiEvent evt{};
    size_t    count = 0;
    while (count < max_batch && xQueueReceive(gMidiNoteQueue, &evt, 0) == pdTRUE) {
        const uint32_t event_now_us = evt.timestamp_us != 0 ? evt.timestamp_us : now_us;
        ExecMidiEvent(ctx, evt, event_now_us);
        ++count;
    }
    return count;
}

size_t DrainEffectQueue(MidiEngineTaskContext* ctx, size_t max_batch, uint32_t now_us) {
    MidiEvent evt{};
    size_t    count = 0;
    while (count < max_batch && xQueueReceive(gMidiEventQueue, &evt, 0) == pdTRUE) {
        const uint32_t event_now_us = evt.timestamp_us != 0 ? evt.timestamp_us : now_us;
        ExecMidiEvent(ctx, evt, event_now_us);
        ++count;
    }
    return count;
}

/** @brief Note キューを可能な限り一括処理（全チャンネル共通の再生遅れ軽減） */
size_t DrainAllPendingNotes(MidiEngineTaskContext* ctx, uint32_t now_us) {
    size_t total = 0;
    for (;;) {
        const size_t n = DrainNoteQueue(ctx, MIDI_NOTE_BATCH_MAX, now_us);
        total += n;
        if (n == 0 || n < MIDI_NOTE_BATCH_MAX || total >= MIDI_NOTE_DRAIN_MAX) {
            break;
        }
    }
    return total;
}

void RefreshAllNoteChannelPitch(MidiEngineTaskContext* ctx) {
    for (int ch = 0; ch < MIDI_CHANNELS; ++ch) {
        if (ch == RhythmChannel::MIDI_RHYTHM_CHANNEL) {
            continue;
        }
        auto* nc = static_cast<NoteChannel*>((*ctx->channels)[ch]);
        const int16_t vib_cents = nc->ComputeVibCents();
        nc->ApplyPitchToVoices(vib_cents);
    }
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
    case MidiControlType::DebugStats:
        {
            const MidiIpcStats midiIpcStats = MidiIpcGetStats();
            std::printf("\nVoice allocation failure: %d\n",
                        VoiceAllocator::GetInstance().GetFailedCount());
            std::printf("midi_ipc queue drops: effect=%lu note=%lu control=%lu reset=%lu\n",
                        static_cast<unsigned long>(midiIpcStats.midi_event_queue_drop_count),
                        static_cast<unsigned long>(midiIpcStats.midi_note_queue_drop_count),
                        static_cast<unsigned long>(midiIpcStats.midi_control_queue_drop_count),
                        static_cast<unsigned long>(midiIpcStats.midi_reset_queue_drop_count));
            std::printf("midi_ipc note_off protect: evict=%lu fallback=%lu\n",
                        static_cast<unsigned long>(midiIpcStats.midi_note_on_evict_count),
                        static_cast<unsigned long>(midiIpcStats.midi_note_off_fallback_count));
        }
        for (auto* ch : *ctx->channels) {
            ch->stats();
        }
        break;
    case MidiControlType::DebugVibratoOverride:
        if (ctl.channel <= static_cast<uint8_t>(VibOverride::Auto)) {
            g_vib_override = static_cast<VibOverride>(ctl.channel);
        }
        RefreshAllNoteChannelPitch(ctx);
        break;
    }
}

void ExecPendingNoteOff(uint8_t channel, uint8_t key, void* ctx) {
    auto* engine_ctx = static_cast<MidiEngineTaskContext*>(ctx);
    MidiEvent evt{};
    evt.type         = MidiEventType::NoteOff;
    evt.channel      = channel;
    evt.data1        = key;
    evt.data2        = 0;
    evt.size         = 3;
    evt.timestamp_us = 0;
    ExecMidiEvent(engine_ctx, evt, static_cast<uint32_t>(time_us_64()));
}

void DrainPendingNoteOffsIfQueueEmpty(MidiEngineTaskContext* ctx) {
    if (uxQueueMessagesWaiting(gMidiNoteQueue) > 0) {
        return;
    }
    (void)MidiIpcDrainPendingNoteOffs(ExecPendingNoteOff, ctx);
}

void HandleControlAndReset(MidiEngineTaskContext* ctx) {
    if (gPendingReset.load(std::memory_order_acquire)) {
        gPendingReset.store(false, std::memory_order_relaxed);
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
    return uxQueueMessagesWaiting(gMidiNoteQueue) > 0 ||
           uxQueueMessagesWaiting(gMidiEventQueue) > 0;
}

void MaybeReconcileIdleFmKeys(MidiEngineTaskContext* ctx, uint32_t& next_reconcile_us) {
    if (HasPendingMidiWork()) {
        return;
    }
    const uint16_t enable_bits = ctx->processor->GetChannelEnableBits();
    for (int ch = 0; ch < MIDI_CHANNELS; ++ch) {
        if (ch == RhythmChannel::MIDI_RHYTHM_CHANNEL) {
            continue;
        }
        if ((enable_bits & (1u << ch)) == 0) {
            continue;
        }
        if (static_cast<NoteChannel*>((*ctx->channels)[ch])->IsActive()) {
            return;
        }
    }
    const uint32_t now_us = static_cast<uint32_t>(time_us_64());
    if (!TimeReached(next_reconcile_us, now_us)) {
        return;
    }
    VoiceAllocator::GetInstance().ReconcileIdleFmKeys(*ctx->channels);
    next_reconcile_us = now_us + kIdleFmReconcilePeriodUs;
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
    uint32_t   next_reconcile_us  = static_cast<uint32_t>(time_us_64()) + kIdleFmReconcilePeriodUs;

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

        (void)DrainAllPendingNotes(ctx, now_us);
        (void)DrainEffectQueue(ctx, MIDI_EFFECT_BATCH_MAX, now_us);
        HandleControlAndReset(ctx);
        DrainPendingNoteOffsIfQueueEmpty(ctx);
        (void)ServiceVibratoIfDue(ctx, next_vibrato_us);
        MaybeReconcileIdleFmKeys(ctx, next_reconcile_us);

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

        if (xQueueReceive(gMidiNoteQueue, &wait_evt, MsToTicksCeil(wait_ms)) == pdTRUE) {
            const uint32_t event_now_us =
                wait_evt.timestamp_us != 0 ? wait_evt.timestamp_us : now_us;
            ExecMidiEvent(ctx, wait_evt, event_now_us);
            (void)DrainAllPendingNotes(ctx, static_cast<uint32_t>(time_us_64()));
        }

        ApplyPanelNoteOnBitmap(static_cast<uint32_t>(time_us_64()));
    }
}
