//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "OpnMidiPanelDriver.h"

#include "hardware/timer.h"
#include "pico/time.h"

namespace {

// 全LED OFF用のPortA値
constexpr uint8_t kBlankPortA = 0x0F;

// 列選択用のPortA値
constexpr uint8_t kColumnPortA[4] = {0x0E, 0x0D, 0x0B, 0x07};

// PB bit7 = LED モード選択（Active Low）
constexpr uint8_t kPbBit7LedModeMask = 0x80;

// Reset 通知点滅パラメータ（変更する場合はここを編集する）
constexpr uint32_t kResetFlashRateHz       = 4;      // 点滅周期 [回/秒]
constexpr uint32_t kResetFlashBlinkCount   = 2;      // 点滅回数（トリガー1回あたり）
constexpr uint32_t kResetFlashHalfPeriodMs = 1000u / (kResetFlashRateHz * 2u);  // ON/OFF 各半周期
constexpr uint32_t kResetFlashTotalPhases  = kResetFlashBlinkCount * 2u;        // ON/OFF の合計フェーズ数

}

OpnMidiPanelDriver::OpnMidiPanelDriver(IIoPort& io)
    : io_(io),
      config_{.debounce_ms = 20, .toggle_hold_ms = 30, .long_press_ms = 2000, .settle_us = 100},
      host_led_bitmap_(0),
      switch_bitmap_(0xffff),  // 全 CH トグル ON
      long_press_bitmap_(0),
      scan_column_(0),
      reset_flash_{.active = false, .phase_index = 0, .phase_start_ms = 0} {
    for (auto& ch : channels_) {
        ch.latched = true;
        ch.stable_pressed = false;
        ch.last_raw = false;
        ch.raw_change_ms = 0;
        ch.press_start_ms = 0;
    }
}

void OpnMidiPanelDriver::Initialize() {
    io_.set_port_direction(true, false);
    io_.write_port_a(kBlankPortA);

    // 1 フレーム分スキャンして初期押下状態を確定
    for (int i = 0; i < 4; ++i) {
        Tick();
    }
}

void OpnMidiPanelDriver::SetLedBitmap(uint16_t led_bitmap) {
    host_led_bitmap_ = led_bitmap;
}

bool OpnMidiPanelDriver::IsMidiReset() const {
    return (long_press_bitmap_ & (1u << 9)) != 0;  // CH10 長押し中
}

void OpnMidiPanelDriver::FlashAllLeds() {
    reset_flash_.active = true;
    reset_flash_.phase_index = 0;
    reset_flash_.phase_start_ms = to_ms_since_boot(get_absolute_time());
}

// モーメンタリ入力をデバウンスし、ホールド時間でトグル／長押しを判定する。
void OpnMidiPanelDriver::UpdateChannelInput(int ch_index, bool raw_pressed, uint32_t now_ms) {
    auto& s = channels_[ch_index];
    const uint16_t ch_bit = static_cast<uint16_t>(1u << ch_index);

    if (raw_pressed != s.last_raw) {
        s.last_raw = raw_pressed;
        s.raw_change_ms = now_ms;
    }

    if ((now_ms - s.raw_change_ms) < config_.debounce_ms) {
        return;
    }

    // 押下継続中: 長押しビットを更新（トグルは離したとき）
    if (s.stable_pressed && raw_pressed) {
        if ((now_ms - s.press_start_ms) >= config_.long_press_ms) {
            long_press_bitmap_ |= ch_bit;
        } else {
            long_press_bitmap_ &= ~ch_bit;
        }
        return;
    }

    if (s.stable_pressed == raw_pressed) {
        return;
    }

    if (!raw_pressed && s.stable_pressed) {
        const uint32_t held = now_ms - s.press_start_ms;
        const bool was_long_press = held >= config_.long_press_ms;
        long_press_bitmap_ &= ~ch_bit;
        // 長押し成立時はトグルしない
        if (!was_long_press && held >= config_.toggle_hold_ms) {
            s.latched = !s.latched;
        }
    }

    if (raw_pressed) {
        s.press_start_ms = now_ms;
        long_press_bitmap_ &= ~ch_bit;
    }

    s.stable_pressed = raw_pressed;
}

void OpnMidiPanelDriver::RebuildSwitchBitmap() {
    uint16_t bm = 0;
    for (int i = 0; i < 16; ++i) {
        if (channels_[i].latched) {
            bm |= static_cast<uint16_t>(1u << i);
        }
    }
    switch_bitmap_ = bm;
}

void OpnMidiPanelDriver::UpdateResetFlash(uint32_t now_ms) {
    if (!reset_flash_.active) {
        return;
    }

    if ((now_ms - reset_flash_.phase_start_ms) < kResetFlashHalfPeriodMs) {
        return;
    }

    ++reset_flash_.phase_index;
    reset_flash_.phase_start_ms = now_ms;
    if (reset_flash_.phase_index >= kResetFlashTotalPhases) {
        reset_flash_.active = false;
    }
}

uint16_t OpnMidiPanelDriver::ResolveEffectiveLedBitmap(bool led_mode_midi) const {
    if (reset_flash_.active) {
        return ((reset_flash_.phase_index % 2u) == 0u) ? 0xffffu : 0x0000u;
    }
    return led_mode_midi ? host_led_bitmap_ : switch_bitmap_;
}

// 列スロット: マトリックス読取 → トグル更新 → LED 出力
void OpnMidiPanelDriver::Tick() {
    const uint8_t col = scan_column_;
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    // 列切替と PB 安定待ち
    io_.write_port_a(kColumnPortA[col]);
    if (config_.settle_us > 0) {
        busy_wait_us(config_.settle_us);
    }

    const uint8_t pb_raw = io_.read_port_b();
    const uint8_t pressed_rows = static_cast<uint8_t>((~pb_raw) & 0x0Fu);
    const bool led_mode_midi = (pb_raw & kPbBit7LedModeMask) == 0u;  // PB bit7: Low=モードB

    for (uint8_t row = 0; row < 4u; ++row) {
        const int ch_index = static_cast<int>(col * 4u + row);
        const bool raw_pressed = (pressed_rows & (1u << row)) != 0;
        UpdateChannelInput(ch_index, raw_pressed, now_ms);
    }

    RebuildSwitchBitmap();
    UpdateResetFlash(now_ms);

    const uint16_t effective_led = ResolveEffectiveLedBitmap(led_mode_midi);

    uint8_t led_row = 0;
    for (uint8_t row = 0; row < 4u; ++row) {
        const int bit = static_cast<int>(col * 4u + row);
        if ((effective_led & (1u << bit)) != 0) {
            led_row |= static_cast<uint8_t>(1u << row);
        }
    }

    if (led_row != 0) {
        const uint8_t pa = static_cast<uint8_t>((led_row << 4) | kColumnPortA[col]);
        io_.write_port_a(pa);
    } else {
        io_.write_port_a(kBlankPortA);  // 当列 LED なし
    }

    scan_column_ = static_cast<uint8_t>((col + 1u) % 4u);
}
