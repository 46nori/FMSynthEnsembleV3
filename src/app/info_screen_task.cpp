//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "info_screen_task.h"

#include <atomic>
#include <cstdio>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "pico/time.h"

#include "LcdMenu.h"
#include "renderer/CharacterDisplayRenderer.h"

#include "display.h"
#include "lcd_character_display_adapter.h"
#include "midi_ipc.h"
#if BUILD_SD_CARD
#include "smf_player_task.h"
#endif
#include "task_config.h"
#include "ui/joystick_input_adapter.h"
#include "ui/menu_screens.h"

namespace {

// ---------------------------------------------------------------------------
// ステータス行（行0）
// ---------------------------------------------------------------------------
// LcdMenuの描画サイクルとは独立に、このタスクが直接Platform::DisplayWrite()で
// 行0を更新する。行1-3はLcdCharacterDisplayAdapterがオフセットして
// LcdMenuに明け渡す（src/platform/lcd_character_display_adapter.cpp参照）。

TaskHandle_t gInfoScreenTaskHandle = nullptr;

// ステータス表示が無いとき、Home画面の行0に出すデフォルト表示
constexpr const char* kDefaultStatusText = "\xff" " FMSynthEnsembleV3";

// MIDI Reset適用時にステータス行へ出す表示と、その表示時間
// (DisplayWrite()は空白埋めしないため、前の表示が残らないよう20桁に揃えておく)
constexpr const char* kResetStatusLine = "MIDI Reset          ";
constexpr uint64_t kResetDisplayUs = 2'000'000;

char gStatusShadow[Platform::kDisplayColumns + 1] = {};
bool gStatusDirty = false;

void SetStatusLine(const char* text) {
    char formatted[Platform::kDisplayColumns + 1];
    std::snprintf(formatted, sizeof(formatted), "%-*.*s",
                  Platform::kDisplayColumns, Platform::kDisplayColumns, text);
    if (std::memcmp(gStatusShadow, formatted, Platform::kDisplayColumns) != 0) {
        std::memcpy(gStatusShadow, formatted, sizeof(formatted));
        gStatusDirty = true;
    }
}

// ---------------------------------------------------------------------------
// 演奏イベント（Play/Stop/Timeout）
// ---------------------------------------------------------------------------
// Playは演奏イベントごとに呼ばれうるため、ゲート(gPerformanceActive)で重複を排除する。
// 演奏活動が途絶えたことはgLastActivityTimeUsからのTimeoutで判定する。

enum class ScreenEventKind : uint8_t { Play, TrackStart, Stop, Error };

struct ScreenEvent {
    ScreenEventKind kind = ScreenEventKind::Stop;
    // Play: 曲名（空なら未確定）。Error: 表示するメッセージ。Stopでは未使用
    char title[Platform::kDisplayColumns + 1] = {};
};

ScreenEvent gEventMailbox;
volatile bool gEventPending = false;

// 「最初の1件だけ伝える」重複排除ゲート
std::atomic<bool> gPerformanceActive{false};

// 直近の演奏活動時刻。ゲートで抑制された分も含めNotifyPlay()の呼び出しごとに
// 必ず更新する。
volatile uint64_t gLastActivityTimeUs = 0;

// Playが5秒以上発生しなかったと判定するまでの閾値
constexpr uint64_t kPlayTimeoutUs = 5'000'000;

char gCurrentTitle[Platform::kDisplayColumns + 1] = {};

void WriteEvent(ScreenEventKind kind, const char* title) {
    gEventMailbox.kind = kind;
    if (title != nullptr) {
        std::snprintf(gEventMailbox.title, sizeof(gEventMailbox.title), "%s", title);
    } else {
        gEventMailbox.title[0] = '\0';
    }
    gEventPending = true;
}

void PostEventWithGate(ScreenEventKind kind, const char* title, bool active) {
    taskENTER_CRITICAL();
    const bool notify = gInfoScreenTaskHandle != nullptr;
    if (notify) {
        gPerformanceActive.store(active, std::memory_order_relaxed);
        WriteEvent(kind, title);
    }
    taskEXIT_CRITICAL();
    if (notify) {
        xTaskNotifyGive(gInfoScreenTaskHandle);
    }
}

void HandlePlay(const char* title) {
    if (title[0] != '\0') {
        std::snprintf(gCurrentTitle, sizeof(gCurrentTitle), "%s", title);
    }
    SetStatusLine(gCurrentTitle[0] != '\0' ? gCurrentTitle : "Playing");
}

void HandleTrackStart() {
    gCurrentTitle[0] = '\0';
    SetStatusLine("Playing");
}

void HandleStop() {
    gCurrentTitle[0] = '\0';
    SetStatusLine("");
}

// SDカードアクセス失敗等、即座にユーザーへ伝えるべきエラー。Play同様5秒でクリアされる
// (CheckTimeout())。
void HandleError(const char* message) {
    gCurrentTitle[0] = '\0';
    SetStatusLine(message);
}

// 通知が無くても毎周期呼ぶ。演奏中にPlayが5秒途絶したらステータス行を消す。
void CheckTimeout() {
    if (!gPerformanceActive.load(std::memory_order_acquire)) {
        return;
    }
#if BUILD_SD_CARD
    // SMF再生中（Pause中を含む）は、演奏イベントが途絶えても（休符・Pause）ステータス行を
    // 消さない。セッションの終了はNotifyStop()で伝わる。
    if (SmfPlayer::GetStatus().state != SmfPlayer::State::Idle) {
        return;
    }
#endif
    const uint64_t now = time_us_64();
    bool timedOut = false;
    taskENTER_CRITICAL();
    if (gPerformanceActive.load(std::memory_order_relaxed) &&
        now - gLastActivityTimeUs >= kPlayTimeoutUs) {
        gPerformanceActive.store(false, std::memory_order_relaxed);
        timedOut = true;
    }
    taskEXIT_CRITICAL();
    if (timedOut) {
        HandleStop();
    }
}

}  // namespace

void InfoScreen::NotifyPlay(const char* title) {
    taskENTER_CRITICAL();
    gLastActivityTimeUs = time_us_64();
    bool notify = false;
    if (gInfoScreenTaskHandle == nullptr) {
        // InfoScreenTask起動前の通知は状態も含めて破棄する
    } else if (title == nullptr) {
        // 汎用の演奏活動パルス。既にPlaying中なら画面更新は不要
        if (!gPerformanceActive.load(std::memory_order_relaxed)) {
            gPerformanceActive.store(true, std::memory_order_relaxed);
            notify = true;
        }
    } else {
        // 曲名確定はゲートに関わらず常に伝える（NoteOnより後に判明する場合がある）
        gPerformanceActive.store(true, std::memory_order_relaxed);
        notify = true;
    }
    if (notify) {
        WriteEvent(ScreenEventKind::Play, title);
    }
    taskEXIT_CRITICAL();
    if (notify) {
        xTaskNotifyGive(gInfoScreenTaskHandle);
    }
}

void InfoScreen::NotifyTrackStart() {
    taskENTER_CRITICAL();
    gLastActivityTimeUs = time_us_64();
    taskEXIT_CRITICAL();
    PostEventWithGate(ScreenEventKind::TrackStart, nullptr, true);
}

void InfoScreen::NotifyStop() {
    // ゲート更新とメールボックス更新を同じクリティカル区間で行い、直後にUSB MIDI等の
    // Playが来た場合は、そのPlay通知がStopを上書きして最新状態を表示できるようにする。
    PostEventWithGate(ScreenEventKind::Stop, nullptr, false);
}

void InfoScreen::NotifyError(const char* message) {
    taskENTER_CRITICAL();
    gLastActivityTimeUs = time_us_64();
    taskEXIT_CRITICAL();
    PostEventWithGate(ScreenEventKind::Error, message, true);
}

void InfoScreenTask(void* param) {
    auto* ctx = static_cast<InfoScreenTaskContext*>(param);
    gInfoScreenTaskHandle = xTaskGetCurrentTaskHandle();

    CharacterDisplayRenderer renderer(&Platform::GetCharacterDisplay(),
                                       Platform::kDisplayColumns,
                                       Platform::kDisplayRows - Platform::LcdCharacterDisplayAdapter::kRowOffset);
    renderer.begin();

    LcdMenu menu(renderer);
    MenuScreen* const homeScreen = AppUi::BuildRootScreen(*ctx);
    menu.setScreen(homeScreen);
    AppUi::SetMenu(&menu);

    AppUi::JoystickInputAdapter joystick(&menu, ctx->panel);
    uint64_t lastSystemInfoRefreshUs = 0;
    uint32_t lastSeenResetPulseSeq = gResetPulseSeq;
    uint64_t resetDisplayUntilUs = 0;
    bool wasShowingReset = false;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(INFO_SCREEN_POLL_PERIOD_MS));

        bool hasEvent = false;
        ScreenEvent event;
        taskENTER_CRITICAL();
        if (gEventPending) {
            event = gEventMailbox;
            gEventPending = false;
            hasEvent = true;
        }
        taskEXIT_CRITICAL();

        if (hasEvent) {
            switch (event.kind) {
            case ScreenEventKind::Play:  HandlePlay(event.title); break;
            case ScreenEventKind::TrackStart: HandleTrackStart(); break;
            case ScreenEventKind::Stop:  HandleStop(); break;
            case ScreenEventKind::Error: HandleError(event.title); break;
            }
        }
        CheckTimeout();

        // ステータス表示（Play/Error）が出ていない間は、Home画面ならデフォルト表示、
        // それ以外の画面では空白にする。表示終了時のHome復帰も画面遷移も、この毎周期の
        // 更新だけで賄える（SetStatusLine()は20桁に空白埋めして差分のみ書くため残骸は残らない）。
        if (!gPerformanceActive.load(std::memory_order_acquire)) {
            SetStatusLine(menu.getScreen() == homeScreen ? kDefaultStatusText : "");
        }

        // MIDI Resetが実際に適用されたら、他のステータス表示より優先して一定時間表示する。
        // 裏では上のPlay/Stop/Error/デフォルト表示の更新が続いているため、表示時間が
        // 過ぎれば最新のステータス行にそのまま戻る。
        const uint64_t nowUs = time_us_64();
        const uint32_t resetPulseSeq = gResetPulseSeq;
        if (resetPulseSeq != lastSeenResetPulseSeq) {
            lastSeenResetPulseSeq = resetPulseSeq;
            resetDisplayUntilUs = nowUs + kResetDisplayUs;
        }
        const bool showingReset = nowUs < resetDisplayUntilUs;
        if (showingReset) {
            if (!wasShowingReset) {
                Platform::DisplayWrite(0, 0, kResetStatusLine);
            }
        } else {
            if (wasShowingReset) {
                gStatusDirty = true;  // 元のステータス行(gStatusShadow)を書き直す
            }
            if (gStatusDirty) {
                Platform::DisplayWrite(0, 0, gStatusShadow);
                gStatusDirty = false;
            }
        }
        wasShowingReset = showingReset;

        joystick.observe();
        AppUi::UpdatePlaybackUi();

        if (nowUs - lastSystemInfoRefreshUs >= INFO_SCREEN_SYSINFO_REFRESH_MS * 1000ULL) {
            lastSystemInfoRefreshUs = nowUs;
            AppUi::RefreshSystemInfo();
            if (menu.getScreen() == AppUi::GetSystemInfoScreen()) {
                menu.refresh();
            }
        }
        menu.poll();
    }
}
