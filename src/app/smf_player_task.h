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

/**
 * @brief テンポ倍率（%）の範囲
 * @details 曲を開始するたびに、再生中の曲の倍率は既定倍率（SetDefaultTempoScale）で初期化する。
 *          kTempoScaleDefaultPercentは既定倍率の電源投入時の値。
 */
constexpr uint16_t kTempoScaleMinPercent = 50;
constexpr uint16_t kTempoScaleMaxPercent = 200;
constexpr uint16_t kTempoScaleDefaultPercent = 100;

/** @brief 再生状態のスナップショット（GetStatus()の戻り値） */
struct Status {
    State state = State::Idle;
    Scope scope = Scope::All;          ///< stateがIdleのときは無効
    uint16_t position = 0;             ///< 現在の曲の範囲内の位置（1始まり）。Idleのときは0
    uint16_t count = 0;                ///< 範囲内の曲数。Idleのときは0
    RepeatMode repeat = RepeatMode::Off;
    PlaybackMode playback_mode = PlaybackMode::Single;
    bool shuffle = false;
    uint32_t tempo_us_per_qn = 0;      ///< 曲の現在のテンポ（倍率適用前）。Idleのときは0
    uint16_t tempo_scale_percent = kTempoScaleDefaultPercent;  ///< 再生中の曲のテンポ倍率（%）
    uint16_t default_tempo_scale_percent = kTempoScaleDefaultPercent;  ///< 曲開始時に読み込む倍率（%）
    uint16_t track_serial = 0;         ///< 曲を開始するたびに進む通し番号（倍率の読み込み検知用）
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
void RequestSetTempoScale(uint16_t percent);  // 再生中の曲のみに効く。範囲外は無視する
void RequestSetDefaultTempoScale(uint16_t percent);  // 次に開始する曲から効く。範囲外は無視する
void RequestLs();
void RequestMount();  // SDカード抜き挿し後の手動復帰用

/**
 * @brief 再生状態のスナップショットを取得する
 * @details どのタスクからも呼べる。SmfPlayerTaskが状態を変えるたびに更新される。
 */
Status GetStatus();

}  // namespace SmfPlayer
