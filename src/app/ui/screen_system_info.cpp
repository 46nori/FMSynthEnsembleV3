//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "screen_system_info.h"

#include <Arduino.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ItemLabel.h"
#include "MenuScreen.h"

#include "MidiFactory.h"
#include "MidiPanelController.h"
#include "OpnBase.h"
#include "display.h"
#include "info_screen_task.h"
#include "menu_context.h"
#include "task_config.h"

namespace AppUi {

namespace {

MenuScreen* g_systemInfoScreen = nullptr;
const InfoScreenTaskContext* g_ctx = nullptr;

ItemLabel* g_voiceLabel = nullptr;
char g_voiceLine[Platform::kDisplayColumns + 1] = {};
uint32_t g_lastRefreshMs = 0;

const char* ModuleName(const OpnBase* module) {
    if (module != nullptr) {
        switch (module->chip_kind()) {
        case ChipKind::YM2203: return "YM2203";
        case ChipKind::YM2608: return "YM2608";
        case ChipKind::YMF288: return "YMF288";
        }
    }
    return "";
}

// ItemLabelのテキストを差し替えるのみ。再描画は呼び出し側で行う
void UpdateVoiceLine() {
    const int active = g_ctx->factory->GetActiveNoteVoiceCount();
    const int csmReserved = g_ctx->factory->GetCsmReservedVoiceCount();
    const int total = active + csmReserved;
    std::snprintf(g_voiceLine, sizeof(g_voiceLine), "Voice:%02d/%02d CSM:%02d",
                  active, total, csmReserved);
    g_voiceLabel->setText(g_voiceLine);
}

}  // namespace

MenuScreen*& BuildSystemInfoScreen(const InfoScreenTaskContext& ctx) {
    g_ctx = &ctx;

    // Dock構成は起動時固定、Voice/CSM数のみ後で更新する
    static char dockLine1[Platform::kDisplayColumns + 1];
    static char dockLine2[Platform::kDisplayColumns + 1];
    std::memset(dockLine1, ' ', Platform::kDisplayColumns);
    std::memset(dockLine2, ' ', Platform::kDisplayColumns);
    dockLine1[Platform::kDisplayColumns] = '\0';
    dockLine2[Platform::kDisplayColumns] = '\0';
    for (int dock = 0; dock < 4; ++dock) {
        const bool panelConnected = (dock == ctx.midiPanelDock) && ctx.panel->IsConnected();
        char field[10];
        std::snprintf(field, sizeof(field), "%d:%-6s%c",
                      dock, ModuleName((*ctx.modules)[dock]), panelConnected ? '*' : ' ');
        char* line = (dock < 2) ? dockLine1 : dockLine2;
        const int column = (dock % 2 == 0) ? 0 : 9;
        std::memcpy(&line[column], field, std::strlen(field));
    }

    g_voiceLabel = new ItemLabel(g_voiceLine);
    UpdateVoiceLine();

    g_systemInfoScreen = new MenuScreen(std::vector<MenuItem*>{
        new ItemLabel(dockLine1),
        new ItemLabel(dockLine2),
        g_voiceLabel,
    });
    return g_systemInfoScreen;
}

void SyncSystemInfo() {
    if (g_ctx == nullptr) {
        return;
    }
    const uint32_t nowMs = millis();
    if (nowMs - g_lastRefreshMs < INFO_SCREEN_SYSINFO_REFRESH_MS) {
        return;
    }
    g_lastRefreshMs = nowMs;
    UpdateVoiceLine();
    RefreshIfShowing(g_systemInfoScreen);
}

}  // namespace AppUi
