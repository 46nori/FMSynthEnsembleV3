//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include "config.h"

class MidiChannel;

/**
 *   Debugger macros
 */
#if ENABLE_DEBUG_PRINT == 1
#define DPRINTF(level, format, ...)             \
    do {                                        \
        if (level <= Debugger::gDEBUG_LEVEL) {  \
            std::printf(format, ##__VA_ARGS__); \
        }                                       \
    } while (0)
#else
#define DPRINTF(level, format, ...)
#endif

namespace Debugger {

// Shared debug state referenced from multiple modules.
extern volatile bool gMidiMode;
extern volatile bool gMidiPanelMode;
extern volatile uint8_t gDEBUG_LEVEL;

/**
 * @brief getchar() with FreeRTOS-friendly blocking.
 * @return 取得した文字。エラー時は EOF (-1)。
 */ 
int getchar(void);

/**
 * @brief fgets() with FreeRTOS-friendly blocking.
 * @param buf 文字列を格納するバッファへのポインタ
 * @param size bufのサイズ
 * @param stream 入力元。isatty() が真のときのみエコーバックする（stdin 等）
 * @return buf を返す。
 */ 
char *fgets(char *buf, int size, FILE *stream);

/**
 * @brief デバッガーコマンドを MIDI Control Event として MIDIエンジンタスクに送信する
 * @param id コマンドID
 * @param value コマンドに関連する値 (例: DumpChannel コマンドならチャンネル番号)
 */
enum class DebugCommandId : uint8_t {
    MidiReset,
    DumpChannel,
    DumpVoice,
    DumpProgram,
    Stats,
    VibratoOverride,
    TlTrim,
    RhythmMix,
};

void SendCommand(DebugCommandId id, uint8_t value);

/**
 * @brief SystemExclusive message を処理する
 * @param raw SysExバイト列へのポインタ
 * @param len バイト列の長さ
 * @details 呼び出し前に `MidiSysEx::Classify()` が VendorDebug と判定し、
 *          `F0 7D 46 4D <cmd> ... F7` ヘッダ一致と最小長 (len>=6) を
 *          確認済みであること（本関数側では再チェックしない）。
 */
void HandleSysEx(const uint8_t* raw, uint16_t len);

/**
 * @brief MIDI統計情報（Voiceアロケート失敗数・IPCキュードロップ数・チャンネル別統計）を出力する
 * @param channels 全 MIDI チャンネルの配列
 */
void PrintMidiStats(const std::array<MidiChannel*, MIDI_CHANNELS>& channels);

}  // namespace Debugger
