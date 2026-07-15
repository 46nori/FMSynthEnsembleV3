//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "debugger_task.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "debugger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "midi_ipc.h"
#include "RhythmChannel.h"
#include "NoteChannel.h"
#include "OpnBase.h"
#include "VoiceAllocator.h"
#include "volume_controller.h"

/*********************************************************
 * Shared variables
 *********************************************************/
volatile uint8_t Debugger::gDEBUG_LEVEL = 0;
volatile bool Debugger::gMidiMode       = true;
volatile bool Debugger::gMidiPanelMode   = true;

/*********************************************************
 * Debugger main loop and command handling
 *********************************************************/
namespace {

#define DLIMITER   " "
#define MAX_TOKENS 4
#define T_CMD      0
#define T_PARAM1   1
#define T_PARAM2   2
#define T_PARAM3   3
typedef struct token_list {
    int n;                    // number of tokens
    char* token[MAX_TOKENS];  // pointer to token
} token_list;

#define NO_ERROR       (0)
#define ERR_COMMAND    (-1)
#define ERR_PARAM_VAL  (-2)
#define ERR_PARAM_MISS (-3)

token_list* tokenizer(char* str, const char* delim, token_list* t);
int exec_command(token_list* t);
void debugger_main();
void set_debugger_context(DebuggerTaskContext* ctx);

int c_help(token_list* t);
int c_debug_level(token_list* t);
int c_midi_mode(token_list* t);
int c_midi_panel_mode(token_list* t);
int c_midi_channel_status(token_list* t);
int c_midi_reset(token_list* t);
int c_midi_dump_channel(token_list* t);
int c_midi_dump_voice(token_list* t);
int c_midi_stats(token_list* t);
int c_volume_table(token_list* t);
int c_volume_raw(token_list* t);
int c_volume_zc(token_list* t);
int c_trim(token_list* t);
int c_rmix(token_list* t);
int c_vibrato(token_list* t);
int c_midi_program(token_list* t);

DebuggerTaskContext* gDebuggerCtx = nullptr;

const struct {
    const char* name;
    int (*func)(token_list*);
} cmd_table[] = {
    {    "dl",       c_debug_level},
    {    "mm",         c_midi_mode},
    {    "mp",   c_midi_panel_mode},
    {    "cs", c_midi_channel_status},
    {"mreset",        c_midi_reset},
    {    "dc", c_midi_dump_channel},
    {    "dv",   c_midi_dump_voice},
    { "stats",        c_midi_stats},
    {   "vol",      c_volume_table},
    {  "vraw",       c_volume_raw},
    { "volzc",        c_volume_zc},
    {  "trim",             c_trim},
    {  "rmix",             c_rmix},
    {   "vib",           c_vibrato},
    {    "pg",       c_midi_program},
    {     "h",              c_help},
    {      "",                NULL}
};

void debugger_main() {
    token_list tokens;
    char cmd_line[32];

    while (1) {
        putchar('>');
        while (Debugger::fgets(cmd_line, sizeof(cmd_line) - 1, stdin) == NULL);
        cmd_line[strcspn(cmd_line, "\r\n")] = '\0';
        tokenizer(cmd_line, DLIMITER, &tokens);
        switch (exec_command(&tokens)) {
        case NO_ERROR:
            break;
        case ERR_COMMAND:
            puts("not found.");
            break;
        case ERR_PARAM_VAL:
            puts("param error.");
            break;
        case ERR_PARAM_MISS:
            puts("missing param.");
            break;
        default:
            puts("error!");
            break;
        }
    }
}

void set_debugger_context(DebuggerTaskContext* ctx) {
    gDebuggerCtx = ctx;
}

// Tokenizer
token_list* tokenizer(char* str, const char* delim, token_list* t) {
    char* token;
    t->n  = 0;
    token = strtok(str, delim);
    while (token != NULL && t->n < MAX_TOKENS) {
        t->token[t->n++] = token;
        token            = strtok(NULL, delim);
    }
    return t;
}

// Get token value as an unsigned int
int get_uint(token_list* t, unsigned int idx, unsigned int* val) {
    if (idx >= t->n) {
        return ERR_PARAM_MISS;
    }

    unsigned int tmp;
    const char* str = t->token[idx];
    if (*str == '$') {
        // Hex
        if (sscanf(str + 1, "%x", &tmp) == 1) {
            *val = tmp;
            return NO_ERROR;
        }
    } else if (sscanf(str, "%u", &tmp) == 1) {
        // Decimal
        *val = tmp;
        return NO_ERROR;
    }
    return ERR_PARAM_VAL;
}

// Lookup command
int exec_command(token_list* t) {
    if (t->n == 0) {
        return NO_ERROR;  // do nothing
    }
    for (int i = 0; cmd_table[i].func != NULL; i++) {
        if (!strcmp(cmd_table[i].name, t->token[T_CMD])) {
            return cmd_table[i].func(t);
        }
    }
    return ERR_COMMAND;  // command not found
}

/*********************************************************
 * Help
 *********************************************************/
int c_help(token_list* t) {
    // Allocate literal in Flash ROM.
    static constexpr char help_str[] =
        "   <> : mandatory\n"
        "   [] : optional\n"
        "h         : Help\n"
        "dl [0-5]  : Set debug print level\n"
        "dc [0-15] : Dump MIDI Channel parameters\n"
        "dv        : Dump MIDI Voice parameters\n"
        "mm [0-1]  : MIDI Mode 0:Ignore MIDI, 1:Process MIDI\n"
        "mp [0-1]  : MIDI Panel 0:Disable scan/Tick, 1:Enable (for A/B comparison)\n"
        "cs [st]   : MIDI Channel ON/OFFStatus\n"
        "stats     : Statistics\n"
        "mreset    : MIDI Reset\n"
        "vol       : Dump volume table\n"
        "vraw <c> <ch> <val> : Set raw NJU72343 value (c=0/1, ch=0-7 A-H)\n"
        "volzc <0|1> : Zero Cross Detection OFF/ON (both chips)\n"
        "trim [0-1]: FM TL trim OFF/ON\n"
        "rmix [0-31]: Rhythm level offset step (0.75dB/step, lowers rhythm)\n"
        "vib [0-2] : Vibrato override 0=OFF 1=ON 2=AUTO(MIDI)\n"
        "pg        : Show MIDI Program (PG) per channel\n"
        "";

    puts(help_str);
    return NO_ERROR;
}

/*********************************************************
 * Debug print level
 *********************************************************/
int c_debug_level(token_list* t) {
    // External memory test
    unsigned int level = 0;
    if (t->n > 1) {
        get_uint(t, T_PARAM1, &level);
        Debugger::gDEBUG_LEVEL = (uint8_t)level;
    }
    std::printf("Debug Level: %d\n", Debugger::gDEBUG_LEVEL);
    return NO_ERROR;
}

/*********************************************************
 * MIDI mode (enable/disable MIDI processing)
 *********************************************************/
int c_midi_mode(token_list* t) {
    unsigned int mode = 0;
    if (t->n > 1) {
        get_uint(t, T_PARAM1, &mode);
        Debugger::gMidiMode = (mode == 0) ? false : true;
    }
    std::printf("MIDI Mode: %s\n", Debugger::gMidiMode ? "ON" : "OFF");
    return NO_ERROR;
}

/*********************************************************
 * MIDI Panel mode (enable/disable panel scan/Tick for A/B comparison)
 *********************************************************/
int c_midi_panel_mode(token_list* t) {
    unsigned int mode = 0;
    if (t->n > 1) {
        get_uint(t, T_PARAM1, &mode);
        Debugger::gMidiPanelMode = (mode == 0) ? false : true;
    }
    std::printf("MIDI Panel Mode: %s\n", Debugger::gMidiPanelMode ? "ON" : "OFF");
    return NO_ERROR;
}

/*********************************************************
 * MIDI channel status
 *********************************************************/
 int c_midi_channel_status(token_list* t) {
    unsigned int status = 0;
    if (t->n > 1) {
        get_uint(t, T_PARAM1, &status);
        gPanelChannelBitmap = status & 0xffff;
    }
    std::printf("MIDI Channel ON/OFF Status: $%04x\n", gPanelChannelBitmap);
    std::printf("CH  ");
    for (int i = 1; i <= 16; i++) {
        std::printf("%2d  ", i);
    }
    std::printf("\n   ");
    for (int i = 0; i < 16; i++) {
            std::printf("%s ", (gPanelChannelBitmap & (1 << i)) ? " ON" : "OFF");
    }
    std::printf("\n");
    return NO_ERROR;
 }

/*********************************************************
 * MIDI Reset
 *********************************************************/
int c_midi_reset(token_list* t) {
    Debugger::SendCommand(Debugger::DebugCommandId::MidiReset, 0);
    return NO_ERROR;
}

/*********************************************************
 * Dump MIDI Channel parameters
 *********************************************************/
int c_midi_dump_channel(token_list* t) {
    if (gDebuggerCtx == nullptr || gDebuggerCtx->channels == nullptr) {
        puts("debug context unavailable.");
        return NO_ERROR;
    }

    if (t->n == 1) {
        // Dump all channels.
        for (auto* ch : *gDebuggerCtx->channels) {
            ch->dump();
        }
    } else if (t->n > 1) {
        // Dump specified channel.
        unsigned int ch = 0;
        get_uint(t, T_PARAM1, &ch);
        if (ch < MIDI_CHANNELS) {
            (*gDebuggerCtx->channels)[ch]->dump();
        }
    }
    return NO_ERROR;
}

/*********************************************************
 * Dump MIDI Voice parameters
 *********************************************************/
int c_midi_dump_voice(token_list* t) {
    VoiceAllocator::GetInstance().dump();
    return NO_ERROR;
}

/*********************************************************
 * Dump Volume Table
 *********************************************************/
int c_volume_table(token_list* t) {
    (void)t;

    const auto& table = Platform::VolumeController::GetInstance().GetVolumeTable();
    static constexpr const char* chip_names[] = {"CHIP_ADR0", "CHIP_ADR1"};
    static constexpr char channel_names[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};

    std::printf("\n=== NJU72343 Volume Table ===\n");
    //std::printf("(shadow values: last values sent by VolumeController)\n");
    for (size_t chip = 0; chip < Platform::VolumeController::kChipCount; ++chip) {
        std::printf("%s:", chip_names[chip]);
        for (size_t ch = 0; ch < Platform::VolumeController::kChannelCount; ++ch) {
            const auto& value = table[chip][ch];
            std::printf(" %c=", channel_names[ch]);
            if (value.muted) {
                std::printf("Mute");
            } else {
                const int16_t abs_db_x2 = (value.db_x2 < 0) ? -value.db_x2 : value.db_x2;
                std::printf("%s%d.%d ",
                            (value.db_x2 < 0) ? "-" : "",
                            abs_db_x2 / 2,
                            (abs_db_x2 % 2) ? 5 : 0);
            }
        }
        std::printf(" (dB)\n");
    }
    return NO_ERROR;
}

namespace {

bool volume_chip_addr(unsigned int chip_idx, uint8_t* chip_addr) {
    switch (chip_idx) {
    case 0:
        *chip_addr = NJU72343::CHIP_ADR0;
        return true;
    case 1:
        *chip_addr = NJU72343::CHIP_ADR1;
        return true;
    default:
        return false;
    }
}

}  // namespace

/*********************************************************
 * Set raw NJU72343 channel value
 *********************************************************/
int c_volume_raw(token_list* t) {
    if (t->n < 4) {
        return ERR_PARAM_MISS;
    }

    unsigned int chip_idx = 0;
    unsigned int channel  = 0;
    unsigned int value    = 0;
    if (get_uint(t, T_PARAM1, &chip_idx) != NO_ERROR ||
        get_uint(t, T_PARAM2, &channel) != NO_ERROR ||
        get_uint(t, T_PARAM3, &value) != NO_ERROR) {
        return ERR_PARAM_VAL;
    }
    if (channel > 7 || value > 255) {
        return ERR_PARAM_VAL;
    }

    uint8_t chip_addr = 0;
    if (!volume_chip_addr(chip_idx, &chip_addr)) {
        return ERR_PARAM_VAL;
    }

    Platform::VolumeController::GetInstance().SetVolumeRaw(
        chip_addr, static_cast<uint8_t>(channel), static_cast<uint8_t>(value));
    std::printf("vraw: chip=%u ch=%u val=0x%02x\n", chip_idx, channel, value);
    return NO_ERROR;
}

/*********************************************************
 * Zero Cross Detection control
 *********************************************************/
int c_volume_zc(token_list* t) {
    if (t->n < 2) {
        return ERR_PARAM_MISS;
    }

    unsigned int enabled = 0;
    if (get_uint(t, T_PARAM1, &enabled) != NO_ERROR || enabled > 1) {
        return ERR_PARAM_VAL;
    }

    Platform::VolumeController::GetInstance().SetZeroCrossDetection(enabled != 0);
    std::printf("volzc: Zero Cross Detection %s\n", enabled ? "ON" : "OFF");
    return NO_ERROR;
}

/*********************************************************
 * FM TL Trim
 *********************************************************/
int c_trim(token_list* t) {
#if ENABLE_FM_TL_TRIM
    if (t->n > 1) {
        unsigned int mode = 0;
        const int err = get_uint(t, T_PARAM1, &mode);
        if (err != NO_ERROR) {
            return err;
        }
        if (mode > 1) {
            return ERR_PARAM_VAL;
        }
        OpnBase::SetTLTrimEnabled(mode != 0);
        VoiceAllocator::GetInstance().RefreshActiveFmVolume();
    }
    std::printf("FM TL Trim: %s\n", OpnBase::IsTLTrimEnabled() ? "ON" : "OFF");
#else
    if (t->n > 1) {
        return ERR_PARAM_VAL;
    }
    std::printf("FM TL Trim: unavailable (disabled at compile time)\n");
#endif
    return NO_ERROR;
}

/*********************************************************
 * Rhythm vs FM level balance
 *********************************************************/
int c_rmix(token_list* t) {
    if (t->n > 1) {
        unsigned int offset = 0;
        if (get_uint(t, T_PARAM1, &offset) != NO_ERROR || offset > 31) {
            return ERR_PARAM_VAL;
        }
        g_rhythm_level_offset = static_cast<int8_t>(offset);
        if (gDebuggerCtx != nullptr && gDebuggerCtx->channels != nullptr) {
            auto* rc = static_cast<RhythmChannel*>(
                (*gDebuggerCtx->channels)[RhythmChannel::MIDI_RHYTHM_CHANNEL]);
            rc->RefreshRhythmLevels();
        }
    }
    std::printf("Rhythm Level Offset: %d step(s) (~%.1f dB)\n",
                static_cast<int>(g_rhythm_level_offset),
                static_cast<double>(g_rhythm_level_offset) * 0.75);
    return NO_ERROR;
}

/*********************************************************
 * MIDI Program (PG) per channel
 *********************************************************/
int c_midi_program(token_list* t) {
    (void)t;

    if (gDebuggerCtx == nullptr || gDebuggerCtx->channels == nullptr) {
        puts("debug context unavailable.");
        return NO_ERROR;
    }

    std::printf("CH   ");
    for (int i = 1; i <= 16; i++) {
        std::printf("%5d ", i);
    }
    std::printf("\nBANK ");
    for (int i = 0; i < MIDI_CHANNELS; i++) {
        const uint32_t bk = (*gDebuggerCtx->channels)[i]->GetProgram();
        std::printf("%5u ", (bk >> 16) & 0xffffu);
    }
    std::printf("\nPG   ");
    for (int i = 0; i < MIDI_CHANNELS; i++) {
        const uint32_t bk = (*gDebuggerCtx->channels)[i]->GetProgram();
        std::printf("%5u ", bk & 0x7fu);
    }
    std::printf("\n");
    return NO_ERROR;
}

/*********************************************************
 * Vibrato override (A-B listening / comparison)
 *********************************************************/
static const char* VibOverrideLabel(VibOverride mode) {
    switch (mode) {
    case VibOverride::Off:
        return "OFF";
    case VibOverride::On:
        return "ON";
    case VibOverride::Auto:
        return "AUTO";
    }
    return "AUTO";
}

int c_vibrato(token_list* t) {
    if (t->n > 1) {
        unsigned int mode = 0;
        if (get_uint(t, T_PARAM1, &mode) != NO_ERROR || mode > 2) {
            return ERR_PARAM_VAL;
        }
        Debugger::SendCommand(Debugger::DebugCommandId::VibratoOverride,
                              static_cast<uint8_t>(mode));
        std::printf("Vibrato Override: %s\n",
                    VibOverrideLabel(static_cast<VibOverride>(mode)));
    } else {
        std::printf("Vibrato Override: %s\n", VibOverrideLabel(g_vib_override));
    }
    return NO_ERROR;
}

/*********************************************************
 * Statistics
 *********************************************************/
int c_midi_stats(token_list* t) {
    if (gDebuggerCtx == nullptr || gDebuggerCtx->channels == nullptr) {
        puts("debug context unavailable.");
        return NO_ERROR;
    }

    const MidiIpcStats midiIpcStats = MidiIpcGetStats();
    std::printf("\nVoice allocation failure: %d\n", VoiceAllocator::GetInstance().GetFailedCount());
    std::printf("midi_ipc queue drops: effect=%lu note=%lu control=%lu reset=%lu\n",
                static_cast<unsigned long>(midiIpcStats.midi_event_queue_drop_count),
                static_cast<unsigned long>(midiIpcStats.midi_note_queue_drop_count),
                static_cast<unsigned long>(midiIpcStats.midi_control_queue_drop_count),
                static_cast<unsigned long>(midiIpcStats.midi_reset_queue_drop_count));
    std::printf("midi_ipc note_off protect: reserve_drop=%lu fallback=%lu\n",
                static_cast<unsigned long>(midiIpcStats.midi_note_on_reserve_drop_count),
                static_cast<unsigned long>(midiIpcStats.midi_note_off_fallback_count));
    for (auto* ch : *gDebuggerCtx->channels) {
        ch->stats();
    }
    return NO_ERROR;
}

}  // namespace

/*********************************************************
 * FreeRTOS Task Entry
 *********************************************************/
void DebuggerTask(void* param) {
    set_debugger_context(static_cast<DebuggerTaskContext*>(param));
    debugger_main();    // never returns
    vTaskDelete(nullptr);
}
