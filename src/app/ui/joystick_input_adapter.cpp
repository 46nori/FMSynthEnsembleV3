//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "joystick_input_adapter.h"

#include <Arduino.h>

#include "LcdMenu.h"
#include "MidiPanelController.h"

namespace AppUi {

namespace {
// 横スクロール操作（viewShiftが変化したLEFT/RIGHT）からこの時間内のLEFTはBACKにしない。
constexpr uint32_t kBackGuardMs = 800;
// オートリピート。方向を押し続けたとき、最初のリピートまでの遅延と、以降の発火間隔。
constexpr uint32_t kRepeatDelayMs = 450;
constexpr uint32_t kRepeatIntervalMs = 100;
}  // namespace

JoystickInputAdapter::JoystickInputAdapter(LcdMenu* menu, MidiPanelController* panel)
    : InputInterface(menu),
      panel_(panel),
      last_direction_(JoystickDirection::None),
      last_pushed_(false),
      has_scrolled_(false),
      last_scroll_ms_(0),
      next_repeat_ms_(0) {}

void JoystickInputAdapter::OnLeft(bool repeat) {
    MenuRenderer* renderer = menu->getRenderer();
    if (renderer->viewShift > 0) {
        menu->process(LEFT);
        Stamp();
    } else if (!repeat && (!has_scrolled_ || millis() - last_scroll_ms_ >= kBackGuardMs)) {
        menu->process(BACK);
    }
}

void JoystickInputAdapter::OnRight() {
    MenuRenderer* renderer = menu->getRenderer();
    const uint8_t before = renderer->viewShift;
    menu->process(RIGHT);
    if (renderer->viewShift != before) {
        Stamp();
    }
}

void JoystickInputAdapter::Stamp() {
    has_scrolled_ = true;
    last_scroll_ms_ = millis();
}

void JoystickInputAdapter::Dispatch(JoystickDirection direction, bool repeat) {
    switch (direction) {
    case JoystickDirection::Up:    menu->process(UP);    break;
    case JoystickDirection::Down:  menu->process(DOWN);  break;
    case JoystickDirection::Left:  OnLeft(repeat);       break;
    case JoystickDirection::Right: OnRight();            break;
    case JoystickDirection::None:  break;
    }
}

void JoystickInputAdapter::observe() {
    const uint32_t now = millis();
    const bool pushed = panel_->IsJoystickPushed();
    const JoystickDirection direction = panel_->GetJoystickDirection();

    if (direction != last_direction_) {
        last_direction_ = direction;
        next_repeat_ms_ = now + kRepeatDelayMs;
        Dispatch(direction, false);
    } else if (pushed) {
        // PUSH中は方向値が保持されるだけなので、リピートを止め、解除後に遅延から数え直す。
        next_repeat_ms_ = now + kRepeatDelayMs;
    } else if (direction != JoystickDirection::None &&
               static_cast<int32_t>(now - next_repeat_ms_) >= 0) {
        next_repeat_ms_ = now + kRepeatIntervalMs;
        Dispatch(direction, true);
    }

    if (pushed && !last_pushed_) {
        menu->process(ENTER);
    }
    last_pushed_ = pushed;
}

}  // namespace AppUi
