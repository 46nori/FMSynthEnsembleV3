//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

class LcdMenu;
class MenuScreen;

/**
 * @brief 各画面モジュール（screen_*.cpp）が共有するLcdMenuへのアクセス
 * @details LcdMenuのコールバックは引数を取らない関数ポインタで、画面遷移や再描画に使う
 *          LcdMenuを渡せない。そのため、登録された1つのLcdMenuをここで保持する。
 */
namespace AppUi {

/**
 * @brief 画面モジュールが使うLcdMenuを登録する
 */
void AttachMenu(LcdMenu* menu);

/**
 * @brief 登録済みのLcdMenuを返す（未登録ならnullptr）
 */
LcdMenu* CurrentMenu();

/**
 * @brief 指定の画面が現在表示中かを返す
 * @details LcdMenuが未登録、またはscreenがnullptrの場合はfalse。
 */
bool IsShowing(const MenuScreen* screen);

/**
 * @brief 指定の画面が現在表示中なら再描画する
 */
void RefreshIfShowing(const MenuScreen* screen);

}  // namespace AppUi
