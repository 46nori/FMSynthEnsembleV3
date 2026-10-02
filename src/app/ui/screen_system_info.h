//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

class MenuScreen;
struct InfoScreenTaskContext;

namespace AppUi {

/**
 * @brief System Info画面を構築する
 * @param [in] ctx タスク起動時に確定しているコンテキスト（非所有、プログラム終了まで有効なこと）
 * @details Dock毎のFMモジュール種別とMIDIパネル接続有無は起動時固定で、ここで文字列にする。
 *          Voice/CSM数の行だけSyncSystemInfo()が更新する。
 * @return 構築した画面を指す静的なポインタ変数への参照（ITEM_SUBMENU()へそのまま渡せる）
 */
MenuScreen*& BuildSystemInfoScreen(const InfoScreenTaskContext& ctx);

/**
 * @brief Voice/CSM数の表示を最新の値に更新する
 * @details 毎周期呼ぶ。I2C書き込み量を抑えるため、更新はINFO_SCREEN_SYSINFO_REFRESH_MS
 *          周期に間引き、System Info画面の表示中だけ再描画する。
 */
void SyncSystemInfo();

}  // namespace AppUi
