//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

/**
 * @brief SmfParser::NextEvent() が返すイベントの種別
 */
enum class SmfEventKind : uint8_t {
    ChannelMessage,  // ランニングステータス解決済みの生MIDIチャンネルメッセージ
    SysEx,           // SMF内SysExイベント（バイト列をそのまま返す）
    TempoChange,      // Set Tempoメタイベント
    EndOfTrack,       // あるトラックの終端
    EndOfFile,        // 全トラックが終端に達した
    FormatError,      // チャンク破損・VLQ異常等、続行不能な解釈エラー
};

/**
 * @brief SmfParser::NextEvent() が返す1件分のイベント
 * @details delta_ticks は直前に NextEvent() が返したイベントからの経過tick
 *          （グローバルタイムライン基準。トラック自身の直前イベントからではない）。
 */
struct SmfEvent {
    SmfEventKind   kind = SmfEventKind::EndOfFile;
    uint32_t       delta_ticks = 0;
    uint32_t       tempo_us_per_qn = 0;  // TempoChangeのみ有効
    const uint8_t* bytes = nullptr;      // ChannelMessage/SysExのみ有効
    uint8_t        length = 0;
};

/**
 * @brief SmfParser::ScanChunks() が返す1トラック分のチャンク境界
 */
struct SmfTrackInfo {
    uint32_t start_offset = 0;  // MTrkのデータ開始オフセット（チャンクヘッダの直後）
    uint32_t end_offset = 0;
};

/**
 * @brief SmfParser::ScanChunks() の結果
 */
enum class SmfScanResult : uint8_t {
    Ok,
    FormatError,    // MThd不正・チャンク破損など
    TooManyTracks,  // MTrk数がmax_tracksを超えた
};

/** @brief 最初のSet Tempoイベントが現れるまでの既定テンポ（120BPM） */
static constexpr uint32_t kSmfDefaultTempoUsPerQuarterNote = 500000;
