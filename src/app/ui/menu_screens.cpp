//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "menu_screens.h"

#include <vector>

#include "ItemCommand.h"
#include "ItemLabel.h"
#include "ItemSubMenu.h"
#include "LcdMenu.h"
#include "MenuScreen.h"

#include "info_screen_task.h"
#include "menu_context.h"
#include "screen_settings.h"
#include "screen_system_info.h"
#include "screen_volume.h"

#if BUILD_SD_CARD
#include "screen_smf.h"
#endif

namespace AppUi {

namespace {

MenuScreen* g_rootScreen = nullptr;

#if BUILD_SD_CARD
void OnNowPlaying() {
    OpenNowPlaying(g_rootScreen);
}
#else
// SDカード無効時にPlay SMF / Playlistからぶら下げる画面。ItemSubMenuは遷移先の画面を
// ポインタ変数への参照で保持するため、静的な変数に置く
MenuScreen* g_playSmfDisabledScreen = nullptr;
MenuScreen* g_playlistDisabledScreen = nullptr;
#endif

}  // namespace

MenuScreen* BuildRootScreen(const InfoScreenTaskContext& ctx) {
    MenuScreen*& settingsScreen =
        BuildSettingsScreen(ctx, BuildVolumeScreen(), BuildSystemInfoScreen(ctx));

#if BUILD_SD_CARD
    const SmfScreens smf = BuildSmfScreens();
    g_rootScreen = new MenuScreen(std::vector<MenuItem*>{
        ITEM_COMMAND("Now Playing", &OnNowPlaying),
        ITEM_SUBMENU("Play SMF", smf.playSmf),
        ITEM_SUBMENU("Playlist", smf.playlist),
        ITEM_SUBMENU("Play Options", smf.playOptions),
        ITEM_SUBMENU("Settings", settingsScreen),
    });
#else
    g_playSmfDisabledScreen = new MenuScreen(std::vector<MenuItem*>{
        new ItemLabel("SD card disabled"),
    });
    g_playlistDisabledScreen = new MenuScreen(std::vector<MenuItem*>{
        new ItemLabel("SD card disabled"),
    });
    g_rootScreen = new MenuScreen(std::vector<MenuItem*>{
        ITEM_SUBMENU("Play SMF", g_playSmfDisabledScreen),
        ITEM_SUBMENU("Playlist", g_playlistDisabledScreen),
        ITEM_SUBMENU("Settings", settingsScreen),
    });
#endif
    return g_rootScreen;
}

void SetMenu(LcdMenu* menu) {
    AttachMenu(menu);
}

void SyncBeforeInput() {
    if (CurrentMenu() == nullptr) {
        return;
    }
    SyncSettings();
    SyncVolume();
}

void SyncAfterInput() {
    if (CurrentMenu() == nullptr) {
        return;
    }
#if BUILD_SD_CARD
    SyncSmf();
#endif
    SyncSystemInfo();
}

}  // namespace AppUi
