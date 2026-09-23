//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

class LcdMenu;
class MenuScreen;
struct InfoScreenTaskContext;

/**
 * @brief LcdMenuの画面ツリー
 * @details Home配下にNow Playing / Play SMF / Playlist / Play Options / Settingsを持つ（System InfoはSettings配下）。
 *          extern/LcdMenuのオブジェクトグラフを組み立てるappの合成ルート。
 */
namespace AppUi {

/**
 * @brief メニュー画面ツリーを構築する
 * @param [in] ctx タスク起動時に確定しているコンテキスト（非所有、プログラム終了まで有効なこと）
 * @return ルート(Home)画面
 * @details 起動時に1度だけ呼ぶ。SDカード上のSMFファイル列挙もここで行う。
 */
MenuScreen* BuildRootScreen(const InfoScreenTaskContext& ctx);

/**
 * @brief System Info画面のVoice/CSM表示を最新の値に更新する
 * @details ItemLabelのテキストを差し替えるのみ。画面への反映（再描画）は呼び出し側が
 *          GetSystemInfoScreen()が現在表示中の画面と一致する場合に限り LcdMenu::refresh()
 *          を呼ぶこと。
 */
void RefreshSystemInfo();

/**
 * @brief 画面遷移に使うLcdMenuを登録する
 * @details 曲を選んだときのTransport画面への遷移など、コールバックからの画面切り替えに
 *          使う。BuildRootScreen()の後、メインループに入る前に1度だけ呼ぶ。
 */
void SetMenu(LcdMenu* menu);

/**
 * @brief SMF再生状態をメニューの表示へ反映する
 * @details 毎周期呼ぶ。Pause/Resumeのラベル、Repeat/Shuffleのラベルを再生状態に合わせ、
 *          Transport画面の表示中に再生が終わったら元の一覧へ戻す。
 */
void UpdatePlaybackUi();

/**
 * @brief 音量調整の表示値を現在の設定値へ同期する
 * @details 毎周期呼ぶ。編集中でない場合だけ、Volume画面の表示中はVolumeControllerの
 *          シャドウ状態へ、Settings画面の表示中はRhythmVol行をg_rhythm_level_offsetへ
 *          同期する。NJU72343やFMレジスタへの書き込みは行わない。
 */
void RefreshVolumeUi();

/**
 * @brief System Info画面を返す
 */
MenuScreen* GetSystemInfoScreen();

}  // namespace AppUi
