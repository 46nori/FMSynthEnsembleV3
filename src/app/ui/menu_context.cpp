//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "menu_context.h"

#include "LcdMenu.h"

namespace AppUi {

namespace {

LcdMenu* g_menu = nullptr;

}  // namespace

void AttachMenu(LcdMenu* menu) {
    g_menu = menu;
}

LcdMenu* CurrentMenu() {
    return g_menu;
}

bool IsShowing(const MenuScreen* screen) {
    return g_menu != nullptr && screen != nullptr && g_menu->getScreen() == screen;
}

void RefreshIfShowing(const MenuScreen* screen) {
    if (IsShowing(screen)) {
        g_menu->refresh();
    }
}

}  // namespace AppUi
