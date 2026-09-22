//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "lcd_character_display_adapter.h"

#include <cstring>

#include "Rw1063Display.h"

namespace Platform {

LcdCharacterDisplayAdapter::LcdCharacterDisplayAdapter(Rw1063Display& display) : display_(display) {}

void LcdCharacterDisplayAdapter::begin() {
    display_.Initialize();
}

void LcdCharacterDisplayAdapter::clear() {
    // display_.Clear()は物理ディスプレイ全体(行0のステータス行含む)を消してしまうため
    // 使わない。LcdMenuが所有する行(kRowOffset以降)だけを空白で上書きする。
    char blank[Rw1063Display::kColumns + 1];
    std::memset(blank, ' ', Rw1063Display::kColumns);
    blank[Rw1063Display::kColumns] = '\0';
    for (int row = kRowOffset; row < Rw1063Display::kRows; ++row) {
        display_.Write(0, row, blank);
    }
}

void LcdCharacterDisplayAdapter::show() {
    display_.SetDisplayOn(true);
}

void LcdCharacterDisplayAdapter::hide() {
    display_.SetDisplayOn(false);
}

void LcdCharacterDisplayAdapter::draw(uint8_t byte) {
    display_.WriteChar(byte);
}

void LcdCharacterDisplayAdapter::draw(const char* text) {
    for (const char* p = text; *p != '\0'; ++p) {
        display_.WriteChar(static_cast<uint8_t>(*p));
    }
}

void LcdCharacterDisplayAdapter::setCursor(uint8_t col, uint8_t row) {
    display_.SetCursor(col, row + kRowOffset);
}

void LcdCharacterDisplayAdapter::setBacklight(bool /*enabled*/) {
    // ACM2004D-FLW-FBW-IICはBL+/BL-がハード配線のみでソフト制御ピンが無いため no-op
}

void LcdCharacterDisplayAdapter::createChar(uint8_t id, uint8_t* c) {
    display_.CreateChar(id, c);
}

void LcdCharacterDisplayAdapter::drawBlinker() {
    display_.SetBlink(true);
}

void LcdCharacterDisplayAdapter::clearBlinker() {
    display_.SetBlink(false);
}

}  // namespace Platform
