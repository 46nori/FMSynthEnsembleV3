//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "display/CharacterDisplayInterface.h"

class Rw1063Display;

namespace Platform {

/**
 * @brief LcdMenu(extern/LcdMenu)のCharacterDisplayInterfaceをRw1063Displayに橋渡しするアダプタ
 * @details ボード資源（I2Cバス・Rw1063Display）の所有はplatformに閉じ、本クラスは
 *          それをラップするのみ。
 *          物理行0は演奏状態のステータス行として予約し、LcdMenuには物理行1〜3だけを
 *          行0〜2として見せる。ステータス行の書き込みは
 *          `Platform::DisplayWrite()`が直接行うため、本クラスは物理行0に一切触れない。
 */
class LcdCharacterDisplayAdapter final : public CharacterDisplayInterface {
public:
    /** @brief ステータス行として予約する物理行数（先頭からkRowOffset行） */
    static constexpr int kRowOffset = 1;

    explicit LcdCharacterDisplayAdapter(Rw1063Display& display);

    void begin() override;
    void clear() override;
    void show() override;
    void hide() override;
    void draw(uint8_t byte) override;
    void draw(const char* text) override;
    void setCursor(uint8_t col, uint8_t row) override;
    void setBacklight(bool enabled) override;
    void createChar(uint8_t id, uint8_t* c) override;
    void drawBlinker() override;
    void clearBlinker() override;

private:
    Rw1063Display& display_;
};

}  // namespace Platform
