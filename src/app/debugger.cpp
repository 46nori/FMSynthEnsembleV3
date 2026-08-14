//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "debugger.h"

#include <cstdint>
#include <cstdio>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"             // taskYIELD()
#include "pico/stdlib.h"      // getchar_timeout_us(), PICO_ERROR_TIMEOUT
#include "midi_ipc.h"
#include "MidiChannel.h"
#include "VoiceAllocator.h"

namespace {

bool ShouldEcho(FILE* stream) {
    return stream != nullptr && isatty(fileno(stream)) != 0;
}

#if ENABLE_MIDI_TIMING_STATS
const char* MidiEventTypeName(MidiEventType type) {
    switch (type) {
    case MidiEventType::NoteOff:          return "NoteOff";
    case MidiEventType::NoteOn:           return "NoteOn";
    case MidiEventType::PolyAftertouch:   return "PolyAT";
    case MidiEventType::ControlChange:    return "CC";
    case MidiEventType::ProgramChange:    return "PC";
    case MidiEventType::ChannelAftertouch:return "ChannelAT";
    case MidiEventType::PitchBend:        return "PB";
    case MidiEventType::ChannelMode:      return "ChannelMode";
    }
    return "?";
}

void PrintTimingOutliers(const char* label, const MidiTimingSample* samples,
                         bool by_execution) {
    std::printf("%s:\n", label);
    for (size_t i = 0; i < kMidiTimingOutlierCount; ++i) {
        const MidiTimingSample& sample = samples[i];
        const uint32_t score = by_execution ? sample.execution_us : sample.queue_delay_us;
        if (score == 0) {
            break;
        }
        std::printf("  #%u delay=%luus exec=%luus depth=%u %s ch=%u d1=%u d2=%u\n",
                    static_cast<unsigned>(i + 1),
                    static_cast<unsigned long>(sample.queue_delay_us),
                    static_cast<unsigned long>(sample.execution_us),
                    static_cast<unsigned>(sample.queue_depth),
                    MidiEventTypeName(sample.type),
                    static_cast<unsigned>(sample.channel + 1),
                    static_cast<unsigned>(sample.data1),
                    static_cast<unsigned>(sample.data2));
    }
}
#endif

}  // namespace

//
// getchar() with FreeRTOS-friendly blocking.
//
int Debugger::getchar(void) {
    int c;
    while ((c = getchar_timeout_us(0)) == PICO_ERROR_TIMEOUT) {
        vTaskDelay(pdMS_TO_TICKS(10));  // 他タスクに処理を譲る
    }
    return c;
}

//
// fgets() with FreeRTOS-friendly blocking.
//
char *Debugger::fgets(char *buf, int size, FILE *stream) {
    const bool echo = ShouldEcho(stream);
    int i = 0;
    while (i < size - 1) {
        int c = Debugger::getchar();
        if (c == EOF) {
            if (i == 0) {
                return nullptr;
            }
            break;
        }

        if (c == '\b' || c == 127) {
            if (i > 0) {
                --i;
                if (echo) {
                    std::putchar('\b');
                    std::putchar(' ');
                    std::putchar('\b');
                }
            }
            continue;
        }

        if (c == '\n' || c == '\r') {
            if (echo) {
                std::putchar('\r');
                std::putchar('\n');
            }
            break;
        }

        if (c >= 32 && c < 127) {
            buf[i++] = static_cast<char>(c);
            if (echo) {
                std::putchar(static_cast<char>(c));
            }
        }
    }
    buf[i] = '\0';
    return buf;
}

//
// Send debugger coommand to MIDI engine task as MIDI Control Event.
//
void Debugger::SendCommand(DebugCommandId id, uint8_t value) {
    MidiControlEvent ctl{};
    ctl.channel = value;
    ctl.timestamp_us = 0;

    switch (id) {
    case DebugCommandId::MidiReset:
        ctl.type = MidiControlType::Reset;
        break;
    case DebugCommandId::DumpChannel:
        ctl.type = MidiControlType::DebugDumpChannel;
        break;
    case DebugCommandId::DumpVoice:
        ctl.type = MidiControlType::DebugDumpVoice;
        break;
    case DebugCommandId::DumpProgram:
        ctl.type = MidiControlType::DebugDumpProgram;
        break;
    case DebugCommandId::Stats:
        ctl.type = MidiControlType::DebugStats;
        break;
    case DebugCommandId::VibratoOverride:
        ctl.type = MidiControlType::DebugVibratoOverride;
        break;
    case DebugCommandId::TlTrim:
        ctl.type = MidiControlType::DebugTlTrim;
        break;
    case DebugCommandId::RhythmMix:
        ctl.type = MidiControlType::DebugRhythmMix;
        break;
    }

    if (!MidiIpcSendMidiControl(ctl)) {
        std::printf("Control queue full\n");
    }
}

//
// Handle SysEx message received from USB MIDI and execute corresponding debug command.
//
namespace {
// Vendor unique SysEx command
constexpr uint8_t DEBUGGER_MIDI_RESET   = 0x01;
constexpr uint8_t DEBUGGER_DUMP_CHANNEL = 0x02;
constexpr uint8_t DEBUGGER_DUMP_VOICE   = 0x03;
constexpr uint8_t DEBUGGER_STATS        = 0x04;
}  // namespace

void Debugger::HandleSysEx(const uint8_t* raw, uint16_t len) {
    // Header match (F0 7D 46 4D ... F7) and len>=6 are guaranteed by the
    // caller via MidiSysEx::Classify(); not re-checked here.
    const uint8_t cmd = raw[4];
    switch (cmd) {
    case DEBUGGER_MIDI_RESET:
        SendCommand(DebugCommandId::MidiReset, 0);
        break;
    case DEBUGGER_DUMP_CHANNEL: {
        const uint8_t ch = (len >= 7) ? raw[5] : 0xff;
        SendCommand(DebugCommandId::DumpChannel, ch);
        break;
    }
    case DEBUGGER_DUMP_VOICE:
        SendCommand(DebugCommandId::DumpVoice, 0);
        break;
    case DEBUGGER_STATS:
        SendCommand(DebugCommandId::Stats, 0);
        break;
    default:
        break;
    }
}

void Debugger::PrintMidiStats(const std::array<MidiChannel*, MIDI_CHANNELS>& channels) {
    const MidiIpcStats midiIpcStats = MidiIpcGetStats();
    std::printf("\nVoice allocation failure: %d\n", VoiceAllocator::GetInstance().GetFailedCount());
    std::printf("midi_ipc queue drops: midi=%lu control=%lu reset=%lu\n",
                static_cast<unsigned long>(midiIpcStats.midi_queue_drop_count),
                static_cast<unsigned long>(midiIpcStats.midi_control_queue_drop_count),
                static_cast<unsigned long>(midiIpcStats.midi_reset_queue_drop_count));
#if ENABLE_MIDI_TIMING_STATS
    std::printf("midi_ipc queue high water: %u\n",
                static_cast<unsigned>(midiIpcStats.midi_queue_high_water_mark));
    std::printf("midi_ipc max delay: note=%luus note_off=%luus effect=%luus\n",
                static_cast<unsigned long>(midiIpcStats.midi_note_queue_delay_max_us),
                static_cast<unsigned long>(midiIpcStats.midi_note_off_queue_delay_max_us),
                static_cast<unsigned long>(midiIpcStats.midi_effect_queue_delay_max_us));
    std::printf("midi_ipc max execution: note=%luus effect=%luus vibrato=%luus\n",
                static_cast<unsigned long>(midiIpcStats.midi_note_execution_max_us),
                static_cast<unsigned long>(midiIpcStats.midi_effect_execution_max_us),
                static_cast<unsigned long>(midiIpcStats.midi_vibrato_execution_max_us));
    PrintTimingOutliers("midi_ipc delay outliers", midiIpcStats.delay_outliers, false);
    PrintTimingOutliers("midi_ipc execution outliers",
                        midiIpcStats.execution_outliers, true);
#endif
    for (auto* ch : channels) {
        ch->stats();
    }
}
