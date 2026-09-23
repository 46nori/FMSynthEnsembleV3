//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "menu_screens.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include "ItemCommand.h"
#include "ItemLabel.h"
#include "ItemSubMenu.h"
#include "ItemToggle.h"
#include "LcdMenu.h"
#include "MenuScreen.h"

#include "MidiFactory.h"
#include "MidiPanelController.h"
#include "OpnBase.h"
#include "config.h"
#include "display.h"
#include "info_screen_task.h"
#include "MidiMessage.h"
#include "midi_ipc.h"
#include "volume_controller.h"
#include "volume_db_widget.h"

#if BUILD_SD_CARD
#include "smf_directory.h"
#include "smf_player_task.h"
#endif

namespace AppUi {

namespace {

MenuScreen* g_rootScreen = nullptr;
MenuScreen* g_playSmfScreen = nullptr;
MenuScreen* g_playlistScreen = nullptr;
MenuScreen* g_transportScreen = nullptr;
MenuScreen* g_systemInfoScreen = nullptr;
MenuScreen* g_playOptionsScreen = nullptr;
MenuScreen* g_settingsScreen = nullptr;
MenuScreen* g_volumeScreen = nullptr;

LcdMenu* g_menu = nullptr;

MidiPanelController* g_panel = nullptr;
const InfoScreenTaskContext* g_ctx = nullptr;

ItemLabel* g_voiceLabel = nullptr;
char g_voiceLine[Platform::kDisplayColumns + 1] = {};

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

// Settings > LED Mode。enabled(true)="Toggle"(Mode A) / enabled(false)="Note"(Mode B、既定)。
void OnLedModeToggled(bool toggle_selected) {
    if (g_panel != nullptr) {
        g_panel->SetLedMode(!toggle_selected);
    }
}

// --- Settings > Volume ---
// NJU72343の物理配線（chip/channel）とLCD表示名の対応。
struct VolumeChannelDef {
    uint8_t chip_idx;  // 0=CHIP_ADR0, 1=CHIP_ADR1
    uint8_t channel;   // 0=A, 1=B, ... 7=H
    const char* label;
};

// dock順(0-3)に並べ、dock内はFM-L/FM-R/SSGの順。最後にLineMix-L/R、LineSmp-L/Rを置く。
constexpr VolumeChannelDef kVolumeChannels[] = {
    {0, 2, "0-FM-L"}, {1, 2, "0-FM-R"}, {0, 0, "0-SSG "},
    {0, 4, "1-FM-L"}, {1, 4, "1-FM-R"}, {0, 1, "1-SSG "},
    {0, 3, "2-FM-L"}, {1, 3, "2-FM-R"}, {1, 0, "2-SSG "},
    {0, 5, "3-FM-L"}, {1, 5, "3-FM-R"}, {1, 1, "3-SSG "},
    {0, 6, "LineMix-L"}, {1, 6, "LineMix-R"}, {0, 7, "LineSmp-L"}, {1, 7, "LineSmp-R"},
};
constexpr std::size_t kVolumeChannelCount = sizeof(kVolumeChannels) / sizeof(kVolumeChannels[0]);
std::array<VolumeDbWidget*, kVolumeChannelCount> g_volumeWidgets{};

// VolumeDbWidgetのonChangeコールバック。UP/DOWNのたびに
// 呼ばれ、NJU72343へ即座に書き込む（リアルタイム反映）。ItemCommandのPlaySmfFileAt<Index>
// と同じ理由（引数を取らない/型が固定の関数ポインタしか渡せない）で、チャンネルごとに
// 個別の関数をテンプレートで機械的に生成する。
template <std::size_t Index>
void OnVolumeChanged(const int16_t& value) {
    const auto& def = kVolumeChannels[Index];
    const uint8_t chip_addr = Platform::VolumeController::kChipAddr[def.chip_idx];
    auto& vc = Platform::VolumeController::GetInstance();
    if (value == VolumeDbWidget::kMuteValue) {
        vc.SetChannelMute(chip_addr, def.channel);
    } else {
        vc.SetChannelVolumeDb(chip_addr, def.channel, value / 2.0f);
    }
}

template <std::size_t... Is>
constexpr std::array<void (*)(const int16_t&), sizeof...(Is)> MakeVolumeChangeCallbacks(std::index_sequence<Is...>) {
    return {&OnVolumeChanged<Is>...};
}

constexpr auto kVolumeChangeCallbacks =
    MakeVolumeChangeCallbacks(std::make_index_sequence<kVolumeChannelCount>{});

// --- Settings > RhythmVol ---
RhythmLevelWidget* g_rhythmWidget = nullptr;

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

#if BUILD_SD_CARD
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

MenuItem* g_pauseItem = nullptr;
MenuItem* g_repeatItem = nullptr;
MenuItem* g_shuffleItem = nullptr;
MenuItem* g_playmodeItem = nullptr;

constexpr const char* kPauseLabel  = "Pause";
constexpr const char* kResumeLabel = "Resume";
constexpr const char* kRepeatLabels[]   = {"Repeat:   Off",    "Repeat:   1", "Repeat:   Loop"};
constexpr const char* kShuffleLabels[]  = {"Shuffle:  Off",    "Shuffle:  On"};
constexpr const char* kPlaymodeLabels[] = {"Playback: Single", "Playback: Cont."};

// UIが持つRepeat/Shuffle/PlaybackModeの現在値。SmfPlayerTaskが正で、UpdatePlaybackUi()が同期する
RepeatMode g_repeat = RepeatMode::Off;
bool g_shuffle = false;
PlaybackMode g_playmode = PlaybackMode::Single;

void OpenTransport(MenuScreen* from) {
    g_transportParent = from;
    g_transportScreen->setParent(from);
    g_transportSawActive = false;
    g_transportOpenedMs = millis();
    g_menu->setScreen(g_transportScreen);
}

// Transport画面から元の一覧へ戻る（BACKと同じく、一覧のカーソル位置を復元する）
void CloseTransport() {
    if (g_transportParent == nullptr) {
        return;
    }
    const uint8_t cursor = g_transportParent->getCursor();
    g_menu->setScreen(g_transportParent);
    g_menu->setCursor(cursor);
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

// Transport画面の項目
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

// Settingsの項目。押すたびに値を進め、SmfPlayerTaskへ送る
void OnRepeatCycle() {
    g_repeat = static_cast<RepeatMode>((static_cast<uint8_t>(g_repeat) + 1) % 3);
    g_repeatItem->setText(kRepeatLabels[static_cast<uint8_t>(g_repeat)]);
    SmfPlayer::RequestSetRepeat(g_repeat);
    g_menu->refresh();
}

void OnShuffleToggle() {
    g_shuffle = !g_shuffle;
    g_shuffleItem->setText(kShuffleLabels[g_shuffle ? 1 : 0]);
    SmfPlayer::RequestSetShuffle(g_shuffle);
    g_menu->refresh();
}

void OnPlaymodeCycle() {
    g_playmode = (g_playmode == PlaybackMode::Single) ? PlaybackMode::Continuous : PlaybackMode::Single;
    g_playmodeItem->setText(kPlaymodeLabels[static_cast<uint8_t>(g_playmode)]);
    SmfPlayer::RequestSetPlaybackMode(g_playmode);
    g_menu->refresh();
}

// --- Play Options > Tempo / Transport > Tempo ---
// テンポ倍率を5%刻みで表示・編集するWidget。値は倍率を5で割った段数で保持する
// （RealtimeLevelWidgetのstepは1固定のため）。Play Optionsの既定倍率に使い、表示は倍率のみ。
// 倍率の値はPlay Optionsの他の行（`Repeat:   Off`等）と桁をそろえて右詰めにする。
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

// Transport画面のTempo行。再生中の曲の倍率を、倍率適用後のBPMと並べて表示する
// （例: `132bpm 110%`）。BPMは曲中のテンポ変更に追従するため、UpdatePlaybackUi()が
// setBpm()で外から与える。
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

TempoPercentWidget* g_defaultTempoWidget = nullptr;
TempoScaleWidget* g_tempoWidget = nullptr;
uint16_t g_tempoTrackSerial = 0;  // UIが倍率を同期した曲の通し番号

// Play Optionsの既定倍率。次に開始する曲から効き、再生中の曲の倍率は変えない
void OnDefaultTempoScaleChanged(const int16_t& value) {
    SmfPlayer::RequestSetDefaultTempoScale(TempoPercentWidget::PercentOf(value));
}

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

void OnNowPlaying() {
    if (SmfPlayer::GetStatus().state != SmfPlayer::State::Idle) {
        OpenTransport(g_rootScreen);
    }
}
#endif

}  // namespace

MenuScreen* BuildRootScreen(const InfoScreenTaskContext& ctx) {
    g_ctx = &ctx;
    g_panel = ctx.panel;

    // --- Play Options（再生制御: Repeat / Shuffle / Playback Mode） ---
#if BUILD_SD_CARD
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
#endif

    // --- Settings（機器設定: LED Mode / RhythmVol / Volume / System Info） ---
    std::vector<MenuItem*> settingsItems;
    settingsItems.push_back(new ItemToggle("LEDmode", "CH-Toggle", "Note", &OnLedModeToggled));

    // --- RhythmVol（デバッガのrmixと同じg_rhythm_level_offsetを0.75dB単位で調整） ---
    {
        const bool available = HasRhythmModule(*ctx.modules);
        const int16_t initial =
            available ? RhythmLevelWidgetValue() : RhythmLevelWidget::kUnavailableValue;
        g_rhythmWidget = new RhythmLevelWidget(initial, &OnRhythmLevelChanged);
        settingsItems.push_back(new VolumeItem("RhythmVol", g_rhythmWidget, available));
    }

    // --- Volume（NJU72343全16CHを個別に0.5dB単位で調整） ---
    {
        auto& vc = Platform::VolumeController::GetInstance();
        std::vector<MenuItem*> volumeItems;
        volumeItems.reserve(kVolumeChannelCount);
        for (std::size_t i = 0; i < kVolumeChannelCount; ++i) {
            const auto& def = kVolumeChannels[i];
            const uint8_t chip_addr = Platform::VolumeController::kChipAddr[def.chip_idx];
            const bool available = vc.IsChannelAvailable(chip_addr, def.channel);
            int16_t initial = VolumeDbWidget::kUnavailableValue;
            if (available) {
                const auto shadow = vc.GetChannelVolume(chip_addr, def.channel);
                initial = shadow.muted ? VolumeDbWidget::kMuteValue : shadow.db_x2;
            }
            auto* widget = new VolumeDbWidget(initial, kVolumeChangeCallbacks[i]);
            g_volumeWidgets[i] = widget;
            volumeItems.push_back(new VolumeItem(
                def.label, widget, available));
        }
        g_volumeScreen = new MenuScreen(volumeItems);
    }
    settingsItems.push_back(ITEM_SUBMENU("Volume", g_volumeScreen));

    // --- System Info（Dock構成は起動時固定、Voice/CSM数のみ後で更新する） ---
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
    RefreshSystemInfo();

    g_systemInfoScreen = new MenuScreen(std::vector<MenuItem*>{
        new ItemLabel(dockLine1),
        new ItemLabel(dockLine2),
        g_voiceLabel,
    });

    // --- Settings（System Infoは読み取り専用のためここにぶら下げる） ---
    settingsItems.push_back(ITEM_SUBMENU("System Info", g_systemInfoScreen));
    g_settingsScreen = new MenuScreen(settingsItems);

    // --- Play SMF / Playlist ---
    // ファイルごとに1行のItemCommandとして並べる。ラベル=ファイル名のみのため
    // ItemList（ラベル+値を1行で共有する方式）よりファイル名の表示幅が広く取れる。
    // PUSHで再生を始め、Transport画面へ移る。範囲は、選んだ一覧（Play SMF=全曲、
    // Playlist=playlistフォルダ内）で決まる。
#if BUILD_SD_CARD
    SmfListContext smfCtx;
    smfCtx.max = kMaxSmfFiles;
    smfCtx.names = g_smfFileNames;
    Platform::ForEachSmfFile(&CollectSmfFile, &smfCtx);

    std::vector<MenuItem*> playSmfItems;
    for (int i = 0; i < smfCtx.count; ++i) {
        playSmfItems.push_back(ITEM_COMMAND(g_smfFileNames[i], kPlaySmfCallbacks[i]));
    }
    if (smfCtx.count == 0) {
        playSmfItems.push_back(new ItemLabel("(no files)"));
    }
    g_playSmfScreen = new MenuScreen(playSmfItems);

    SmfListContext playlistCtx;
    playlistCtx.max = kMaxPlaylistFiles;
    playlistCtx.names = g_playlistFileNames;
    Platform::ForEachPlaylistFile(&CollectSmfFile, &playlistCtx);

    std::vector<MenuItem*> playlistItems;
    for (int i = 0; i < playlistCtx.count; ++i) {
        playlistItems.push_back(ITEM_COMMAND(g_playlistFileNames[i], kPlaylistCallbacks[i]));
    }
    if (playlistCtx.count == 0) {
        playlistItems.push_back(new ItemLabel("(no files)"));
    }
    g_playlistScreen = new MenuScreen(playlistItems);

    // --- Transport ---
    g_pauseItem = ITEM_COMMAND(kPauseLabel, &OnPauseResume);
    g_transportScreen = new MenuScreen(std::vector<MenuItem*>{
        g_pauseItem,
        ITEM_COMMAND("Stop", &OnStop),
        ITEM_COMMAND("Next", &OnNext),
        ITEM_COMMAND("Prev", &OnPrev),
        new VolumeItem("Tempo", g_tempoWidget = new TempoScaleWidget(&OnTempoScaleChanged), true),
    });
#else
    g_playSmfScreen = new MenuScreen(std::vector<MenuItem*>{
        new ItemLabel("SD card disabled"),
    });
    g_playlistScreen = new MenuScreen(std::vector<MenuItem*>{
        new ItemLabel("SD card disabled"),
    });
#endif

    // --- Home ---
    g_rootScreen = new MenuScreen(std::vector<MenuItem*>{
#if BUILD_SD_CARD
        ITEM_COMMAND("Now Playing", &OnNowPlaying),
#endif
        ITEM_SUBMENU("Play SMF", g_playSmfScreen),
        ITEM_SUBMENU("Playlist", g_playlistScreen),
#if BUILD_SD_CARD
        ITEM_SUBMENU("Play Options", g_playOptionsScreen),
#endif
        ITEM_SUBMENU("Settings", g_settingsScreen),
    });
    return g_rootScreen;
}

void RefreshSystemInfo() {
    if (g_ctx == nullptr) {
        return;
    }
    const int active = g_ctx->factory->GetActiveNoteVoiceCount();
    const int csmReserved = g_ctx->factory->GetCsmReservedVoiceCount();
    const int total = active + csmReserved;
    std::snprintf(g_voiceLine, sizeof(g_voiceLine), "Voice:%02d/%02d CSM:%02d",
                  active, total, csmReserved);
    if (g_voiceLabel != nullptr) {
        g_voiceLabel->setText(g_voiceLine);
    }
}

MenuScreen* GetSystemInfoScreen() {
    return g_systemInfoScreen;
}

void SetMenu(LcdMenu* menu) {
    g_menu = menu;
}

void RefreshVolumeUi() {
    if (g_menu == nullptr || MenuItem::isEditing()) {
        return;
    }

    // Settings画面のRhythmVol行。リズム音源が無い構成（N/A固定）は同期しない
    if (g_menu->getScreen() == g_settingsScreen) {
        if (g_rhythmWidget != nullptr &&
            g_rhythmWidget->getValue() != RhythmLevelWidget::kUnavailableValue &&
            g_rhythmWidget->syncValue(RhythmLevelWidgetValue())) {
            g_menu->refresh();
        }
        return;
    }
    if (g_menu->getScreen() != g_volumeScreen) {
        return;
    }

    auto& vc = Platform::VolumeController::GetInstance();
    bool redraw = false;
    for (std::size_t i = 0; i < kVolumeChannelCount; ++i) {
        const auto& def = kVolumeChannels[i];
        const uint8_t chip_addr = Platform::VolumeController::kChipAddr[def.chip_idx];
        int16_t value = VolumeDbWidget::kUnavailableValue;
        if (vc.IsChannelAvailable(chip_addr, def.channel)) {
            const auto shadow = vc.GetChannelVolume(chip_addr, def.channel);
            value = shadow.muted ? VolumeDbWidget::kMuteValue : shadow.db_x2;
        }
        if (g_volumeWidgets[i] != nullptr) {
            redraw = g_volumeWidgets[i]->syncValue(value) || redraw;
        }
    }
    if (redraw) {
        g_menu->refresh();
    }
}

void UpdatePlaybackUi() {
#if BUILD_SD_CARD
    if (g_menu == nullptr) {
        return;
    }
    const SmfPlayer::Status status = SmfPlayer::GetStatus();
    MenuScreen* const current = g_menu->getScreen();
    bool redraw = false;

    // Pause/Resumeのラベルを状態に合わせる
    const char* pauseLabel =
        (status.state == SmfPlayer::State::Paused) ? kResumeLabel : kPauseLabel;
    if (std::strcmp(g_pauseItem->getText(), pauseLabel) != 0) {
        g_pauseItem->setText(pauseLabel);
        redraw = redraw || (current == g_transportScreen);
    }

    // Repeat/Shuffleのラベルを、SmfPlayerTaskが持つ現在値に合わせる（デバッガ等の変更も反映）
    if (status.repeat != g_repeat) {
        g_repeat = status.repeat;
        g_repeatItem->setText(kRepeatLabels[static_cast<uint8_t>(g_repeat)]);
        redraw = redraw || (current == g_playOptionsScreen);
    }
    if (status.shuffle != g_shuffle) {
        g_shuffle = status.shuffle;
        g_shuffleItem->setText(kShuffleLabels[g_shuffle ? 1 : 0]);
        redraw = redraw || (current == g_playOptionsScreen);
    }
    if (status.playback_mode != g_playmode) {
        g_playmode = status.playback_mode;
        g_playmodeItem->setText(kPlaymodeLabels[static_cast<uint8_t>(g_playmode)]);
        redraw = redraw || (current == g_playOptionsScreen);
    }
    // Play OptionsのTempo行（既定倍率）。編集中でなければ、コマンドの取りこぼしに備えて合わせる
    if (!MenuItem::isEditing() &&
        g_defaultTempoWidget->syncValue(TempoPercentWidget::ValueOf(status.default_tempo_scale_percent))) {
        redraw = redraw || (current == g_playOptionsScreen);
    }

    // Transport画面のTempo行。倍率はUIが正だが、曲の開始でSmfPlayerTaskが既定倍率を読み込むため、
    // 曲が変わったら編集中でも合わせる。編集中でなければ、コマンドの取りこぼしに備えて常に合わせる
    const int16_t scaleValue = TempoPercentWidget::ValueOf(status.tempo_scale_percent);
    if (status.track_serial != g_tempoTrackSerial || !MenuItem::isEditing()) {
        g_tempoTrackSerial = status.track_serial;
        if (g_tempoWidget->syncValue(scaleValue)) {
            redraw = redraw || (current == g_transportScreen);
        }
    }
    if (g_tempoWidget->setBpm(EffectiveBpm(status.tempo_us_per_qn, status.tempo_scale_percent))) {
        redraw = redraw || (current == g_transportScreen);
    }

    if (redraw) {
        g_menu->refresh();
    }

    // Transport画面は、再生が終わった（セッション終了・全曲再生不能）ら一覧へ戻る
    if (current == g_transportScreen) {
        if (status.state != SmfPlayer::State::Idle) {
            g_transportSawActive = true;
        } else if (g_transportSawActive ||
                   millis() - g_transportOpenedMs >= kTransportStartGraceMs) {
            CloseTransport();
        }
    }
#endif
}

}  // namespace AppUi
