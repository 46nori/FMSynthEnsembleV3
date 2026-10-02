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
 *          extern/LcdMenuのオブジェクトグラフを組み立てるappの合成ルート。各画面の構築と
 *          同期処理は画面ごとのモジュール（screen_*.h）が持ち、ここではHome画面の組み立てと
 *          同期処理の振り分けだけを行う。
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
 * @brief 画面遷移に使うLcdMenuを登録する
 * @details 曲を選んだときのTransport画面への遷移など、コールバックからの画面切り替えに
 *          使う。BuildRootScreen()の後、メインループに入る前に1度だけ呼ぶ。
 */
void SetMenu(LcdMenu* menu);

/**
 * @brief 編集対象の値を現在の設定値へ同期する（入力処理の前）
 * @details 毎周期、ジョイスティック入力を処理する前に呼ぶ。音量調整（Volume画面・
 *          RhythmVol行）の表示値を、編集中でない場合だけ実際の値へ合わせる。入力の前に
 *          行うのは、デバッガなどによる変更を取り込んだ値から編集を始めるため。表示中の
 *          画面に変化があれば再描画まで行う。NJU72343やFMレジスタへの書き込みは行わない。
 */
void SyncBeforeInput();

/**
 * @brief 再生状態とシステム情報を画面の表示へ反映する（入力処理の後）
 * @details 毎周期、ジョイスティック入力を処理した後に呼ぶ。SMF再生状態（Pause/Resume、
 *          Repeat/Shuffle/Playback、Tempoの表示とTransport画面の自動復帰）とSystem Infoの
 *          Voice/CSM数を反映する。入力の後に行うのは、入力で起きた画面遷移を同じ周期の
 *          判定に含めるため。表示中の画面に変化があれば再描画まで行う。
 */
void SyncAfterInput();

}  // namespace AppUi
