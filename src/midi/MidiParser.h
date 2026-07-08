//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

#include "MidiMessage.h"

/**
 * @brief MIDI バイト列をイベント構造体に変換するパーサー
 */
class MidiParser {
public:
    /**
     * @brief チャンネルボイス/モードメッセージを MidiEvent に変換する
     *
     * @param[in]  raw  MIDI バイト列の先頭ポインタ
     *                  - 非 null であること
     *                  - raw[0] は 0x80\u30fc0xEF のステータスバイトであること
     *                    (SysEx 0xF0 および System Common/Realtime 0xF1\u30fc0xFF は非対応)
     *                  - ランニングステータスは非対応。raw[0] は必ずステータスバイトであること
     * @param[in]  len  raw の有効バイト数 (メッセージ長以上であること)
     * @param[out] out  変換結果を格納する MidiEvent (成功時のみ更新される)
     * @return 変換成功なら true、入力不正・非対応メッセージなら false
     */
    static bool TryParseEvent(const uint8_t* raw, uint8_t len, MidiEvent& out);

    /**
     * @brief バイト列が完全な SysEx メッセージか判定する
     *
     * @param[in]  raw  MIDI バイト列の先頭ポインタ (raw[0]==0xF0 を期待)
     * @param[in]  len  raw の有効バイト数
     * @return raw[0]==0xF0 かつ raw[len-1]==0xF7 なら true
     */
    static bool IsSysEx(const uint8_t* raw, uint8_t len);

    /**
     * @brief ステータスバイトがリアルタイムメッセージか判定する
     *
     * @param[in]  status  MIDI ステータスバイト
     * @return 0xF8\u30fc0xFF (Timing Clock, Active Sensing, System Reset 等) なら true
     */
    static bool IsRealtimeStatus(uint8_t status);
};
