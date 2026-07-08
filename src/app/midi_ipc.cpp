//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "midi_ipc.h"
#include <atomic>
#include <cstdio>
#include "config.h"
#include "task.h"

// MIDIメッセージキューの定義
namespace {

constexpr UBaseType_t kMidiEventQueueLength   = 64;
constexpr UBaseType_t kMidiNoteQueueLength    = 128;
constexpr UBaseType_t kMidiControlQueueLength = 8;

constexpr unsigned kPendingNoteOffWords = 4;  // 128 keys per channel

}  // namespace

QueueHandle_t gMidiEventQueue   = nullptr;
QueueHandle_t gMidiNoteQueue    = nullptr;
QueueHandle_t gMidiControlQueue = nullptr;

namespace {

volatile uint32_t gMidiEventQueueDropCount = 0;
volatile uint32_t gMidiNoteQueueDropCount  = 0;
volatile uint32_t gMidiControlQueueDropCount = 0;
volatile uint32_t gMidiResetQueueDropCount = 0;
volatile uint32_t gMidiNoteOnEvictCount = 0;
volatile uint32_t gMidiNoteOffFallbackCount = 0;

std::atomic<uint32_t> gPendingNoteOffBitmap[16][kPendingNoteOffWords] = {};

void log_queue_failure_once_every_64(const char* label, volatile uint32_t& counter) {
    ++counter;
#if ENABLE_DEBUG_PRINT == 1
    if ((counter & 0x3fu) == 1u) {
        std::printf("midi_ipc queue full: %s drop=%lu\n", label, static_cast<unsigned long>(counter));
    }
#endif
}

void log_note_on_evict_once_every_64() {
    ++gMidiNoteOnEvictCount;
#if ENABLE_DEBUG_PRINT == 1
    if ((gMidiNoteOnEvictCount & 0x3fu) == 1u) {
        std::printf("midi_ipc note_on evicted for note_off evict=%lu\n",
                    static_cast<unsigned long>(gMidiNoteOnEvictCount));
    }
#endif
}

void log_note_off_fallback_once_every_64(uint8_t channel, uint8_t key) {
    ++gMidiNoteOffFallbackCount;
#if ENABLE_DEBUG_PRINT == 1
    if ((gMidiNoteOffFallbackCount & 0x3fu) == 1u) {
        std::printf("midi_ipc note_off fallback ch=%u key=%u count=%lu\n", channel, key,
                    static_cast<unsigned long>(gMidiNoteOffFallbackCount));
    }
#endif
}

bool RestoreNoteQueue(const MidiEvent* buf, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (xQueueSendToBack(gMidiNoteQueue, &buf[i], 0) != pdTRUE) {
            return false;
        }
    }
    return true;
}

/** @brief 満杯キュー末尾の NoteOn を 1 件追い出して空きを 1 スロット作る */
bool TryEvictNewestNoteOn() {
    MidiEvent buf[kMidiNoteQueueLength];
    size_t    n = 0;
    while (n < kMidiNoteQueueLength && xQueueReceive(gMidiNoteQueue, &buf[n], 0) == pdTRUE) {
        ++n;
    }
    if (n == 0) {
        return false;
    }

    size_t drop_idx = n;
    for (size_t i = n; i > 0; --i) {
        if (!MidiEventIsNoteOff(buf[i - 1])) {
            drop_idx = i - 1;
            break;
        }
    }
    if (drop_idx == n) {
        return RestoreNoteQueue(buf, n);
    }

    log_note_on_evict_once_every_64();

    for (size_t i = 0; i < n; ++i) {
        if (i == drop_idx) {
            continue;
        }
        if (xQueueSendToBack(gMidiNoteQueue, &buf[i], 0) != pdTRUE) {
            return false;
        }
    }
    return true;
}

void SetPendingNoteOff(uint8_t channel, uint8_t key) {
    if (channel >= 16) {
        return;
    }
    const unsigned word = key >> 5;
    const unsigned bit  = key & 31u;
    gPendingNoteOffBitmap[channel][word].fetch_or(1u << bit, std::memory_order_release);
    log_note_off_fallback_once_every_64(channel, key);
}

bool SendNoteOffToQueue(const MidiEvent& event) {
    if (xQueueSendToBack(gMidiNoteQueue, &event, 0) == pdTRUE) {
        return true;
    }
    if (!TryEvictNewestNoteOn()) {
        return false;
    }
    return xQueueSendToBack(gMidiNoteQueue, &event, 0) == pdTRUE;
}

}  // namespace

bool MidiIpcInitialize() {
    gMidiEventQueue   = xQueueCreate(kMidiEventQueueLength, sizeof(MidiEvent));
    gMidiNoteQueue    = xQueueCreate(kMidiNoteQueueLength, sizeof(MidiEvent));
    gMidiControlQueue = xQueueCreate(kMidiControlQueueLength, sizeof(MidiControlEvent));
    return gMidiEventQueue != nullptr && gMidiNoteQueue != nullptr &&
           gMidiControlQueue != nullptr;
}

bool MidiIpcSendMidiEvent(const MidiEvent& event) {
    if (gMidiEventQueue == nullptr) {
        log_queue_failure_once_every_64("midi_event_uninitialized", gMidiEventQueueDropCount);
        return false;
    }

    if (xQueueSendToBack(gMidiEventQueue, &event, 0) == pdTRUE) {
        return true;
    }

    log_queue_failure_once_every_64("midi_event", gMidiEventQueueDropCount);
    return false;
}

bool MidiIpcSendMidiNoteEvent(const MidiEvent& event) {
    if (gMidiNoteQueue == nullptr) {
        if (MidiEventIsNoteOff(event)) {
            SetPendingNoteOff(event.channel, event.data1);
            return true;
        }
        log_queue_failure_once_every_64("midi_note_uninitialized", gMidiNoteQueueDropCount);
        return false;
    }

    if (MidiEventIsNoteOff(event)) {
        if (SendNoteOffToQueue(event)) {
            return true;
        }
        SetPendingNoteOff(event.channel, event.data1);
        return true;
    }

    if (xQueueSendToBack(gMidiNoteQueue, &event, 0) == pdTRUE) {
        return true;
    }

    log_queue_failure_once_every_64("midi_note", gMidiNoteQueueDropCount);
    return false;
}

bool MidiIpcSendMidiControl(const MidiControlEvent& event) {
    if (gMidiControlQueue == nullptr) {
        if (event.type == MidiControlType::Reset) {
            gPendingReset.store(true, std::memory_order_release);
            log_queue_failure_once_every_64("midi_reset_uninitialized", gMidiResetQueueDropCount);
        } else {
            log_queue_failure_once_every_64("midi_control_uninitialized", gMidiControlQueueDropCount);
        }
        return false;
    }

    const BaseType_t result = (event.type == MidiControlType::Reset)
                                  ? xQueueSendToFront(gMidiControlQueue, &event, 0)
                                  : xQueueSendToBack(gMidiControlQueue, &event, 0);
    if (result == pdTRUE) {
        return true;
    }

    if (event.type == MidiControlType::Reset) {
        gPendingReset.store(true, std::memory_order_release);
        log_queue_failure_once_every_64("midi_reset", gMidiResetQueueDropCount);
    } else {
        log_queue_failure_once_every_64("midi_control", gMidiControlQueueDropCount);
    }
    return false;
}

size_t MidiIpcDrainPendingNoteOffs(MidiPendingNoteOffFn fn, void* ctx) {
    if (fn == nullptr) {
        return 0;
    }

    size_t count = 0;
    for (uint8_t ch = 0; ch < 16; ++ch) {
        for (unsigned w = 0; w < kPendingNoteOffWords; ++w) {
            uint32_t bits =
                gPendingNoteOffBitmap[ch][w].exchange(0, std::memory_order_acq_rel);
            while (bits != 0) {
                const unsigned bit = static_cast<unsigned>(__builtin_ctz(bits));
                bits &= bits - 1;
                fn(ch, static_cast<uint8_t>(w * 32 + bit), ctx);
                ++count;
            }
        }
    }
    return count;
}

MidiIpcStats MidiIpcGetStats() {
    return MidiIpcStats{
        static_cast<uint32_t>(gMidiEventQueueDropCount),
        static_cast<uint32_t>(gMidiNoteQueueDropCount),
        static_cast<uint32_t>(gMidiControlQueueDropCount),
        static_cast<uint32_t>(gMidiResetQueueDropCount),
        static_cast<uint32_t>(gMidiNoteOnEvictCount),
        static_cast<uint32_t>(gMidiNoteOffFallbackCount),
    };
}

// MidiEngineTask / MidiPanelTask 間の共有変数
volatile uint16_t gPanelChannelBitmap = 0xffff;
volatile uint16_t gLastNoteOnBitmap = 0;
std::atomic<bool> gPendingReset{false};
