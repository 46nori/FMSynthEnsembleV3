//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "display.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "Rw1063Display.h"
#include "lcd_character_display_adapter.h"

namespace Platform {

namespace {
constexpr uint kI2cSda = 20;                // GPIO20: I2C0 SDA
constexpr uint kI2cScl = 21;                // GPIO21: I2C0 SCL
constexpr uint32_t kI2cBaudrate = 400'000;  // Fast-mode(400kHz)

Rw1063Display g_display(i2c0);
LcdCharacterDisplayAdapter g_lcd_character_display(g_display);
}  // namespace

void InitializeI2cBus() {
    i2c_init(i2c0, kI2cBaudrate);
    gpio_set_function(kI2cSda, GPIO_FUNC_I2C);
    gpio_set_function(kI2cScl, GPIO_FUNC_I2C);
    // 内部プルアップを無効化
    gpio_disable_pulls(kI2cSda);
    gpio_disable_pulls(kI2cScl);
}

void DisplayWrite(int column, int row, const char* text) {
    g_display.Write(column, row, text);
}

CharacterDisplayInterface& GetCharacterDisplay() {
    return g_lcd_character_display;
}

}  // namespace Platform
