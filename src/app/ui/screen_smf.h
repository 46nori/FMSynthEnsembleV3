//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

class MenuScreen;

/**
 * @brief SMF再生に関わる画面（Play SMF / Playlist / Play Options / Transport）
 * @details BUILD_SD_CARD=ONのときだけビルドする。
 */
namespace AppUi {

/**
 * @brief Home画面からぶら下げるSMF再生関連の画面
 * @details 各メンバは、画面を指す静的なポインタ変数への参照（ITEM_SUBMENU()へそのまま渡せる）。
 */
struct SmfScreens {
    MenuScreen*& playSmf;      ///< SDカード上の全ファイルの一覧
    MenuScreen*& playlist;     ///< playlistフォルダ内のファイルの一覧
    MenuScreen*& playOptions;  ///< Repeat / Shuffle / Playback / Tempo（既定倍率）
};

/**
 * @brief SMF再生関連の画面を構築する
 * @details 起動時に1度だけ呼ぶ。SDカード上のSMFファイル列挙もここで行う。
 *          Transport画面は曲を選んだときにだけ移る画面のため、戻り値には含めない。
 */
SmfScreens BuildSmfScreens();

/**
 * @brief 再生中（Playing/Paused）ならTransport画面へ移る
 * @param [in] from Transport画面から戻る先の画面
 * @details Idleのときは何もしない。
 */
void OpenNowPlaying(MenuScreen* from);

/**
 * @brief SMF再生状態を画面の表示へ反映する
 * @details 毎周期呼ぶ。Pause/Resume、Repeat/Shuffle/Playback、Tempoの表示を再生状態に
 *          合わせ、Transport画面の表示中に再生が終わったら元の一覧へ戻す。
 */
void SyncSmf();

}  // namespace AppUi
