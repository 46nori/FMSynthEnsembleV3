//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

class CharacterDisplayInterface;

/**
 * @brief ディスプレイの所有・初期化・書き込み
 * @details GPIO割り当て・ディスプレイのコントローラ種別・バス接続は本ファイルに閉じ込め、
 *          上位へは初期化APIと座標指定の文字列書き込みAPIのみ公開する。
 */
namespace Platform {

/** @brief ディスプレイの表示桁数・行数（ACM2004D-FLW-FBW-IIC: 20文字x4行） */
constexpr int kDisplayColumns = 20;
constexpr int kDisplayRows    = 4;

/**
 * @brief ディスプレイ用I2Cを初期化する
 */
void InitializeI2cBus();

/**
 * @brief 指定位置に文字列を書き込む
 * @param [in] column カラム番号 (0-19)
 * @param [in] row    行番号 (0-3)
 * @param [in] text   ヌル終端文字列。行末尾を超える分は書き込まない
 * @details ステータス行（行0）専用。行1-3はLcdMenu経由（GetCharacterDisplay()）で
 *          描画するため、この関数からは触れないこと。
 */
void DisplayWrite(int column, int row, const char* text);

/**
 * @brief LcdMenu(extern/LcdMenu)向けのCharacterDisplayInterfaceを取得する
 * @details 実体はLcdCharacterDisplayAdapter。
 *          ディスプレイ自体の初期化(Function Set / Clear / Display ON)は
 *          `CharacterDisplayRenderer::begin()`経由でこのインスタンスに対して行われる。
 */
CharacterDisplayInterface& GetCharacterDisplay();

}  // namespace Platform
