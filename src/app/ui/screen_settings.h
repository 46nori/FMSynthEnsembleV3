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
 * @brief Settings画面を構築する
 * @param [in] ctx タスク起動時に確定しているコンテキスト（非所有、プログラム終了まで有効なこと）
 * @param [in] volumeScreen `Volume`行から入る画面を指す静的なポインタ変数
 * @param [in] systemInfoScreen `System Info`行から入る画面を指す静的なポインタ変数
 * @return 構築した画面を指す静的なポインタ変数への参照（ITEM_SUBMENU()へそのまま渡せる）
 * @details LED Mode / RhythmVolの行はこの画面が直接持つ。RhythmVolは、リズム音源を持つ
 *          モジュールが1台も無い構成では編集不可（N/A）にする。ItemSubMenuは遷移先の画面を
 *          ポインタ変数への参照で保持するため、遷移先は参照で受け取る。
 */
MenuScreen*& BuildSettingsScreen(const InfoScreenTaskContext& ctx, MenuScreen*& volumeScreen,
                                 MenuScreen*& systemInfoScreen);

/**
 * @brief RhythmVol行の表示値をg_rhythm_level_offsetへ同期する
 * @details 毎周期呼ぶ。Settings画面の表示中かつ編集中でない場合だけ同期する。
 *          FMレジスタへの書き込みは行わない。
 */
void SyncSettings();

}  // namespace AppUi
