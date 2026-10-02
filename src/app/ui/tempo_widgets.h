//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>
#include <cstdio>

#include "smf_player_task.h"
#include "level_widgets.h"

namespace AppUi {

/**
 * @brief テンポ倍率を5%刻みで表示・編集するWidget
 * @details 値は倍率を5で割った段数で保持する（RealtimeLevelWidgetのstepは1固定のため）。
 *          Play Optionsの既定倍率に使い、表示は倍率のみ。倍率の値はPlay Optionsの他の行
 *          （`Repeat:   Off`等）と桁をそろえて右詰めにする。
 */
class TempoPercentWidget : public RealtimeLevelWidget {
public:
    static constexpr int16_t kStepPercent = 5;
    static constexpr int16_t kMinValue = SmfPlayer::kTempoScaleMinPercent / kStepPercent;
    static constexpr int16_t kMaxValue = SmfPlayer::kTempoScaleMaxPercent / kStepPercent;
    static constexpr int16_t kDefaultValue = SmfPlayer::kTempoScaleDefaultPercent / kStepPercent;

    explicit TempoPercentWidget(void (*onChange)(const int16_t&))
        : RealtimeLevelWidget(kDefaultValue, kMinValue, kMaxValue, onChange) {}

    static int16_t ValueOf(uint16_t percent) { return static_cast<int16_t>(percent / kStepPercent); }
    static uint16_t PercentOf(int16_t value) { return static_cast<uint16_t>(value * kStepPercent); }

protected:
    uint8_t draw(char* buffer, const uint8_t start) override {
        if (start >= ITEM_DRAW_BUFFER_SIZE) return 0;
        return snprintf(buffer + start, ITEM_DRAW_BUFFER_SIZE - start, "%7u%%",
                        PercentOf(getValue()));
    }
};

static_assert(SmfPlayer::kTempoScaleMinPercent % TempoPercentWidget::kStepPercent == 0 &&
              SmfPlayer::kTempoScaleMaxPercent % TempoPercentWidget::kStepPercent == 0 &&
              SmfPlayer::kTempoScaleDefaultPercent % TempoPercentWidget::kStepPercent == 0,
              "tempo scale range must be a multiple of the UI step");

/**
 * @brief Transport画面のTempo行のWidget
 * @details 再生中の曲の倍率を、倍率適用後のBPMと並べて表示する（例: `132bpm 110%`）。
 *          BPMは曲中のテンポ変更に追従するため、setBpm()で外から与える。
 */
class TempoScaleWidget : public TempoPercentWidget {
public:
    using TempoPercentWidget::TempoPercentWidget;

    // 表示するBPMを更新する。変化した場合はtrue
    bool setBpm(uint16_t bpm) {
        if (bpm_ == bpm) {
            return false;
        }
        bpm_ = bpm;
        return true;
    }

protected:
    uint8_t draw(char* buffer, const uint8_t start) override {
        if (start >= ITEM_DRAW_BUFFER_SIZE) return 0;
        return snprintf(buffer + start, ITEM_DRAW_BUFFER_SIZE - start, "%3ubpm %3u%%",
                        bpm_, PercentOf(getValue()));
    }

private:
    uint16_t bpm_ = 0;
};

}  // namespace AppUi
