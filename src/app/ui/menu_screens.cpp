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
    g_playOptionsScreen = new MenuScreen(std::vector<MenuItem*>{
        g_repeatItem,
        g_shuffleItem,
        g_playmodeItem,
    });
#endif

    // --- Settings（機器設定: LED Mode / System Info） ---
    std::vector<MenuItem*> settingsItems;
    settingsItems.push_back(new ItemToggle("LEDmode", "CH-Toggle", "Note", &OnLedModeToggled));

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
