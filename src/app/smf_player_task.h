//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

/**
 * @brief SDカード上のSMF（Standard MIDI File）を読み込み、gMidiQueue経由で再生する
 * @details doc/design_smf_player.md 参照。Core0固定。
 */
void SmfPlayerTask(void* param);

namespace SmfPlayer {

/**
 * @brief Debuggerからのコマンド送信API（fire-and-forget）
 * @details 応答は待たない。実行結果はSmfPlayerTaskが標準出力へ直接表示する。
 *          SmfPlayerTaskの起動前に呼ぶと無視される。
 */
void RequestPlay(uint16_t index);   // Lsが表示した連番（1始まり）を指定する
void RequestStop();
void RequestPause();
void RequestResume();
void RequestLs();
void RequestMount();  // SDカード抜き挿し後の手動復帰用

}  // namespace SmfPlayer
