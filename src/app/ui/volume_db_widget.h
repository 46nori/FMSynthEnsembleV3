//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>
#include <cstdio>

#include "ItemWidget.h"
#include "volume_controller.h"
#include "widget/WidgetRange.h"

namespace AppUi {

/**
 * @brief NJU72343の1チャンネル分の音量を表示・編集するWidget
 * @details 値は`db_x2`（0.5dB単位、dBの2倍）のint16_tで保持する。レンジの下限
 *          （`kMinDbX2`、`Platform::VolumeController::kMinDb`由来）からさらに1ステップ
 *          下げた`kMuteValue`をMute専用の値として扱う。さらにもう1ステップ下げた
 *          `kUnavailableValue`は、編集不可チャンネル（未接続dock、YMF288搭載dockのSSG
 *          入力）専用の表示値で、`VolumeItem`が`available=false`のときの初期値として
 *          のみ使う（UP/DOWNで到達することはない）。
 *          `draw()`をオーバーライドし、`kUnavailableValue`なら"N/A"、`kMuteValue`なら
 *          "Mute"、それ以外は常に`+`/`-`符号を付けた0.5dB表示（整数部は2桁幅、0埋めなし。
 *          例: `+ 5.0dB`、`-95.0dB`）にする。"Mute"は誰でも設定できる通常の状態、
 *          "N/A"は本画面から変更できないことを示す。
 *          `UP`/`DOWN`のたびにコールバック（`onChange`）を呼び、NJU72343へ即座に
 *          書き込む（リアルタイム反映）。`cancelEdit()`をオーバーライドして何もしない
 *          ようにし、`WidgetRange`標準の「編集開始時点の値へ巻き戻す」動作を無効化する。
 *          既にハードウェアへ書き込み済みの値を、表示だけ巻き戻して不整合にしないため。
 */
class VolumeDbWidget : public WidgetRange<int16_t> {
public:
    // Platform::VolumeController::kMinDb/kMaxDbから導出。独自の範囲をハードコードしない。
    static constexpr int16_t kMinDbX2 = static_cast<int16_t>(Platform::VolumeController::kMinDb * 2);
    static constexpr int16_t kMaxDbX2 = static_cast<int16_t>(Platform::VolumeController::kMaxDb * 2);
    static constexpr int16_t kMuteValue = kMinDbX2 - 1;
    static constexpr int16_t kUnavailableValue = kMuteValue - 1;

    VolumeDbWidget(int16_t initialValue, void (*onChange)(const int16_t&))
        : WidgetRange<int16_t>(initialValue, /*step=*/1, kMuteValue, kMaxDbX2, "%s",
                               /*cursorOffset=*/0, /*cycle=*/false, onChange) {}

    void cancelEdit() override {
        // 巻き戻さない: UP/DOWNの時点で既にNJU72343へ反映済みのため、表示もそのまま保つ。
    }

    /**
     * @brief シャドウ状態から表示値だけを同期する
     * @details onChangeを呼ばず、NJU72343への重複書き込みを発生させない。
     * @return 表示値が変化した場合はtrue
     */
    bool syncValue(const int16_t newValue) {
        if (this->value == newValue) {
            return false;
        }
        this->value = newValue;
        return true;
    }

protected:
    uint8_t draw(char* buffer, const uint8_t start) override {
        if (start >= ITEM_DRAW_BUFFER_SIZE) return 0;
        const int16_t value = getValue();
        if (value == kUnavailableValue) {
            return snprintf(buffer + start, ITEM_DRAW_BUFFER_SIZE - start, "N/A");
        }
        if (value == kMuteValue) {
            return snprintf(buffer + start, ITEM_DRAW_BUFFER_SIZE - start, "Mute");
        }
        const int16_t abs_x2 = value < 0 ? static_cast<int16_t>(-value) : value;
        return snprintf(buffer + start, ITEM_DRAW_BUFFER_SIZE - start, "%s%2d.%ddB",
                        value < 0 ? "-" : "+", abs_x2 / 2, (abs_x2 % 2) ? 5 : 0);
    }
};

/**
 * @brief `VolumeDbWidget`を1個持つ音量調整行
 * @details `available`が`false`（未接続dock、またはYMF288搭載dockのSSG入力）の行は、
 *          一覧には表示するが編集不可にする。`process()`をオーバーライドし、編集モード
 *          に入っていない状態での`ENTER`（PUSH）を無視することでガードする。
 */
class VolumeItem : public ItemWidget<int16_t> {
public:
    VolumeItem(const char* text, VolumeDbWidget* widget, bool available)
        : ItemWidget<int16_t>(text, widget), available_(available) {}

protected:
    bool process(LcdMenu* menu, const unsigned char command) override {
        if (!available_ && !MenuItem::isEditing() && command == ENTER) {
            return true;
        }
        return ItemWidget<int16_t>::process(menu, command);
    }

private:
    bool available_;
};

}  // namespace AppUi
