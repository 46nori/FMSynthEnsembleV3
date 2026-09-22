//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

#include "PlaybackSequence.h"

/**
 * @brief SDカード上のSMF（Standard MIDI File）を読み込み、gMidiQueue経由で再生する
 * @details doc/design_smf_player.md、doc/design_smf_playback.md 参照。Core0固定。
 */
void SmfPlayerTask(void* param);

namespace SmfPlayer {

/** @brief 再生状態 */
enum class State : uint8_t { Idle, Playing, Paused };

/** @brief 再生範囲 */
enum class Scope : uint8_t {
    All,       ///< SDカード全体（Lsの連番）
    Playlist,  ///< playlistフォルダ内（ファイル名昇順）
    Single,    ///< 組み込みフィクスチャ1件、または範囲外の1ファイル
};

/** @brief 再生状態のスナップショット（GetStatus()の戻り値） */
struct Status {
    State state = State::Idle;
    Scope scope = Scope::All;          ///< stateがIdleのときは無効
    uint16_t position = 0;             ///< 現在の曲の範囲内の位置（1始まり）。Idleのときは0
    uint16_t count = 0;                ///< 範囲内の曲数。Idleのときは0
    RepeatMode repeat = RepeatMode::Off;
    PlaybackMode playback_mode = PlaybackMode::Single;
    bool shuffle = false;
};

/**
 * @brief 他タスクからのコマンド送信API（fire-and-forget）
 * @details 応答は待たない。実行結果はSmfPlayerTaskが標準出力へ直接表示する。
 *          コマンドは固定長のキューに積まれる。SmfPlayerTaskの起動前に呼ぶと無視される。
 */
void RequestPlay(uint16_t index);          // Lsが表示した連番（1始まり）。範囲はAll（フィクスチャならSingle）
void RequestPlayPlaylist(uint16_t position);  // playlistフォルダ内の位置（1始まり）。範囲はPlaylist
void RequestStop();
void RequestPause();
void RequestResume();
void RequestNext();
void RequestPrev();
void RequestSetRepeat(RepeatMode mode);
void RequestSetShuffle(bool on);
void RequestSetPlaybackMode(PlaybackMode mode);
void RequestLs();
void RequestMount();  // SDカード抜き挿し後の手動復帰用

/**
 * @brief 再生状態のスナップショットを取得する
 * @details どのタスクからも呼べる。SmfPlayerTaskが状態を変えるたびに更新される。
 */
Status GetStatus();

}  // namespace SmfPlayer
