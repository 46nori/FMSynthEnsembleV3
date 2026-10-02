//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "screen_settings.h"

#include <array>
#include <vector>

#include "ItemSubMenu.h"
#include "ItemToggle.h"
#include "MenuScreen.h"

#include "MidiMessage.h"
#include "MidiPanelController.h"
#include "OpnBase.h"
#include "info_screen_task.h"
#include "menu_context.h"
#include "midi_ipc.h"
#include "level_widgets.h"

namespace AppUi {

namespace {

MenuScreen* g_settingsScreen = nullptr;
MidiPanelController* g_panel = nullptr;
RhythmLevelWidget* g_rhythmWidget = nullptr;

// LED Mode。enabled(true)="Toggle"(Mode A) / enabled(false)="Note"(Mode B、既定)。
void OnLedModeToggled(bool toggle_selected) {
    if (g_panel != nullptr) {
        g_panel->SetLedMode(!toggle_selected);
    }
}

// g_rhythm_level_offset（減衰step数）をRhythmLevelWidgetの表示値（符号反転）に変換する
int16_t RhythmLevelWidgetValue() {
    return static_cast<int16_t>(-g_rhythm_level_offset);
}

// RhythmLevelWidgetのonChangeコールバック。g_rhythm_level_offsetの更新とRTLの再設定は
// FMバスを扱うCore1で行うため、MIDI Control EventをMidiEngineTaskへ送るだけにする。
void OnRhythmLevelChanged(const int16_t& value) {
    if (value < RhythmLevelWidget::kMinValue || value > 0) {
        return;
    }
    MidiControlEvent ctl{};
    ctl.type = MidiControlType::RhythmLevelOffset;
    ctl.channel = static_cast<uint8_t>(-value);
    ctl.timestamp_us = 0;
    (void)MidiIpcSendMidiControl(ctl);
}

bool HasRhythmModule(const std::array<OpnBase*, 4>& modules) {
    for (const OpnBase* module : modules) {
        if (module != nullptr && module->rhythm() != nullptr) {
            return true;
        }
    }
    return false;
}

}  // namespace

MenuScreen*& BuildSettingsScreen(const InfoScreenTaskContext& ctx, MenuScreen*& volumeScreen,
                                 MenuScreen*& systemInfoScreen) {
    g_panel = ctx.panel;

    // RhythmVolは、デバッガのrmixと同じg_rhythm_level_offsetを0.75dB単位で調整する
    const bool rhythmAvailable = HasRhythmModule(*ctx.modules);
    const int16_t rhythmInitial =
        rhythmAvailable ? RhythmLevelWidgetValue() : RhythmLevelWidget::kUnavailableValue;
    g_rhythmWidget = new RhythmLevelWidget(rhythmInitial, &OnRhythmLevelChanged);

    g_settingsScreen = new MenuScreen(std::vector<MenuItem*>{
        new ItemToggle("LEDmode", "CH-Toggle", "Note", &OnLedModeToggled),
        new VolumeItem("RhythmVol", g_rhythmWidget, rhythmAvailable),
        ITEM_SUBMENU("Volume", volumeScreen),
        ITEM_SUBMENU("System Info", systemInfoScreen),
    });
    return g_settingsScreen;
}

void SyncSettings() {
    if (MenuItem::isEditing() || !IsShowing(g_settingsScreen)) {
        return;
    }
    // リズム音源が無い構成（N/A固定）は同期しない
    if (g_rhythmWidget != nullptr &&
        g_rhythmWidget->getValue() != RhythmLevelWidget::kUnavailableValue &&
        g_rhythmWidget->syncValue(RhythmLevelWidgetValue())) {
        CurrentMenu()->refresh();
    }
}

}  // namespace AppUi
