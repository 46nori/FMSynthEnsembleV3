//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

#include "input/InputInterface.h"

class MidiPanelController;
enum class JoystickDirection : uint8_t;

namespace AppUi {

/**
 * @brief ジョイスティックの方向・PUSHをLcdMenuのコマンドに変換するInputInterface実装
 * @details PUSHはENTER、UP/DOWN/RIGHTはそのまま送る。LEFTは横スクロール中
 *          （renderer->viewShift > 0）ならLEFT、先頭に戻っていればBACKに変換する。
 *          スクロールを先頭まで戻した直後の惰性押しで前画面へ戻らないよう、
 *          最後の横スクロールからkBackGuardMsの間はBACKにしない。
 *          方向はオートリピートする（PUSHは対象外）。リピート由来のLEFTはBACKにしない
 *          （押しっぱなしで階層を連続して戻らないため）。PUSH中はリピートを止める。
 *          デバウンスはMidiPanelController側（実体はOpnMidiPanelDriver）で完了済みの
 *          値をポーリングして扱う。
 */
class JoystickInputAdapter final : public InputInterface {
public:
    JoystickInputAdapter(LcdMenu* menu, MidiPanelController* panel);

    void observe() override;

private:
    void Dispatch(JoystickDirection direction, bool repeat);
    void OnLeft(bool repeat);
    void OnRight();
    void Stamp();

    MidiPanelController* panel_;
    JoystickDirection last_direction_;
    bool last_pushed_;
    bool has_scrolled_;
    uint32_t last_scroll_ms_;
    uint32_t next_repeat_ms_;
};

}  // namespace AppUi
