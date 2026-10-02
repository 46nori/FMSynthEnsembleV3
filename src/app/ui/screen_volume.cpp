//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "screen_volume.h"

#include <array>
#include <utility>
#include <vector>

#include "MenuScreen.h"

#include "menu_context.h"
#include "volume_controller.h"
#include "level_widgets.h"

namespace AppUi {

namespace {

MenuScreen* g_volumeScreen = nullptr;

// NJU72343の物理配線（chip/channel）とLCD表示名の対応。
struct VolumeChannelDef {
    uint8_t chip_idx;  // 0=CHIP_ADR0, 1=CHIP_ADR1
    uint8_t channel;   // 0=A, 1=B, ... 7=H
    const char* label;
    bool offset;       // ミキサー出力のオフセット（OutOffset行）の対象か。サンプリング用のLineMix/LineSmpは対象外
};

// dock順(0-3)に並べ、dock内はFM-L/FM-R/SSGの順。最後にLineMix-L/R、LineSmp-L/Rを置く。
constexpr VolumeChannelDef kVolumeChannels[] = {
    {0, 2, "0-FM-L", true}, {1, 2, "0-FM-R", true}, {0, 0, "0-SSG ", true},
    {0, 4, "1-FM-L", true}, {1, 4, "1-FM-R", true}, {0, 1, "1-SSG ", true},
    {0, 3, "2-FM-L", true}, {1, 3, "2-FM-R", true}, {1, 0, "2-SSG ", true},
    {0, 5, "3-FM-L", true}, {1, 5, "3-FM-R", true}, {1, 1, "3-SSG ", true},
    {0, 6, "LineMix-L", false}, {1, 6, "LineMix-R", false},
    {0, 7, "LineSmp-L", false}, {1, 7, "LineSmp-R", false},
};
constexpr std::size_t kVolumeChannelCount = sizeof(kVolumeChannels) / sizeof(kVolumeChannels[0]);
std::array<VolumeDbWidget*, kVolumeChannelCount> g_volumeWidgets{};

// 各CHの設定値（db_x2またはVolumeDbWidget::kMuteValue）。行に表示するのはこの値で、
// NJU72343へはオフセット対象CHならオフセットを加えた値（VolumeWithOffset()）を書き込む。
std::array<int16_t, kVolumeChannelCount> g_volumeSettings{};
int16_t g_volumeOffset = 0;  // db_x2

uint8_t VolumeChipAddr(std::size_t index) {
    return Platform::VolumeController::kChipAddr[kVolumeChannels[index].chip_idx];
}

bool IsVolumeChannelAvailable(std::size_t index) {
    return Platform::VolumeController::GetInstance().IsChannelAvailable(
        VolumeChipAddr(index), kVolumeChannels[index].channel);
}

// VolumeControllerのシャドウ値（最後に書き込んだ値）をVolumeDbWidgetの値の形式で返す
int16_t VolumeShadowValue(std::size_t index) {
    const auto shadow = Platform::VolumeController::GetInstance().GetChannelVolume(
        VolumeChipAddr(index), kVolumeChannels[index].channel);
    return shadow.muted ? VolumeDbWidget::kMuteValue : shadow.db_x2;
}

int16_t ClampVolumeDbX2(int value) {
    if (value < VolumeDbWidget::kMinDbX2) return VolumeDbWidget::kMinDbX2;
    if (value > VolumeDbWidget::kMaxDbX2) return VolumeDbWidget::kMaxDbX2;
    return static_cast<int16_t>(value);
}

// オフセット対象CHなら設定値にオフセットを加えた実際の音量、対象外CHは設定値そのもの。
// Muteはオフセットに関わらずMuteのまま。レンジ外はクリップし、下限を超えてもMuteにはしない。
int16_t VolumeWithOffset(std::size_t index) {
    const int16_t setting = g_volumeSettings[index];
    if (!kVolumeChannels[index].offset || setting == VolumeDbWidget::kMuteValue) {
        return setting;
    }
    return ClampVolumeDbX2(setting + g_volumeOffset);
}

// 設定値とオフセットから求めた音量をNJU72343へ書き込む。書き込み済みの値と同じなら何もしない
void ApplyVolume(std::size_t index) {
    const int16_t value = VolumeWithOffset(index);
    if (value == VolumeShadowValue(index)) {
        return;
    }
    auto& vc = Platform::VolumeController::GetInstance();
    const uint8_t chip_addr = VolumeChipAddr(index);
    const uint8_t channel = kVolumeChannels[index].channel;
    if (value == VolumeDbWidget::kMuteValue) {
        vc.SetChannelMute(chip_addr, channel);
    } else {
        vc.SetChannelVolumeDb(chip_addr, channel, value / 2.0f);
    }
}

// VolumeDbWidgetのonChangeコールバック。UP/DOWNのたびに呼ばれ、NJU72343へ即座に書き込む
// （リアルタイム反映）。onChangeには型が固定の関数ポインタしか渡せず、どのCHかを引数で
// 受け取れないため、チャンネルごとに個別の関数をテンプレートで機械的に生成する。
template <std::size_t Index>
void OnVolumeChanged(const int16_t& value) {
    g_volumeSettings[Index] = value;
    ApplyVolume(Index);
}

// VolumeOffsetWidgetのonChangeコールバック。編集可能なオフセット対象CHへ即座に反映する
void OnVolumeOffsetChanged(const int16_t& value) {
    g_volumeOffset = value;
    for (std::size_t i = 0; i < kVolumeChannelCount; ++i) {
        if (kVolumeChannels[i].offset && IsVolumeChannelAvailable(i)) {
            ApplyVolume(i);
        }
    }
}

template <std::size_t... Is>
constexpr std::array<void (*)(const int16_t&), sizeof...(Is)> MakeVolumeChangeCallbacks(std::index_sequence<Is...>) {
    return {&OnVolumeChanged<Is>...};
}

constexpr auto kVolumeChangeCallbacks =
    MakeVolumeChangeCallbacks(std::make_index_sequence<kVolumeChannelCount>{});

}  // namespace

MenuScreen*& BuildVolumeScreen() {
    std::vector<MenuItem*> volumeItems;
    volumeItems.reserve(kVolumeChannelCount + 1);
    volumeItems.push_back(
        new VolumeItem("OutOffset", new VolumeOffsetWidget(&OnVolumeOffsetChanged), true));
    for (std::size_t i = 0; i < kVolumeChannelCount; ++i) {
        const bool available = IsVolumeChannelAvailable(i);
        int16_t initial = VolumeDbWidget::kUnavailableValue;
        if (available) {
            initial = VolumeShadowValue(i);
            g_volumeSettings[i] = initial;
        }
        auto* widget = new VolumeDbWidget(initial, kVolumeChangeCallbacks[i]);
        g_volumeWidgets[i] = widget;
        volumeItems.push_back(new VolumeItem(
            kVolumeChannels[i].label, widget, available));
    }
    g_volumeScreen = new MenuScreen(volumeItems);
    return g_volumeScreen;
}

void SyncVolume() {
    if (MenuItem::isEditing() || !IsShowing(g_volumeScreen)) {
        return;
    }

    bool redraw = false;
    for (std::size_t i = 0; i < kVolumeChannelCount; ++i) {
        int16_t value = VolumeDbWidget::kUnavailableValue;
        if (IsVolumeChannelAvailable(i)) {
            // 設定値+オフセットと異なる値が書き込まれていれば、デバッガなどによる変更として
            // 設定値へ取り込む（オフセット対象CHはオフセット分を差し引く）
            const int16_t shadow = VolumeShadowValue(i);
            if (shadow != VolumeWithOffset(i)) {
                const bool subtract = kVolumeChannels[i].offset && shadow != VolumeDbWidget::kMuteValue;
                g_volumeSettings[i] = subtract ? ClampVolumeDbX2(shadow - g_volumeOffset) : shadow;
            }
            value = g_volumeSettings[i];
        }
        if (g_volumeWidgets[i] != nullptr) {
            redraw = g_volumeWidgets[i]->syncValue(value) || redraw;
        }
    }
    if (redraw) {
        CurrentMenu()->refresh();
    }
}

}  // namespace AppUi
