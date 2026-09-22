//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

#include "hardware/i2c.h"

/**
 * @brief RW1063-0A互換 I2Cキャラクタディスプレイドライバ
 * @details ACM2004D-FLW-FBW-IIC（20文字x4行）を対象とする。I2Cバスの所有・初期化は
 *          呼び出し側が行い、本クラスはハンドルを受け取って読み書きするのみ。
 */
class Rw1063Display {
public:
    static constexpr uint8_t kI2cAddress = 0x3F;
    static constexpr int kColumns = 20;
    static constexpr int kRows    = 4;

    /**
     * @brief コンストラクタ
     * @param [in] bus 初期化済みI2Cバスハンドル（非所有）
     */
    explicit Rw1063Display(i2c_inst_t* bus);

    /**
     * @brief ディスプレイの初期化（Function Set / Clear / Display ON）
     */
    void Initialize();

    /**
     * @brief 画面全体をクリアする
     */
    void Clear();

    /**
     * @brief 指定位置に文字列を書き込む
     * @param [in] column カラム番号 (0-19)
     * @param [in] row    行番号 (0-3)
     * @param [in] text   ヌル終端文字列。行末尾を超える分は書き込まない
     */
    void Write(int column, int row, const char* text);

    /**
     * @brief カーソル位置(DDRAMアドレス)を移動する
     * @param [in] column カラム番号 (0-19)
     * @param [in] row    行番号 (0-3)
     * @details 範囲外は無視する。以降の WriteChar() はハードウェアのオートインクリメントで
     *          このカーソルから連続して書き込む。
     */
    void SetCursor(int column, int row);

    /**
     * @brief 現在のカーソル位置に1文字書き込み、カーソルを1つ進める
     * @param [in] ch 書き込む文字コード
     * @details 桁の範囲チェックは行わない（呼び出し側がSetCursor()で管理する前提）。
     */
    void WriteChar(uint8_t ch);

    /**
     * @brief CGRAM(ユーザー定義文字)にパターンを書き込む
     * @param [in] id      CGRAMスロット番号 (0-7)
     * @param [in] pattern 8バイトのドットパターン（各バイト下位5bitを使用）
     * @details 実行後はDDRAMアドレスが不定になるため、続けて文字を描画する場合は
     *          呼び出し側でSetCursor()し直すこと。
     */
    void CreateChar(uint8_t id, const uint8_t pattern[8]);

    /**
     * @brief ディスプレイ全体のON/OFFを切り替える
     */
    void SetDisplayOn(bool on);

    /**
     * @brief カーソル位置の点滅表示を切り替える
     */
    void SetBlink(bool on);

private:
    void WriteCommand(uint8_t cmd);
    void WriteData(uint8_t data);
    void ApplyDisplayControl();

    i2c_inst_t* bus_;
    uint8_t display_control_bits_;  // Display ON/OFF Controlコマンドの現在値(D/C/Bビット)
};
