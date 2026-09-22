//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "Rw1063Display.h"

#include "pico/time.h"

namespace {
// コマンド/データ判別用の制御バイト（RW1063互換I2Cプロトコル）
constexpr uint8_t kControlCommand = 0x00;
constexpr uint8_t kControlData    = 0x40;

// 各行のDDRAM先頭アドレス（20x4キャラクタLCDの標準配置）
constexpr uint8_t kRowAddress[Rw1063Display::kRows] = {0x00, 0x40, 0x14, 0x54};

constexpr uint8_t kCmdFunctionSet  = 0x38;  // 8bit / 2-line(4行LCDもN=1) / 5x8フォント
constexpr uint8_t kCmdClearDisplay = 0x01;
constexpr uint8_t kCmdSetCgramAddr = 0x40;
constexpr uint8_t kCmdSetDdramAddr = 0x80;

// Display ON/OFF Control: 0x08 | D(Display)<<2 | C(Cursor)<<1 | B(Blink)
constexpr uint8_t kCmdDisplayControlBase = 0x08;
constexpr uint8_t kBitDisplayOn = 1u << 2;
constexpr uint8_t kBitBlinkOn   = 1u << 0;
constexpr uint8_t kCmdDisplayOn = kCmdDisplayControlBase | kBitDisplayOn;  // 初期値: Display ON, Cursor/Blink OFF
}  // namespace

Rw1063Display::Rw1063Display(i2c_inst_t* bus) : bus_(bus), display_control_bits_(kCmdDisplayOn) {}

void Rw1063Display::WriteCommand(uint8_t cmd) {
    uint8_t buf[2] = {kControlCommand, cmd};
    i2c_write_blocking(bus_, kI2cAddress, buf, sizeof(buf), false);
}

void Rw1063Display::WriteData(uint8_t data) {
    uint8_t buf[2] = {kControlData, data};
    i2c_write_blocking(bus_, kI2cAddress, buf, sizeof(buf), false);
}

void Rw1063Display::ApplyDisplayControl() {
    WriteCommand(display_control_bits_);
}

void Rw1063Display::SetCursor(int column, int row) {
    if (row < 0 || row >= kRows || column < 0 || column >= kColumns) {
        return;
    }
    WriteCommand(kCmdSetDdramAddr | (kRowAddress[row] + column));
}

void Rw1063Display::WriteChar(uint8_t ch) {
    WriteData(ch);
}

void Rw1063Display::CreateChar(uint8_t id, const uint8_t pattern[8]) {
    id &= 0x07;  // CGRAMスロットは0-7の8個
    WriteCommand(kCmdSetCgramAddr | (id * 8));
    for (int i = 0; i < 8; ++i) {
        WriteData(pattern[i] & 0x1F);  // 5x8フォント: 有効なのは下位5bit
    }
}

void Rw1063Display::SetDisplayOn(bool on) {
    if (on) {
        display_control_bits_ |= kBitDisplayOn;
    } else {
        display_control_bits_ &= static_cast<uint8_t>(~kBitDisplayOn);
    }
    ApplyDisplayControl();
}

void Rw1063Display::SetBlink(bool on) {
    if (on) {
        display_control_bits_ |= kBitBlinkOn;
    } else {
        display_control_bits_ &= static_cast<uint8_t>(~kBitBlinkOn);
    }
    ApplyDisplayControl();
}

void Rw1063Display::Initialize() {
    sleep_ms(50);  // 電源投入直後の安定待ち
    WriteCommand(kCmdFunctionSet);
    Clear();
    display_control_bits_ = kCmdDisplayOn;
    ApplyDisplayControl();
}

void Rw1063Display::Clear() {
    WriteCommand(kCmdClearDisplay);
    sleep_ms(2);  // Clear Displayの実行時間(データシート規定0.76ms)を確保
}

void Rw1063Display::Write(int column, int row, const char* text) {
    SetCursor(column, row);
    for (int c = column; *text != '\0' && c < kColumns; ++c, ++text) {
        WriteChar(static_cast<uint8_t>(*text));
    }
}
