//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "screen_smf.h"

#include <Arduino.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include "ItemCommand.h"
#include "ItemLabel.h"
#include "LcdMenu.h"
#include "MenuScreen.h"

#include "config.h"
#include "display.h"
#include "menu_context.h"
#include "smf_directory.h"
#include "smf_player_task.h"
#include "tempo_widgets.h"
#include "level_widgets.h"

namespace AppUi {

namespace {

MenuScreen* g_playSmfScreen = nullptr;
MenuScreen* g_playlistScreen = nullptr;
MenuScreen* g_playOptionsScreen = nullptr;
MenuScreen* g_transportScreen = nullptr;

// ---------------------------------------------------------------------------
// Play SMF / Playlist（ファイル一覧）
// ---------------------------------------------------------------------------

// 表示上限はconfig.hのMENU_MAX_SMF_FILES。LcdMenuの項目位置はuint8_tで1画面の項目数が
// 最大256のため。境界の余裕として255件を上限にしている。
constexpr int kMaxSmfFiles = MENU_MAX_SMF_FILES;
static_assert(kMaxSmfFiles >= 1 && kMaxSmfFiles <= 255,
              "MENU_MAX_SMF_FILES must be in 1..255 (LcdMenu item index is uint8_t)");
constexpr int kMaxPlaylistFiles = Platform::kPlaylistMaxFiles;
constexpr int kSmfNameLen = Platform::kDisplayColumns;
char g_smfFileNames[kMaxSmfFiles][kSmfNameLen + 1];
char g_playlistFileNames[kMaxPlaylistFiles][kSmfNameLen + 1];

struct SmfListContext {
    int count = 0;
    int max = 0;
    char (*names)[kSmfNameLen + 1] = nullptr;
};

bool CollectSmfFile(void* context, const char* path) {
    auto* ctx = static_cast<SmfListContext*>(context);
    if (ctx->count >= ctx->max) {
        return false;  // 走査を打ち切る
    }
    // pathはSDボリューム上のフルパス。20桁の画面に収まらずLcdMenuの横スクロールが
    // 常に必要になるため、ディレクトリ部分を除いたファイル名だけを表示する。
    const char* slash = std::strrchr(path, '/');
    const char* name = (slash != nullptr) ? slash + 1 : path;
    std::snprintf(ctx->names[ctx->count], kSmfNameLen + 1, "%s", name);
    ++ctx->count;
    return true;
}

// ---------------------------------------------------------------------------
// Transport画面と一覧の連携
// ---------------------------------------------------------------------------

// Transport画面へ移った直後、再生状態が公開されるまでの猶予。これを過ぎても再生が
// 始まらなければ（全曲が再生不能など）一覧へ戻る
constexpr uint32_t kTransportStartGraceMs = 5000;

MenuScreen* g_transportParent = nullptr;  // Transport画面から戻る先（曲を選んだ一覧）
bool g_transportSawActive = false;        // Transport画面を開いてから、再生中の状態を観測したか
uint32_t g_transportOpenedMs = 0;

void OpenTransport(MenuScreen* from) {
    g_transportParent = from;
    g_transportScreen->setParent(from);
    g_transportSawActive = false;
    g_transportOpenedMs = millis();
    CurrentMenu()->setScreen(g_transportScreen);
}

// Transport画面から元の一覧へ戻る（BACKと同じく、一覧のカーソル位置を復元する）
void CloseTransport() {
    if (g_transportParent == nullptr) {
        return;
    }
    const uint8_t cursor = g_transportParent->getCursor();
    CurrentMenu()->setScreen(g_transportParent);
    CurrentMenu()->setCursor(cursor);
}

// 曲をPUSHしたときの動作。再生中の曲（同じ範囲・同じ位置）ならそのままTransport画面へ、
// 別の曲なら新しいセッションを開始してTransport画面へ移る
void StartOrOpenTransport(SmfPlayer::Scope scope, uint16_t position, MenuScreen* from) {
    const SmfPlayer::Status status = SmfPlayer::GetStatus();
    const bool same_track = status.state != SmfPlayer::State::Idle && status.scope == scope &&
                            status.position == position;
    if (!same_track) {
        if (scope == SmfPlayer::Scope::All) {
            SmfPlayer::RequestPlay(position);
        } else {
            SmfPlayer::RequestPlayPlaylist(position);
        }
    }
    OpenTransport(from);
}

// ItemCommandのコールバックは引数を取らない関数ポインタ(void(*)())なので、ファイルの
// 位置(Index)ごとに個別の関数をテンプレートで機械的に生成し、コンパイル時に
// 関数ポインタ表を作る

template <std::size_t Index>
void PlaySmfFileAt() {
    StartOrOpenTransport(SmfPlayer::Scope::All, static_cast<uint16_t>(Index + 1), g_playSmfScreen);
}

template <std::size_t... Is>
constexpr std::array<void (*)(), sizeof...(Is)> MakePlaySmfCallbacks(std::index_sequence<Is...>) {
    return {&PlaySmfFileAt<Is>...};
}

constexpr auto kPlaySmfCallbacks =
    MakePlaySmfCallbacks(std::make_index_sequence<static_cast<std::size_t>(kMaxSmfFiles)>{});

template <std::size_t Index>
void PlayPlaylistFileAt() {
    StartOrOpenTransport(SmfPlayer::Scope::Playlist, static_cast<uint16_t>(Index + 1),
                         g_playlistScreen);
}

template <std::size_t... Is>
constexpr std::array<void (*)(), sizeof...(Is)> MakePlaylistCallbacks(std::index_sequence<Is...>) {
    return {&PlayPlaylistFileAt<Is>...};
}

constexpr auto kPlaylistCallbacks =
    MakePlaylistCallbacks(std::make_index_sequence<static_cast<std::size_t>(kMaxPlaylistFiles)>{});

// 列挙済みのファイル名から、ファイルごとに1行のItemCommandを並べた一覧を作る。
// ラベル=ファイル名のみのため、ItemList（ラベル+値を1行で共有する方式）よりファイル名の
// 表示幅が広く取れる。
template <std::size_t N>
MenuScreen* BuildFileListScreen(char (*names)[kSmfNameLen + 1], int count,
                                const std::array<void (*)(), N>& callbacks) {
    std::vector<MenuItem*> items;
    items.reserve(count > 0 ? count : 1);
    for (int i = 0; i < count; ++i) {
        items.push_back(ITEM_COMMAND(names[i], callbacks[i]));
    }
    if (count == 0) {
        items.push_back(new ItemLabel("(no files)"));
    }
    return new MenuScreen(items);
}

// ---------------------------------------------------------------------------
// Transport画面の項目
// ---------------------------------------------------------------------------

MenuItem* g_pauseItem = nullptr;
constexpr const char* kPauseLabel  = "Pause";
constexpr const char* kResumeLabel = "Resume";

void OnPauseResume() {
    if (SmfPlayer::GetStatus().state == SmfPlayer::State::Paused) {
        SmfPlayer::RequestResume();
    } else {
        SmfPlayer::RequestPause();
    }
}

void OnStop() {
    SmfPlayer::RequestStop();
    CloseTransport();
}

void OnNext() { SmfPlayer::RequestNext(); }
void OnPrev() { SmfPlayer::RequestPrev(); }

TempoScaleWidget* g_tempoWidget = nullptr;
uint16_t g_tempoTrackSerial = 0;  // UIが倍率を同期した曲の通し番号

void OnTempoScaleChanged(const int16_t& value) {
    SmfPlayer::RequestSetTempoScale(TempoPercentWidget::PercentOf(value));
}

// 曲のテンポ（µs/四分音符）と倍率から、表示用のBPM（四捨五入）を求める
uint16_t EffectiveBpm(uint32_t tempo_us_per_qn, uint16_t scale_percent) {
    if (tempo_us_per_qn == 0) {
        return 0;
    }
    // BPM = 60e6 / tempo × scale / 100。四捨五入のため1000倍して計算する
    const uint64_t numerator = 600000000ULL * scale_percent;
    return static_cast<uint16_t>((numerator / tempo_us_per_qn + 500) / 1000);
}

// ---------------------------------------------------------------------------
// Play Options画面の項目
// ---------------------------------------------------------------------------

MenuItem* g_repeatItem = nullptr;
MenuItem* g_shuffleItem = nullptr;
MenuItem* g_playmodeItem = nullptr;

constexpr const char* kRepeatLabels[]   = {"Repeat:   Off",    "Repeat:   1", "Repeat:   Loop"};
constexpr const char* kShuffleLabels[]  = {"Shuffle:  Off",    "Shuffle:  On"};
constexpr const char* kPlaymodeLabels[] = {"Playback: Single", "Playback: Cont."};

// UIが持つRepeat/Shuffle/PlaybackModeの現在値。SmfPlayerTaskが正で、SyncSmf()が同期する
RepeatMode g_repeat = RepeatMode::Off;
bool g_shuffle = false;
PlaybackMode g_playmode = PlaybackMode::Single;

// 押すたびに値を進め、SmfPlayerTaskへ送る
void OnRepeatCycle() {
    g_repeat = static_cast<RepeatMode>((static_cast<uint8_t>(g_repeat) + 1) % 3);
    g_repeatItem->setText(kRepeatLabels[static_cast<uint8_t>(g_repeat)]);
    SmfPlayer::RequestSetRepeat(g_repeat);
    CurrentMenu()->refresh();
}

void OnShuffleToggle() {
    g_shuffle = !g_shuffle;
    g_shuffleItem->setText(kShuffleLabels[g_shuffle ? 1 : 0]);
    SmfPlayer::RequestSetShuffle(g_shuffle);
    CurrentMenu()->refresh();
}

void OnPlaymodeCycle() {
    g_playmode = (g_playmode == PlaybackMode::Single) ? PlaybackMode::Continuous : PlaybackMode::Single;
    g_playmodeItem->setText(kPlaymodeLabels[static_cast<uint8_t>(g_playmode)]);
    SmfPlayer::RequestSetPlaybackMode(g_playmode);
    CurrentMenu()->refresh();
}

TempoPercentWidget* g_defaultTempoWidget = nullptr;

// Play Optionsの既定倍率。次に開始する曲から効き、再生中の曲の倍率は変えない
void OnDefaultTempoScaleChanged(const int16_t& value) {
    SmfPlayer::RequestSetDefaultTempoScale(TempoPercentWidget::PercentOf(value));
}

}  // namespace

SmfScreens BuildSmfScreens() {
    // --- Play SMF / Playlist ---
    // PUSHで再生を始め、Transport画面へ移る。範囲は、選んだ一覧（Play SMF=全曲、
    // Playlist=playlistフォルダ内）で決まる。
    SmfListContext smfCtx;
    smfCtx.max = kMaxSmfFiles;
    smfCtx.names = g_smfFileNames;
    Platform::ForEachSmfFile(&CollectSmfFile, &smfCtx);
    g_playSmfScreen = BuildFileListScreen(g_smfFileNames, smfCtx.count, kPlaySmfCallbacks);

    SmfListContext playlistCtx;
    playlistCtx.max = kMaxPlaylistFiles;
    playlistCtx.names = g_playlistFileNames;
    Platform::ForEachPlaylistFile(&CollectSmfFile, &playlistCtx);
    g_playlistScreen =
        BuildFileListScreen(g_playlistFileNames, playlistCtx.count, kPlaylistCallbacks);

    // --- Play Options（再生制御: Repeat / Shuffle / Playback Mode / 既定テンポ倍率） ---
    g_repeatItem = ITEM_COMMAND(kRepeatLabels[0], &OnRepeatCycle);
    g_shuffleItem = ITEM_COMMAND(kShuffleLabels[0], &OnShuffleToggle);
    g_playmodeItem = ITEM_COMMAND(kPlaymodeLabels[0], &OnPlaymodeCycle);
    g_defaultTempoWidget = new TempoPercentWidget(&OnDefaultTempoScaleChanged);
    g_playOptionsScreen = new MenuScreen(std::vector<MenuItem*>{
        g_repeatItem,
        g_shuffleItem,
        g_playmodeItem,
        new VolumeItem("Tempo", g_defaultTempoWidget, true),
    });

    // --- Transport ---
    g_pauseItem = ITEM_COMMAND(kPauseLabel, &OnPauseResume);
    g_tempoWidget = new TempoScaleWidget(&OnTempoScaleChanged);
    g_transportScreen = new MenuScreen(std::vector<MenuItem*>{
        g_pauseItem,
        ITEM_COMMAND("Stop", &OnStop),
        ITEM_COMMAND("Next", &OnNext),
        ITEM_COMMAND("Prev", &OnPrev),
        new VolumeItem("Tempo", g_tempoWidget, true),
    });

    return SmfScreens{g_playSmfScreen, g_playlistScreen, g_playOptionsScreen};
}

void OpenNowPlaying(MenuScreen* from) {
    if (SmfPlayer::GetStatus().state != SmfPlayer::State::Idle) {
        OpenTransport(from);
    }
}

void SyncSmf() {
    if (g_transportScreen == nullptr) {
        return;
    }
    const SmfPlayer::Status status = SmfPlayer::GetStatus();
    bool transportDirty = false;
    bool optionsDirty = false;

    // Pause/Resumeのラベルを状態に合わせる
    const char* pauseLabel =
        (status.state == SmfPlayer::State::Paused) ? kResumeLabel : kPauseLabel;
    if (std::strcmp(g_pauseItem->getText(), pauseLabel) != 0) {
        g_pauseItem->setText(pauseLabel);
        transportDirty = true;
    }

    // Repeat/Shuffleのラベルを、SmfPlayerTaskが持つ現在値に合わせる（デバッガ等の変更も反映）
    if (status.repeat != g_repeat) {
        g_repeat = status.repeat;
        g_repeatItem->setText(kRepeatLabels[static_cast<uint8_t>(g_repeat)]);
        optionsDirty = true;
    }
    if (status.shuffle != g_shuffle) {
        g_shuffle = status.shuffle;
        g_shuffleItem->setText(kShuffleLabels[g_shuffle ? 1 : 0]);
        optionsDirty = true;
    }
    if (status.playback_mode != g_playmode) {
        g_playmode = status.playback_mode;
        g_playmodeItem->setText(kPlaymodeLabels[static_cast<uint8_t>(g_playmode)]);
        optionsDirty = true;
    }
    // Play OptionsのTempo行（既定倍率）。編集中でなければ、コマンドの取りこぼしに備えて合わせる
    if (!MenuItem::isEditing() &&
        g_defaultTempoWidget->syncValue(TempoPercentWidget::ValueOf(status.default_tempo_scale_percent))) {
        optionsDirty = true;
    }

    // Transport画面のTempo行。倍率はUIが正だが、曲の開始でSmfPlayerTaskが既定倍率を読み込むため、
    // 曲が変わったら編集中でも合わせる。編集中でなければ、コマンドの取りこぼしに備えて常に合わせる
    const int16_t scaleValue = TempoPercentWidget::ValueOf(status.tempo_scale_percent);
    if (status.track_serial != g_tempoTrackSerial || !MenuItem::isEditing()) {
        g_tempoTrackSerial = status.track_serial;
        if (g_tempoWidget->syncValue(scaleValue)) {
            transportDirty = true;
        }
    }
    if (g_tempoWidget->setBpm(EffectiveBpm(status.tempo_us_per_qn, status.tempo_scale_percent))) {
        transportDirty = true;
    }

    if (transportDirty) {
        RefreshIfShowing(g_transportScreen);
    }
    if (optionsDirty) {
        RefreshIfShowing(g_playOptionsScreen);
    }

    // Transport画面は、再生が終わった（セッション終了・全曲再生不能）ら一覧へ戻る。
    // Tempo行の編集中は戻らない。setScreen()は編集を終わらせないため、LcdMenuの編集フラグが残ってしまう。
    // 編集を抜けた後の周期で戻る
    if (IsShowing(g_transportScreen)) {
        if (status.state != SmfPlayer::State::Idle) {
            g_transportSawActive = true;
        } else if (!MenuItem::isEditing() &&
                   (g_transportSawActive ||
                    millis() - g_transportOpenedMs >= kTransportStartGraceMs)) {
            CloseTransport();
        }
    }
}

}  // namespace AppUi
