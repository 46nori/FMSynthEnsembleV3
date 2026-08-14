//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "midi_ipc.h"
#include <cstdio>
#include "config.h"

// MIDIメッセージキューの定義
namespace {

// 旧 Note/Event キューの合計容量を維持し、単一 FIFO 化以外の条件差を抑える。
constexpr UBaseType_t kMidiQueueLength        = 192;
constexpr UBaseType_t kMidiControlQueueLength = 8;

}  // namespace

QueueHandle_t gMidiQueue        = nullptr;
QueueHandle_t gMidiControlQueue = nullptr;

namespace {

volatile uint32_t gMidiQueueDropCount = 0;
volatile uint32_t gMidiControlQueueDropCount = 0;
volatile uint32_t gMidiResetQueueDropCount = 0;
#if ENABLE_MIDI_TIMING_STATS
uint16_t gMidiQueueHighWaterMark = 0;
uint32_t gMidiNoteQueueDelayMaxUs = 0;
uint32_t gMidiNoteOffQueueDelayMaxUs = 0;
uint32_t gMidiEffectQueueDelayMaxUs = 0;
uint32_t gMidiNoteExecutionMaxUs = 0;
uint32_t gMidiEffectExecutionMaxUs = 0;
uint32_t gMidiVibratoExecutionMaxUs = 0;
MidiTimingSample gDelayOutliers[kMidiTimingOutlierCount] = {};
MidiTimingSample gExecutionOutliers[kMidiTimingOutlierCount] = {};
#endif

void log_queue_failure_once_every_64(const char* label, volatile uint32_t& counter) {
    ++counter;
#if ENABLE_DEBUG_PRINT == 1
    if ((counter & 0x3fu) == 1u) {
        std::printf("midi_ipc queue full: %s drop=%lu\n", label, static_cast<unsigned long>(counter));
    }
#endif
}

#if ENABLE_MIDI_TIMING_STATS
uint32_t sample_score(const MidiTimingSample& sample, bool by_execution) {
    return by_execution ? sample.execution_us : sample.queue_delay_us;
}

void insert_outlier(MidiTimingSample* outliers, const MidiTimingSample& sample,
                    bool by_execution) {
    const uint32_t score = sample_score(sample, by_execution);
    if (score == 0) {
        return;
    }

    for (size_t i = 0; i < kMidiTimingOutlierCount; ++i) {
        if (score <= sample_score(outliers[i], by_execution)) {
            continue;
        }
        for (size_t j = kMidiTimingOutlierCount - 1; j > i; --j) {
            outliers[j] = outliers[j - 1];
        }
        outliers[i] = sample;
        return;
    }
}
#endif

}  // namespace

bool MidiIpcInitialize() {
    gMidiQueue        = xQueueCreate(kMidiQueueLength, sizeof(MidiEvent));
    gMidiControlQueue = xQueueCreate(kMidiControlQueueLength, sizeof(MidiControlEvent));
    return gMidiQueue != nullptr && gMidiControlQueue != nullptr;
}

bool MidiIpcSendMidiEvent(const MidiEvent& event) {
    if (gMidiQueue == nullptr) {
        log_queue_failure_once_every_64("midi_uninitialized", gMidiQueueDropCount);
        return false;
    }

    if (xQueueSendToBack(gMidiQueue, &event, 0) == pdTRUE) {
        return true;
    }

    log_queue_failure_once_every_64("midi", gMidiQueueDropCount);
    return false;
}

#if ENABLE_MIDI_TIMING_STATS
void MidiIpcRecordMidiEventTiming(const MidiEvent& event, UBaseType_t queue_depth,
                                  uint32_t dequeue_us, uint32_t completed_us) {
    const uint32_t queue_delay_us =
        event.timestamp_us != 0 ? dequeue_us - event.timestamp_us : 0;
    const uint32_t execution_us = completed_us - dequeue_us;
    const uint16_t depth = queue_depth > UINT16_MAX
                               ? UINT16_MAX
                               : static_cast<uint16_t>(queue_depth);

    if (depth > gMidiQueueHighWaterMark) {
        gMidiQueueHighWaterMark = depth;
    }

    uint32_t& delay_max = MidiEventIsNote(event)
                              ? gMidiNoteQueueDelayMaxUs
                              : gMidiEffectQueueDelayMaxUs;
    uint32_t& execution_max = MidiEventIsNote(event)
                                  ? gMidiNoteExecutionMaxUs
                                  : gMidiEffectExecutionMaxUs;
    if (queue_delay_us > delay_max) {
        delay_max = queue_delay_us;
    }
    if (MidiEventIsNoteOff(event) && queue_delay_us > gMidiNoteOffQueueDelayMaxUs) {
        gMidiNoteOffQueueDelayMaxUs = queue_delay_us;
    }
    if (execution_us > execution_max) {
        execution_max = execution_us;
    }

    const MidiTimingSample sample{
        queue_delay_us,
        execution_us,
        depth,
        event.type,
        event.channel,
        event.data1,
        event.data2,
    };
    insert_outlier(gDelayOutliers, sample, false);
    insert_outlier(gExecutionOutliers, sample, true);
}

void MidiIpcRecordVibratoTiming(uint32_t execution_us) {
    if (execution_us > gMidiVibratoExecutionMaxUs) {
        gMidiVibratoExecutionMaxUs = execution_us;
    }
}
#endif

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

MidiIpcStats MidiIpcGetStats() {
    MidiIpcStats stats{
        static_cast<uint32_t>(gMidiQueueDropCount),
        static_cast<uint32_t>(gMidiControlQueueDropCount),
        static_cast<uint32_t>(gMidiResetQueueDropCount),
#if ENABLE_MIDI_TIMING_STATS
        gMidiQueueHighWaterMark,
        gMidiNoteQueueDelayMaxUs,
        gMidiNoteOffQueueDelayMaxUs,
        gMidiEffectQueueDelayMaxUs,
        gMidiNoteExecutionMaxUs,
        gMidiEffectExecutionMaxUs,
        gMidiVibratoExecutionMaxUs,
        {},
        {},
#endif
    };
#if ENABLE_MIDI_TIMING_STATS
    for (size_t i = 0; i < kMidiTimingOutlierCount; ++i) {
        stats.delay_outliers[i] = gDelayOutliers[i];
        stats.execution_outliers[i] = gExecutionOutliers[i];
    }
#endif
    return stats;
}

// MidiEngineTask / MidiPanelTask 間の共有変数
volatile uint16_t gPanelChannelBitmap = 0xffff;
volatile uint16_t gLastNoteOnBitmap = 0;
volatile uint32_t gResetPulseSeq = 0;
std::atomic<bool> gPendingReset{false};
