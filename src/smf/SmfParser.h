//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

#include "SmfByteSource.h"
#include "SmfMessage.h"

/**
 * @brief SMF（Standard MIDI File）のチャンク・VLQ・ランニングステータス・
 *        メタイベントを解釈し、Format 1の複数トラックをマージ再生するパーサー
 * @details pico-sdk/FreeRTOS非依存。SDカード等の
 *          実I/OはSmfByteSourceの実装側に閉じ込め、本クラスはバイト列の意味解釈のみ行う。
 *
 * 使い方:
 *   1. ScanChunks() で MThd と各 MTrk のチャンク境界だけを順次読みする（シーク不要）
 *   2. 呼び出し側が各トラックの開始位置へ位置決め済みの SmfByteSource を用意する
 *   3. Begin() で再生を開始する
 *   4. NextEvent() を繰り返し呼び、返された delta_ticks ぶん待ってからイベントを処理する
 */
class SmfParser {
public:
    /** @brief 同時にマージ可能な最大トラック数（Begin()に渡せる配列の上限） */
    static constexpr uint8_t kMaxTracks = 32;

    /**
     * @brief MThdと各MTrkのチャンク境界だけを読む
     * @details トラック本体は読まない、シーク不要の1回の順次スキャン。
     *          トラック用FILは1つも開かない段階なので、失敗時の後始末は不要。
     *          最後の正常なチャンクの直後に、チャンクヘッダが不完全（8バイトに満たない）、
     *          または宣言されたチャンク長が残りバイト数を超えるなど、チャンクとして辻褄が
     *          合わないデータが続く場合、1つ以上のトラックが見つかっていればそこでスキャンを
     *          打ち切り、末尾の異常データを無視して正常終了として扱う
     *          （out_trailing_garbageにtrueを書き込む）。トラックが1つも見つからないまま
     *          異常に到達した場合はFormatErrorを返す。
     * @param[in]  header_source ファイル先頭から読む一時的なSmfByteSource
     * @param[out] out_tracks 各トラックの開始/終端オフセットを書き込む配列
     * @param[in]  max_tracks out_tracksの要素数（kMaxSmfTracks等、呼び出し側が決める上限）
     * @param[out] out_track_count 見つかったトラック数
     * @param[out] out_trailing_garbage 末尾の異常データを無視した場合にtrueを書き込む（nullptr可）
     * @return スキャン結果
     */
    SmfScanResult ScanChunks(SmfByteSource& header_source, SmfTrackInfo* out_tracks,
                              uint8_t max_tracks, uint8_t& out_track_count,
                              bool* out_trailing_garbage = nullptr);

    /**
     * @brief ScanChunks()の結果をもとに再生を開始する
     * @param[in] track_sources 各トラックの開始位置へ位置決め済みのSmfByteSource配列
     *            （呼び出し側が寿命を保証すること。要素数はkMaxTracks以下）
     * @param[in] track_count track_sourcesの要素数
     * @return 各トラックの先頭イベントを1件も読めなければfalse
     */
    bool Begin(SmfByteSource* const* track_sources, uint8_t track_count);

    /**
     * @brief 次のイベントを1件取得する（トラックマージ込み、シークしない）
     * @details 全トラックの中で次の絶対tickが最も早いものを選ぶ。同tickの場合は
     *          TempoChangeを優先する。
     * @param[out] out 取得したイベント
     * @return 常にtrue（全トラック終端ならkind==EndOfFileを返す）
     */
    bool NextEvent(SmfEvent& out);

    /** @brief MThdのdivision（四分音符あたりのtick数） */
    uint16_t TicksPerQuarterNote() const { return ticks_per_quarter_note_; }

private:
    static constexpr uint32_t kEndOfTrackTick = 0xffffffffu;  // 「これ以上イベントなし」の番兵
    static constexpr uint8_t  kMaxPendingEventBytes = 32;     // 通常のチャンネルメッセージ+短いSysEx用

    struct TrackCursor {
        SmfByteSource* source = nullptr;
        uint32_t       abs_tick = 0;             // このトラックの読み取り位置の絶対tick
        uint32_t       next_event_abs_tick = kEndOfTrackTick;
        uint8_t        running_status = 0;
        SmfEventKind   pending_kind = SmfEventKind::EndOfTrack;
        uint32_t       pending_tempo = 0;
        uint8_t        pending_bytes[kMaxPendingEventBytes];
        uint8_t        pending_length = 0;
    };

    // このトラックの次の「報告対象イベント」を1件デコードし、
    // track.next_event_abs_tick / pending_* を更新する。
    // メタイベントのうちTempoChange/EndOfTrack以外は読み飛ばし、
    // 報告対象が見つかるかトラックが尽きるまで内部でループする。
    void DecodeNextPendingEvent(TrackCursor& track);

    // status（ランニングステータス解決済み）とdata0からチャンネルメッセージ本体を
    // pending_bytes/pending_lengthへ書き込む。2バイト目データの読み取りに失敗すればfalse。
    bool DecodeChannelMessageBody(TrackCursor& track, uint8_t status, uint8_t data0);

    // VLQ（可変長数値）を読む。読めなければfalseを返す。
    static bool ReadVlq(SmfByteSource& source, uint32_t& out_value);

    uint16_t     ticks_per_quarter_note_ = 480;
    uint32_t     last_returned_abs_tick_ = 0;
    TrackCursor  tracks_[kMaxTracks];
    uint8_t      track_count_ = 0;

    // NextEvent()が返すout.bytesの実体。勝者トラックのpending_bytesは
    // NextEvent()内でその場でDecodeNextPendingEvent()により次イベント用に
    // 上書きされるため、呼び出し側に返す前にここへコピーする。
    uint8_t      last_event_bytes_[kMaxPendingEventBytes];
};
