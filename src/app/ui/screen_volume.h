//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

class MenuScreen;

namespace AppUi {

/**
 * @brief Volume画面を構築する
 * @details 先頭にミキサー出力のOutOffset行、続いてNJU72343の全16CHを1行1CHで並べる。
 *          編集可能な行の設定値はVolumeControllerのシャドウ値で初期化する（オフセットは0）。
 * @return 構築した画面を指す静的なポインタ変数への参照（ITEM_SUBMENU()へそのまま渡せる）
 */
MenuScreen*& BuildVolumeScreen();

/**
 * @brief Volume画面の表示値を現在の設定値へ同期する
 * @details 毎周期呼ぶ。Volume画面の表示中かつ編集中でない場合だけ、VolumeControllerの
 *          シャドウ状態へ同期する。シャドウ値が各CHの設定値+オフセットと異なるCHだけ、
 *          オフセットを差し引いた値（LineMix/LineSampleはシャドウ値そのもの）を設定値として
 *          取り込む。NJU72343への書き込みは行わない。
 */
void SyncVolume();

}  // namespace AppUi
