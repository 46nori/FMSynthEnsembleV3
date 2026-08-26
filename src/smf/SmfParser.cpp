//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "SmfParser.h"

namespace {

bool ReadBigEndianU16(SmfByteSource& source, uint16_t& out) {
    uint8_t hi, lo;
    if (!source.ReadByte(hi) || !source.ReadByte(lo)) {
        return false;
    }
    out = static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8) | lo);
    return true;
}

bool ReadBigEndianU32(SmfByteSource& source, uint32_t& out) {
    uint8_t b[4];
    for (uint8_t& byte : b) {
        if (!source.ReadByte(byte)) {
            return false;
        }
    }
    out = (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
          (static_cast<uint32_t>(b[2]) << 8) | b[3];
    return true;
}

bool SkipBytes(SmfByteSource& source, uint32_t count) {
    uint8_t dummy;
    for (uint32_t i = 0; i < count; ++i) {
        if (!source.ReadByte(dummy)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool SmfParser::ReadVlq(SmfByteSource& source, uint32_t& out_value) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t b;
        if (!source.ReadByte(b)) {
            return false;
        }
        value = (value << 7) | (b & 0x7f);
        if ((b & 0x80) == 0) {
            out_value = value;
            return true;
        }
    }
    return false;  // 4バイトを超えるVLQは不正
}

SmfScanResult SmfParser::ScanChunks(SmfByteSource& header_source, SmfTrackInfo* out_tracks,
                                     uint8_t max_tracks, uint8_t& out_track_count,
                                     bool* out_trailing_garbage) {
    out_track_count = 0;
    if (out_trailing_garbage != nullptr) {
        *out_trailing_garbage = false;
    }
    uint32_t pos = 0;

    uint8_t magic[4];
    for (uint8_t& byte : magic) {
        if (!header_source.ReadByte(byte)) {
            return SmfScanResult::FormatError;
        }
    }
    pos += 4;
    if (magic[0] != 'M' || magic[1] != 'T' || magic[2] != 'h' || magic[3] != 'd') {
        return SmfScanResult::FormatError;
    }

    uint32_t header_len = 0;
    if (!ReadBigEndianU32(header_source, header_len) || header_len < 6) {
        return SmfScanResult::FormatError;
    }
    pos += 4;

    uint16_t format = 0, ntrks = 0, division = 0;
    if (!ReadBigEndianU16(header_source, format) || !ReadBigEndianU16(header_source, ntrks) ||
        !ReadBigEndianU16(header_source, division)) {
        return SmfScanResult::FormatError;
    }
    pos += 6;
    (void)format;
    (void)ntrks;  // MTrkチャンクを実走査して数えるため、ヘッダ記載値は使わない

    if ((division & 0x8000) || division == 0) {
        return SmfScanResult::FormatError;  // SMPTE形式、または0は対象外（tick換算不能）
    }
    ticks_per_quarter_note_ = division;

    if (header_len > 6) {
        if (!SkipBytes(header_source, header_len - 6)) {
            return SmfScanResult::FormatError;
        }
        pos += header_len - 6;
    }

    for (;;) {
        uint8_t chunk_id[4];
        uint8_t got = 0;
        for (; got < 4; ++got) {
            if (!header_source.ReadByte(chunk_id[got])) {
                break;
            }
        }
        if (got != 4) {
            // ファイル終端(got==0)、または末尾に不完全なチャンクヘッダ(1-3バイト)が残るのみ。
            // 正常終了扱いとし、後者の場合のみ末尾異常ありとして報告する。
            if (got != 0 && out_trailing_garbage != nullptr) {
                *out_trailing_garbage = true;
            }
            break;
        }
        pos += 4;

        uint32_t chunk_len = 0;
        if (!ReadBigEndianU32(header_source, chunk_len)) {
            if (out_track_count == 0) {
                return SmfScanResult::FormatError;
            }
            if (out_trailing_garbage != nullptr) {
                *out_trailing_garbage = true;
            }
            break;
        }
        pos += 4;

        const bool is_mtrk = (chunk_id[0] == 'M' && chunk_id[1] == 'T' && chunk_id[2] == 'r' &&
                               chunk_id[3] == 'k');
        if (is_mtrk && out_track_count >= max_tracks) {
            return SmfScanResult::TooManyTracks;
        }

        if (!SkipBytes(header_source, chunk_len)) {
            // 宣言されたチャンク長が残りバイト数を超えている。チャンクとして辻褄が合わない末尾データ。
            if (out_track_count == 0) {
                return SmfScanResult::FormatError;
            }
            if (out_trailing_garbage != nullptr) {
                *out_trailing_garbage = true;
            }
            break;
        }

        if (is_mtrk) {
            out_tracks[out_track_count].start_offset = pos;
            out_tracks[out_track_count].end_offset = pos + chunk_len;
            ++out_track_count;
        }
        pos += chunk_len;
    }

    return SmfScanResult::Ok;
}

bool SmfParser::Begin(SmfByteSource* const* track_sources, uint8_t track_count) {
    if (track_count > kMaxTracks) {
        track_count = kMaxTracks;
    }
    track_count_ = track_count;
    last_returned_abs_tick_ = 0;

    bool any_event = false;
    for (uint8_t i = 0; i < track_count_; ++i) {
        tracks_[i] = TrackCursor{};
        tracks_[i].source = track_sources[i];
        DecodeNextPendingEvent(tracks_[i]);
        if (tracks_[i].next_event_abs_tick != kEndOfTrackTick) {
            any_event = true;
        }
    }
    return any_event;
}

bool SmfParser::DecodeChannelMessageBody(TrackCursor& track, uint8_t status, uint8_t data0) {
    track.pending_bytes[0] = status;
    track.pending_bytes[1] = data0;
    const uint8_t hi = status & 0xf0;
    if (hi == 0xc0 || hi == 0xd0) {
        // Program Change / Channel Aftertouch は2バイト
        track.pending_length = 2;
    } else {
        uint8_t data1;
        if (!track.source->ReadByte(data1)) {
            return false;
        }
        track.pending_bytes[2] = data1;
        track.pending_length = 3;
    }
    track.pending_kind = SmfEventKind::ChannelMessage;
    return true;
}

void SmfParser::DecodeNextPendingEvent(TrackCursor& track) {
    for (;;) {
        uint32_t delta = 0;
        if (!ReadVlq(*track.source, delta)) {
            track.next_event_abs_tick = kEndOfTrackTick;
            return;
        }
        track.abs_tick += delta;

        uint8_t status;
        if (!track.source->ReadByte(status)) {
            track.next_event_abs_tick = kEndOfTrackTick;
            return;
        }

        if (status < 0x80) {
            // ランニングステータス: このバイトは新しいステータスバイトではなく
            // 直前のチャンネルメッセージのデータ先頭バイト
            if (track.running_status == 0) {
                track.next_event_abs_tick = kEndOfTrackTick;  // ランニングステータス未確立
                return;
            }
            if (!DecodeChannelMessageBody(track, track.running_status, status)) {
                track.next_event_abs_tick = kEndOfTrackTick;
                return;
            }
            track.next_event_abs_tick = track.abs_tick;
            return;
        }

        if (status == 0xff) {
            // メタイベント: FF <type> <VLQ長> <データ...>
            uint8_t meta_type;
            if (!track.source->ReadByte(meta_type)) {
                track.next_event_abs_tick = kEndOfTrackTick;
                return;
            }
            uint32_t meta_len = 0;
            if (!ReadVlq(*track.source, meta_len)) {
                track.next_event_abs_tick = kEndOfTrackTick;
                return;
            }

            if (meta_type == 0x51 && meta_len == 3) {
                // Set Tempo
                uint8_t b0, b1, b2;
                if (!track.source->ReadByte(b0) || !track.source->ReadByte(b1) ||
                    !track.source->ReadByte(b2)) {
                    track.next_event_abs_tick = kEndOfTrackTick;
                    return;
                }
                track.pending_kind = SmfEventKind::TempoChange;
                track.pending_tempo = (static_cast<uint32_t>(b0) << 16) |
                                       (static_cast<uint32_t>(b1) << 8) | b2;
                track.next_event_abs_tick = track.abs_tick;
                return;
            }
            if (meta_type == 0x2f) {
                // End of Track
                track.pending_kind = SmfEventKind::EndOfTrack;
                track.next_event_abs_tick = track.abs_tick;
                return;
            }
            // それ以外のメタイベントは読み飛ばし、次のdelta+eventへ進む
            if (!SkipBytes(*track.source, meta_len)) {
                track.next_event_abs_tick = kEndOfTrackTick;
                return;
            }
            continue;
        }

        if (status == 0xf0 || status == 0xf7) {
            // SMF内SysEx: <F0/F7> <VLQ長> <長さぶんのデータ>
            uint32_t len = 0;
            if (!ReadVlq(*track.source, len)) {
                track.next_event_abs_tick = kEndOfTrackTick;
                return;
            }
            if (len + 1 > kMaxPendingEventBytes) {
                // バッファ上限を超える長いSysExは諦めて読み飛ばす
                if (!SkipBytes(*track.source, len)) {
                    track.next_event_abs_tick = kEndOfTrackTick;
                    return;
                }
                continue;
            }
            track.pending_bytes[0] = status;
            for (uint32_t i = 0; i < len; ++i) {
                if (!track.source->ReadByte(track.pending_bytes[1 + i])) {
                    track.next_event_abs_tick = kEndOfTrackTick;
                    return;
                }
            }
            track.pending_length = static_cast<uint8_t>(len + 1);
            track.pending_kind = SmfEventKind::SysEx;
            track.running_status = 0;  // SysExはランニングステータスをクリアする
            track.next_event_abs_tick = track.abs_tick;
            return;
        }

        if (status < 0xf0) {
            // チャンネルボイス/モードメッセージ（0x80-0xEF）
            track.running_status = status;
            uint8_t data0;
            if (!track.source->ReadByte(data0)) {
                track.next_event_abs_tick = kEndOfTrackTick;
                return;
            }
            if (!DecodeChannelMessageBody(track, status, data0)) {
                track.next_event_abs_tick = kEndOfTrackTick;
                return;
            }
            track.next_event_abs_tick = track.abs_tick;
            return;
        }

        // 0xF1-0xF6, 0xF8-0xFE 等、SMFトラック内に通常現れないステータス。
        // 続行不能として終端扱いにする
        track.next_event_abs_tick = kEndOfTrackTick;
        return;
    }
}

bool SmfParser::NextEvent(SmfEvent& out) {
    int winner = -1;
    for (uint8_t i = 0; i < track_count_; ++i) {
        if (tracks_[i].next_event_abs_tick == kEndOfTrackTick) {
            continue;
        }
        if (winner < 0) {
            winner = i;
            continue;
        }
        TrackCursor& w = tracks_[static_cast<uint8_t>(winner)];
        TrackCursor& c = tracks_[i];
        if (c.next_event_abs_tick < w.next_event_abs_tick) {
            winner = i;
        } else if (c.next_event_abs_tick == w.next_event_abs_tick &&
                   c.pending_kind == SmfEventKind::TempoChange &&
                   w.pending_kind != SmfEventKind::TempoChange) {
            // 同tickならTempoChangeを優先する
            winner = i;
        }
    }

    if (winner < 0) {
        out = SmfEvent{};
        out.kind = SmfEventKind::EndOfFile;
        return true;
    }

    TrackCursor& track = tracks_[static_cast<uint8_t>(winner)];
    out.kind = track.pending_kind;
    out.delta_ticks = track.next_event_abs_tick - last_returned_abs_tick_;
    last_returned_abs_tick_ = track.next_event_abs_tick;

    switch (out.kind) {
    case SmfEventKind::TempoChange:
        out.tempo_us_per_qn = track.pending_tempo;
        out.bytes = nullptr;
        out.length = 0;
        break;
    case SmfEventKind::ChannelMessage:
    case SmfEventKind::SysEx:
        // track.pending_bytes はこの直後の DecodeNextPendingEvent() で
        // 次イベント用に上書きされるため、呼び出し側へ返す前にコピーする。
        for (uint8_t i = 0; i < track.pending_length; ++i) {
            last_event_bytes_[i] = track.pending_bytes[i];
        }
        out.bytes = last_event_bytes_;
        out.length = track.pending_length;
        out.tempo_us_per_qn = 0;
        break;
    default:
        out.bytes = nullptr;
        out.length = 0;
        out.tempo_us_per_qn = 0;
        break;
    }

    if (out.kind == SmfEventKind::EndOfTrack || out.kind == SmfEventKind::FormatError) {
        track.next_event_abs_tick = kEndOfTrackTick;
    } else {
        DecodeNextPendingEvent(track);
    }
    return true;
}
