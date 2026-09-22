//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "smf_player_task.h"

#include <cstdio>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "pico/time.h"

#include "midi_ipc.h"
#include "MidiStreamAssembler.h"
#include "SmfParser.h"
#include "SmfMemoryByteSource.h"
#include "smf_sd_byte_source.h"
#include "smf_directory.h"
#include "init.h"
#include "config.h"
#include "PlaybackSequence.h"
#include "fixtures/smf_test_fixtures.h"

#if BUILD_I2C_DISPLAY
#include "info_screen_task.h"
#endif

namespace {

// 手持ちファイルの実測最大値(21)に余裕を見た値
constexpr uint8_t kMaxSmfTracks = 25;

// ---------------------------------------------------------------------------
// Debugger <-> SmfPlayerTask コマンド
// ---------------------------------------------------------------------------

enum class SmfCommand : uint8_t {
    Play, PlayPlaylist, Stop, Pause, Resume, Next, Prev, SetRepeat, SetShuffle, SetPlaybackMode, Ls, Mount
};

struct SmfCommandMessage {
    SmfCommand type;
    uint16_t   arg;  // Play/PlayPlaylist: 位置、SetRepeat: RepeatMode、SetShuffle: 0/1
};

// コマンドは固定長のリングバッファに積む。SmfPlayerTaskは通知で起床したとき、
// 積まれているコマンドをすべて順に処理する。満杯のときは新しいコマンドを捨てる。
constexpr uint8_t kCommandQueueDepth = 8;

TaskHandle_t gSmfPlayerTaskHandle = nullptr;
SmfCommandMessage gCommandQueue[kCommandQueueDepth];
uint8_t gCommandHead = 0;
uint8_t gCommandCount = 0;

void SendCommand(SmfCommand type, uint16_t arg) {
    if (gSmfPlayerTaskHandle == nullptr) {
        return;  // SmfPlayerTask起動前は無視する
    }
    taskENTER_CRITICAL();
    if (gCommandCount < kCommandQueueDepth) {
        const uint8_t tail = static_cast<uint8_t>((gCommandHead + gCommandCount) % kCommandQueueDepth);
        gCommandQueue[tail] = {type, arg};
        ++gCommandCount;
    }
    taskEXIT_CRITICAL();
    xTaskNotifyGive(gSmfPlayerTaskHandle);
}

bool PopCommand(SmfCommandMessage* out) {
    bool popped = false;
    taskENTER_CRITICAL();
    if (gCommandCount > 0) {
        *out = gCommandQueue[gCommandHead];
        gCommandHead = static_cast<uint8_t>((gCommandHead + 1) % kCommandQueueDepth);
        --gCommandCount;
        popped = true;
    }
    taskEXIT_CRITICAL();
    return popped;
}

// 再生状態のスナップショット。SmfPlayerTaskだけが書き、GetStatus()がコピーを返す
SmfPlayer::Status gStatus;

// ---------------------------------------------------------------------------
// 実機組み込みフィクスチャ
// ---------------------------------------------------------------------------
// SDカードやファイル準備なしで実機上の全経路を確認するためのテストベンチ。
// バイト列本体は src/smf/fixtures/smf_test_fixtures.h に置く。

struct SmfFixture {
    const char*    name;
    const uint8_t* data;
    uint32_t       length;
};

constexpr SmfFixture kFixtures[] = {
    {"test_scale", kTestScaleBytes, sizeof(kTestScaleBytes)},
    {"test_format1_multitrack", kTestFormat1MultitrackBytes, sizeof(kTestFormat1MultitrackBytes)},
    {"test_vibrato", kTestVibratoBytes, sizeof(kTestVibratoBytes)},
    {"test_full_polyphony", kTestFullPolyphonyBytes, sizeof(kTestFullPolyphonyBytes)},
};
constexpr uint16_t kFixtureCount = sizeof(kFixtures) / sizeof(kFixtures[0]);

// ---------------------------------------------------------------------------
// gMidiQueueへの投入
// ---------------------------------------------------------------------------

class SmfPlayerStreamSink : public IMidiStreamSink {
public:
    void OnMidiEvent(const MidiEvent& event) override {
        MidiEvent evt = event;
        evt.timestamp_us = static_cast<uint32_t>(time_us_64());
        (void)MidiIpcSendMidiEvent(evt);
#if BUILD_I2C_DISPLAY
        InfoScreen::NotifyPlay();
#endif
    }

    void OnProfileReset() override {
        MidiControlEvent ctl{};
        ctl.type = MidiControlType::Reset;
        ctl.channel = 0;
        ctl.timestamp_us = 0;
        (void)MidiIpcSendMidiControl(ctl);
    }

    void OnVendorSysEx(const uint8_t* /*raw*/, uint16_t /*len*/) override {
        // SMFファイル内でベンダー拡張SysExを使う想定はない
    }
};

// 全16チャンネルへAll Notes Off(CC#123)相当を発行する
// （Playing/Pausedから抜けるすべての経路で行う）
void SendAllNotesOff() {
    for (uint8_t ch = 0; ch < 16; ++ch) {
        MidiEvent evt{};
        evt.type = MidiEventType::ChannelMode;
        evt.channel = ch;
        evt.data1 = 123;  // All Notes Off
        evt.data2 = 0;
        evt.size = 3;
        evt.timestamp_us = static_cast<uint32_t>(time_us_64());
        (void)MidiIpcSendMidiEventGuaranteed(evt);
    }
}

// ---------------------------------------------------------------------------
// Ls / Play <index>
// ---------------------------------------------------------------------------
// インデックスはキャッシュしない。Lsは見つけた順に番号を振って列挙するだけ、
// Playは同じ走査をやり直してN番目のファイルを特定する。

struct LsContext {
    uint16_t next_index = 1;
};

bool LsVisitor(void* context, const char* path) {
    auto* ctx = static_cast<LsContext*>(context);
    std::printf("%u: %s\n", ctx->next_index, path);
    ++ctx->next_index;
    // 大量行を一気に送出するとUART送信/ターミナル受信側でマルチバイトUTF-8の
    // 境界を跨いだ文字化けが起きうるため、1行ごとに小休止してバーストを緩和する
    vTaskDelay(pdMS_TO_TICKS(2));
    return true;
}

constexpr size_t kMaxFoundPathLength = 256;

struct FindContext {
    uint16_t target_index;
    uint16_t current_index = 1;
    char     found_path[kMaxFoundPathLength] = {0};
    bool     found = false;
};

bool FindVisitor(void* context, const char* path) {
    auto* ctx = static_cast<FindContext*>(context);
    if (ctx->current_index == ctx->target_index) {
        std::strncpy(ctx->found_path, path, sizeof(ctx->found_path) - 1);
        ctx->found_path[sizeof(ctx->found_path) - 1] = '\0';
        ctx->found = true;
        return false;  // 打ち切り
    }
    ++ctx->current_index;
    return true;
}

// 範囲Allの開始用。全ファイル数を数えつつ、target_indexのパスを控える（打ち切らない）
struct ScanContext {
    uint16_t target_index;
    uint16_t total = 0;
    char     found_path[kMaxFoundPathLength] = {0};
    bool     found = false;
};

bool ScanVisitor(void* context, const char* path) {
    auto* ctx = static_cast<ScanContext*>(context);
    ++ctx->total;
    if (ctx->total == ctx->target_index) {
        std::strncpy(ctx->found_path, path, sizeof(ctx->found_path) - 1);
        ctx->found_path[sizeof(ctx->found_path) - 1] = '\0';
        ctx->found = true;
    }
    return true;
}

struct CountContext {
    uint16_t total = 0;
};

bool CountVisitor(void* context, const char* /*path*/) {
    ++static_cast<CountContext*>(context)->total;
    return true;
}

// 範囲Allの曲数の上限（LCDのPlay SMF一覧と同じ集合にする）
constexpr uint16_t kMaxAllCount = MENU_MAX_SMF_FILES;
static_assert(kMaxAllCount <= PlaybackSequence::kMaxCount,
              "MENU_MAX_SMF_FILES exceeds PlaybackSequence::kMaxCount");

// ---------------------------------------------------------------------------
// 再生ステートマシン
// ---------------------------------------------------------------------------

enum class PlayerState : uint8_t { Idle, Playing, Paused };

// 曲の終了（End of File / エラー）は、検知した箇所では処理せずここに記録し、
// メインループのProcessPending()でまとめて処理する。曲の開始処理の中から次の曲の
// 開始処理を呼ぶ再帰を避けるため。
enum class PendingEvent : uint8_t { None, TrackEnded, TrackFailed };

class SmfPlayerRunner {
public:
    explicit SmfPlayerRunner(IMidiStreamSink& sink) : assembler_(sink) {}

    void HandleLs() {
        LsContext ctx;
        if (!Platform::ForEachSmfFile(LsVisitor, &ctx)) {
            std::printf("smf: cannot enumerate SD card\n");
#if BUILD_I2C_DISPLAY
            InfoScreen::NotifyError("SD: scan failed");
#endif
            return;
        }
        for (uint16_t i = 0; i < kFixtureCount; ++i) {
            std::printf("%u: (builtin) %s\n", ctx.next_index, kFixtures[i].name);
            ++ctx.next_index;
        }
    }

    // SDカード抜き挿し後の手動復帰用。ForEachSmfFile/OpenAtのリアクティブな
    // 再マウントで通常は不要だが、明示的に成否を確認したい場合に使う
    void HandleMount() {
        if (Platform::RemountSdCard()) {
            std::printf("smf: SD card mounted\n");
        } else {
            std::printf("smf: SD card mount failed\n");
        }
    }

    // 範囲All（Lsの連番）で新しいセッションを開始する。連番がSD側の総数を超えていれば
    // 組み込みフィクスチャ、SD側でも範囲の上限を超えていれば単発（Single）として再生する
    void HandlePlay(uint16_t index) {
        if (index == 0) {
            std::printf("smf: invalid index 0\n");
            return;
        }
        DiscardSession();

        ScanContext ctx{index};
        if (!Platform::ForEachSmfFile(ScanVisitor, &ctx)) {
            std::printf("smf: cannot enumerate SD card\n");
#if BUILD_I2C_DISPLAY
            InfoScreen::NotifyError("SD: scan failed");
#endif
            PublishStatus();
            return;
        }
        if (ctx.found) {
            const uint16_t all_count = ctx.total < kMaxAllCount ? ctx.total : kMaxAllCount;
            if (index <= all_count) {
                BeginSession(SmfPlayer::Scope::All, all_count, index);
            } else {
                std::snprintf(single_path_, sizeof(single_path_), "%s", ctx.found_path);
                single_fixture_ = 0;
                BeginSession(SmfPlayer::Scope::Single, 1, 1);
            }
            return;
        }

        // SD側で見つからなければ、組み込みフィクスチャ側のインデックスとみなす
        // （Lsが列挙する連番はSD側の後にフィクスチャが続く）
        if (index > ctx.total) {
            const uint16_t fixture_index = static_cast<uint16_t>(index - ctx.total);
            if (fixture_index >= 1 && fixture_index <= kFixtureCount) {
                single_fixture_ = fixture_index;
                BeginSession(SmfPlayer::Scope::Single, 1, 1);
                return;
            }
        }

        std::printf("smf: index %u not found\n", index);
#if BUILD_I2C_DISPLAY
        InfoScreen::NotifyError("SD: not found");
#endif
        PublishStatus();
    }

    // 範囲Playlist（playlistフォルダ内、名前昇順）で新しいセッションを開始する
    void HandlePlayPlaylist(uint16_t position) {
        DiscardSession();

        CountContext count_ctx;
        const bool opened = Platform::ForEachPlaylistFile(CountVisitor, &count_ctx);
        if (position == 0 || position > count_ctx.total) {
            std::printf(opened ? "smf: playlist position %u not found\n"
                               : "smf: playlist folder not found (position %u)\n", position);
#if BUILD_I2C_DISPLAY
            InfoScreen::NotifyError("SD: not found");
#endif
            PublishStatus();
            return;
        }
        BeginSession(SmfPlayer::Scope::Playlist, count_ctx.total, position);
    }

    void HandleStop() {
        if (state_ == PlayerState::Idle && !seq_.active()) {
            return;
        }
        EndSession(true);
    }

    void HandlePause() {
        if (state_ != PlayerState::Playing) {
            return;
        }
        const uint64_t now = time_us_64();
        paused_remaining_us_ = scheduled_time_us_ > now ? scheduled_time_us_ - now : 0;
        state_ = PlayerState::Paused;
        SendAllNotesOff();
        PublishStatus();
    }

    void HandleResume() {
        if (state_ != PlayerState::Paused) {
            return;
        }
        state_ = PlayerState::Playing;
        scheduled_time_us_ = time_us_64() + paused_remaining_us_;
        PublishStatus();
    }

    // Next/Prev。Pause中でもPlayingになって移動先の曲を再生する
    void HandleStep(SequenceAdvance kind) {
        if (!seq_.active()) {
            return;
        }
        const SequenceStep step = seq_.Advance(kind, Seed());
        if (step == SequenceStep::Stay) {
            return;
        }
        StopTrack();
        fail_count_ = 0;
        StartFromCursor();
    }

    void HandleSetRepeat(RepeatMode mode) {
        seq_.SetRepeat(mode);
        PublishStatus();
    }

    void HandleSetShuffle(bool on) {
        seq_.SetShuffle(on, Seed());
        PublishStatus();
    }

    void HandleSetPlaybackMode(PlaybackMode mode) {
        seq_.SetPlaybackMode(mode);
        PublishStatus();
    }

    // メインループのulTaskNotifyTake()に渡す待ちtick数
    TickType_t WaitTicks() const {
        if (pending_ != PendingEvent::None) {
            return 0;
        }
        if (state_ != PlayerState::Playing) {
            return portMAX_DELAY;
        }
        const uint64_t now = time_us_64();
        if (scheduled_time_us_ <= now) {
            return 0;
        }
        const uint64_t wait_us = scheduled_time_us_ - now;
        const uint64_t wait_ms = (wait_us + 999) / 1000;
        const TickType_t wait_ticks = pdMS_TO_TICKS(wait_ms);
        return wait_ticks > 0 ? wait_ticks : 1;
    }

    // ulTaskNotifyTake()がタイムアウトした（=次のイベント発火時刻に到達した）ときに呼ぶ
    void FireScheduledEvent() {
        if (state_ != PlayerState::Playing || pending_ != PendingEvent::None) {
            return;
        }
        if (scheduled_time_us_ > time_us_64()) {
            return;
        }

        switch (pending_event_.kind) {
        case SmfEventKind::TempoChange:
            current_tempo_us_per_qn_ = pending_event_.tempo_us_per_qn;
            break;
        case SmfEventKind::ChannelMessage:
        case SmfEventKind::SysEx:
            for (uint8_t i = 0; i < pending_event_.length; ++i) {
                assembler_.PushByte(pending_event_.bytes[i]);
            }
            break;
        case SmfEventKind::TrackName:
            if (song_title_[0] == '\0') {
                const uint8_t len = pending_event_.length;
                std::memcpy(song_title_, pending_event_.bytes, len);
                song_title_[len] = '\0';
#if BUILD_I2C_DISPLAY
                // 空のTrack Nameイベント（FF 03 00）では通知しない。NotifyPlay()は
                // 曲名確定としてゲートに関わらずステータス行を上書きするため、空文字列を
                // 渡すとNotifyTrackStart()によるクリアが行われないまま上書きされ、
                // 古い曲名が残ってしまう。
                if (len != 0) {
                    InfoScreen::NotifyPlay(song_title_);
                }
#endif
            }
            break;
        case SmfEventKind::EndOfTrack:
        case SmfEventKind::EndOfFile:
        case SmfEventKind::FormatError:
            break;
        }

        FetchAndSchedule();
    }

    // 再生中の各トラックのSmfByteSourceにI/Oエラーが出ていないか確認する
    void CheckIoErrors() {
        if (state_ != PlayerState::Playing || pending_ != PendingEvent::None) {
            return;
        }
        if (HasSdIoError()) {
            AbortWithError("smf: SD card I/O error");
        }
    }

    // 記録された曲の終了（End of File / エラー）を処理し、次の曲へ進むか、セッションを終える
    void ProcessPending() {
        if (pending_ == PendingEvent::None) {
            return;
        }
        const PendingEvent event = pending_;
        StopTrack();  // pending_も消える
        if (event == PendingEvent::TrackEnded) {
            fail_count_ = 0;
            if (seq_.Advance(SequenceAdvance::TrackEnded, Seed()) == SequenceStep::Finished) {
                EndSession(true);
            } else {
                StartFromCursor();
            }
        } else if (AdvanceAfterFailure()) {
            StartFromCursor();
        }
    }

private:
    static uint32_t Seed() { return static_cast<uint32_t>(time_us_64()); }

    // 全トラックのSmfByteSourceのいずれかがIoErrorを報告しているか
    // （SmfMemoryByteSourceはIoErrorを返さないため、using_sd_のときのみ意味を持つ）
    bool HasSdIoError() const {
        if (!using_sd_) {
            return false;
        }
        for (uint8_t i = 0; i < track_count_; ++i) {
            if (sd_track_sources_[i].LastStatus() == SmfByteSourceStatus::IoError) {
                return true;
            }
        }
        return false;
    }

    void PublishStatus() {
        SmfPlayer::Status status;
        switch (state_) {
        case PlayerState::Idle:    status.state = SmfPlayer::State::Idle; break;
        case PlayerState::Playing: status.state = SmfPlayer::State::Playing; break;
        case PlayerState::Paused:  status.state = SmfPlayer::State::Paused; break;
        }
        if (seq_.active()) {
            status.scope = scope_;
            status.position = seq_.position();
            status.count = seq_.count();
        }
        status.repeat = seq_.repeat();
        status.playback_mode = seq_.playback_mode();
        status.shuffle = seq_.shuffle();
        taskENTER_CRITICAL();
        gStatus = status;
        taskEXIT_CRITICAL();
    }

    // 再生中の曲を止める。Playing/Pausedからは全ノートを止めて抜ける。セッションは残す
    void StopTrack() {
        if (state_ == PlayerState::Playing || state_ == PlayerState::Paused) {
            SendAllNotesOff();
        }
        state_ = PlayerState::Idle;
        pending_ = PendingEvent::None;
        CloseTracks();
    }

    // 再生中の曲とセッションを、状態の公開や通知なしで破棄する（新しいセッションを作る前に使う）
    void DiscardSession() {
        StopTrack();
        seq_.End();
    }

    // セッションを終了してIdleへ戻る。notify_stopは、LCDのステータス行に停止を伝えるか
    // （エラーで終わったときは、エラー表示を残すためfalse）
    void EndSession(bool notify_stop) {
        DiscardSession();
        PublishStatus();
#if BUILD_I2C_DISPLAY
        if (notify_stop) {
            InfoScreen::NotifyStop();
        }
#else
        (void)notify_stop;
#endif
    }

    void BeginSession(SmfPlayer::Scope scope, uint16_t count, uint16_t position) {
        scope_ = scope;
        fail_count_ = 0;
        if (!seq_.Begin(count, position, Seed())) {
            std::printf("smf: invalid session (count %u, position %u)\n", count, position);
            PublishStatus();
            return;
        }
        StartFromCursor();
    }

    // 順序のカーソル位置の曲を開始する。開始できなければ次の曲へ進めて試し直し、
    // 進める曲が無ければセッションを終了する
    void StartFromCursor() {
        for (;;) {
            if (StartCurrent()) {
                PublishStatus();
#if BUILD_I2C_DISPLAY
                InfoScreen::NotifyTrackStart();
#endif
                return;
            }
            if (!AdvanceAfterFailure()) {
                return;
            }
        }
    }

    // 曲の開始/再生の失敗を数え、次に試す曲へカーソルを進める。
    // 進められればtrue。連続失敗が範囲の曲数に達した、または順序の終わりならセッションを終えてfalse
    bool AdvanceAfterFailure() {
        ++fail_count_;
        if (fail_count_ < seq_.count() &&
            seq_.Advance(SequenceAdvance::TrackFailed, Seed()) != SequenceStep::Finished) {
            return true;
        }
        EndSession(false);
        return false;
    }

    // カーソル位置の曲を、範囲に応じて位置からパスへ解決して開始する
    bool StartCurrent() {
        const uint16_t position = seq_.position();
        switch (scope_) {
        case SmfPlayer::Scope::All:
        case SmfPlayer::Scope::Playlist: {
            FindContext ctx{position};
            bool enumerated = false;
            if (scope_ == SmfPlayer::Scope::All) {
                enumerated = Platform::ForEachSmfFile(FindVisitor, &ctx);
            } else {
                enumerated = Platform::ForEachPlaylistFile(FindVisitor, &ctx);
            }
            if (!enumerated) {
                std::printf("smf: cannot enumerate SD card\n");
#if BUILD_I2C_DISPLAY
                InfoScreen::NotifyError("SD: scan failed");
#endif
                return false;
            }
            if (!ctx.found) {
                std::printf("smf: position %u not found\n", position);
#if BUILD_I2C_DISPLAY
                InfoScreen::NotifyError("SD: not found");
#endif
                return false;
            }
            return PlaySdFile(ctx.found_path);
        }
        case SmfPlayer::Scope::Single:
            if (single_fixture_ != 0) {
                return PlayFixture(kFixtures[single_fixture_ - 1]);
            }
            return PlaySdFile(single_path_);
        }
        return false;
    }

    // 曲の再生中にエラーを検知した。エラーを通知し、曲の終了として記録する
    void AbortWithError(const char* message) {
        std::printf("%s\n", message);
#if BUILD_I2C_DISPLAY
        InfoScreen::NotifyError("SD: I/O error");
#endif
        pending_ = PendingEvent::TrackFailed;
    }

    void CloseTracks() {
        if (using_sd_) {
            for (uint8_t i = 0; i < track_count_; ++i) {
                sd_track_sources_[i].Close();
            }
        }
        track_count_ = 0;
        using_sd_ = false;
    }

    bool PlaySdFile(const char* path) {
        Platform::SmfSdByteSource header_source;
        if (!header_source.OpenAt(path, 0)) {
            std::printf("smf: cannot open %s\n", path);
#if BUILD_I2C_DISPLAY
            InfoScreen::NotifyError("SD: open failed");
#endif
            return false;
        }

        SmfTrackInfo track_infos[kMaxSmfTracks];
        uint8_t track_count = 0;
        bool trailing_garbage = false;
        const SmfScanResult scan_result = parser_.ScanChunks(
            header_source, track_infos, kMaxSmfTracks, track_count, &trailing_garbage);
        header_source.Close();

        if (scan_result == SmfScanResult::TooManyTracks) {
            std::printf("smf: too many tracks (limit %u) in %s\n", kMaxSmfTracks, path);
#if BUILD_I2C_DISPLAY
            InfoScreen::NotifyError("SD: too many trk");
#endif
            return false;
        }
        if (scan_result != SmfScanResult::Ok) {
            std::printf("smf: format error in %s\n", path);
#if BUILD_I2C_DISPLAY
            InfoScreen::NotifyError("SD: bad format");
#endif
            return false;
        }
        if (trailing_garbage) {
            std::printf("smf: warning: ignored malformed trailing data in %s\n", path);
        }

        SmfByteSource* sources[kMaxSmfTracks];
        for (uint8_t i = 0; i < track_count; ++i) {
            if (!sd_track_sources_[i].OpenAt(path, track_infos[i].start_offset)) {
                std::printf("smf: cannot open track %u of %s\n", i, path);
#if BUILD_I2C_DISPLAY
                InfoScreen::NotifyError("SD: open failed");
#endif
                for (uint8_t j = 0; j < i; ++j) {
                    sd_track_sources_[j].Close();
                }
                return false;
            }
            sources[i] = &sd_track_sources_[i];
        }

        track_count_ = track_count;
        using_sd_ = true;
        if (!StartPlayback(sources, track_count)) {
            return false;
        }
        std::printf("smf: playing %s (%u track%s)\n", path, track_count,
                    track_count == 1 ? "" : "s");
        return true;
    }

    bool PlayFixture(const SmfFixture& fixture) {
        SmfMemoryByteSource header_source(fixture.data, fixture.length);

        SmfTrackInfo track_infos[kMaxSmfTracks];
        uint8_t track_count = 0;
        bool trailing_garbage = false;
        const SmfScanResult scan_result = parser_.ScanChunks(
            header_source, track_infos, kMaxSmfTracks, track_count, &trailing_garbage);

        if (scan_result != SmfScanResult::Ok) {
            std::printf("smf: format error in builtin fixture %s\n", fixture.name);
            return false;
        }
        if (trailing_garbage) {
            std::printf("smf: warning: ignored malformed trailing data in builtin fixture %s\n",
                        fixture.name);
        }

        SmfByteSource* sources[kMaxSmfTracks];
        for (uint8_t i = 0; i < track_count; ++i) {
            fixture_track_sources_[i] = SmfMemoryByteSource(
                fixture.data, fixture.length, track_infos[i].start_offset);
            sources[i] = &fixture_track_sources_[i];
        }

        track_count_ = track_count;
        using_sd_ = false;
        if (!StartPlayback(sources, track_count)) {
            return false;
        }
        std::printf("smf: playing builtin fixture %s (%u track%s)\n", fixture.name, track_count,
                    track_count == 1 ? "" : "s");
        return true;
    }

    // 再生を開始できればtrue。開始できなければトラックを閉じてfalse（stateはIdleのまま）
    bool StartPlayback(SmfByteSource* const* sources, uint8_t track_count) {
        song_title_[0] = '\0';
        current_tempo_us_per_qn_ = kSmfDefaultTempoUsPerQuarterNote;
        if (!parser_.Begin(sources, track_count)) {
            std::printf("smf: no playable events\n");
#if BUILD_I2C_DISPLAY
            InfoScreen::NotifyError("SD: bad format");
#endif
            CloseTracks();
            state_ = PlayerState::Idle;
            return false;
        }
        state_ = PlayerState::Playing;
        scheduled_time_us_ = time_us_64();
        paused_remaining_us_ = 0;
        FetchAndSchedule();
        return true;
    }

    // parser_からpending_event_を1件取得し、その発火予定時刻(scheduled_time_us_)を
    // 現在の再生位置に積算する。EndOfFileなら曲の終了として記録する
    void FetchAndSchedule() {
        if (!parser_.NextEvent(pending_event_)) {
            AbortWithError("smf: parser internal error");
            return;
        }
        if (pending_event_.kind == SmfEventKind::EndOfFile) {
            // 全トラックが同一呼び出し内で同時に読み取り失敗すると、正常なEOFと
            // 区別できずここに来る。CheckIoErrors()が走る前にI/Oエラーとして
            // 検知しておかないと、正常終了として握り潰されてしまう
            if (HasSdIoError()) {
                AbortWithError("smf: SD card I/O error");
                return;
            }
            std::printf("smf: playback finished\n");
            pending_ = PendingEvent::TrackEnded;
            return;
        }

        const uint32_t ticks_per_qn = parser_.TicksPerQuarterNote();
        const uint64_t delta_us =
            (static_cast<uint64_t>(pending_event_.delta_ticks) * current_tempo_us_per_qn_) /
            ticks_per_qn;
        scheduled_time_us_ += delta_us;
    }

    SmfParser    parser_;
    MidiStreamAssembler assembler_;
    PlayerState  state_ = PlayerState::Idle;
    PendingEvent pending_ = PendingEvent::None;

    // セッション
    PlaybackSequence seq_;
    SmfPlayer::Scope scope_ = SmfPlayer::Scope::All;
    uint16_t single_fixture_ = 0;             // Singleで組み込みフィクスチャなら1始まりの番号、SDファイルなら0
    char     single_path_[kMaxFoundPathLength] = {0};  // SingleでSDファイルのときのパス
    uint16_t fail_count_ = 0;                 // 曲が正常に終わるまでの連続失敗数

    uint8_t  track_count_ = 0;
    bool     using_sd_ = false;
    Platform::SmfSdByteSource sd_track_sources_[kMaxSmfTracks];
    SmfMemoryByteSource       fixture_track_sources_[kMaxSmfTracks];

    SmfEvent pending_event_{};
    uint32_t current_tempo_us_per_qn_ = kSmfDefaultTempoUsPerQuarterNote;
    uint64_t scheduled_time_us_ = 0;
    uint64_t paused_remaining_us_ = 0;

    // 最初に現れたTrack Nameメタイベント（曲名として扱われることが多い）。
    // LCD表示用に最初の1件だけ保持する
    char song_title_[33] = {0};
};

}  // namespace

void SmfPlayer::RequestPlay(uint16_t index)            { SendCommand(SmfCommand::Play, index); }
void SmfPlayer::RequestPlayPlaylist(uint16_t position) { SendCommand(SmfCommand::PlayPlaylist, position); }
void SmfPlayer::RequestStop()                          { SendCommand(SmfCommand::Stop, 0); }
void SmfPlayer::RequestPause()                         { SendCommand(SmfCommand::Pause, 0); }
void SmfPlayer::RequestResume()                        { SendCommand(SmfCommand::Resume, 0); }
void SmfPlayer::RequestNext()                          { SendCommand(SmfCommand::Next, 0); }
void SmfPlayer::RequestPrev()                          { SendCommand(SmfCommand::Prev, 0); }
void SmfPlayer::RequestSetRepeat(RepeatMode mode)      { SendCommand(SmfCommand::SetRepeat, static_cast<uint16_t>(mode)); }
void SmfPlayer::RequestSetShuffle(bool on)             { SendCommand(SmfCommand::SetShuffle, on ? 1 : 0); }
void SmfPlayer::RequestSetPlaybackMode(PlaybackMode mode) { SendCommand(SmfCommand::SetPlaybackMode, static_cast<uint16_t>(mode)); }
void SmfPlayer::RequestLs()                            { SendCommand(SmfCommand::Ls, 0); }
void SmfPlayer::RequestMount()                         { SendCommand(SmfCommand::Mount, 0); }

SmfPlayer::Status SmfPlayer::GetStatus() {
    Status status;
    taskENTER_CRITICAL();
    status = gStatus;
    taskEXIT_CRITICAL();
    return status;
}

void SmfPlayerTask(void* /*param*/) {
    gSmfPlayerTaskHandle = xTaskGetCurrentTaskHandle();

    static SmfPlayerStreamSink sink;
    static SmfPlayerRunner runner(sink);

    for (;;) {
        const TickType_t wait = runner.WaitTicks();
        if (ulTaskNotifyTake(pdTRUE, wait) > 0) {
            SmfCommandMessage cmd;
            while (PopCommand(&cmd)) {
                switch (cmd.type) {
                case SmfCommand::Play:         runner.HandlePlay(cmd.arg); break;
                case SmfCommand::PlayPlaylist: runner.HandlePlayPlaylist(cmd.arg); break;
                case SmfCommand::Stop:         runner.HandleStop(); break;
                case SmfCommand::Pause:        runner.HandlePause(); break;
                case SmfCommand::Resume:       runner.HandleResume(); break;
                case SmfCommand::Next:         runner.HandleStep(SequenceAdvance::UserNext); break;
                case SmfCommand::Prev:         runner.HandleStep(SequenceAdvance::UserPrev); break;
                case SmfCommand::SetRepeat:
                    if (cmd.arg <= static_cast<uint16_t>(RepeatMode::Loop)) {
                        runner.HandleSetRepeat(static_cast<RepeatMode>(cmd.arg));
                    }
                    break;
                case SmfCommand::SetShuffle:   runner.HandleSetShuffle(cmd.arg != 0); break;
                case SmfCommand::SetPlaybackMode:
                    if (cmd.arg <= static_cast<uint16_t>(PlaybackMode::Continuous)) {
                        runner.HandleSetPlaybackMode(static_cast<PlaybackMode>(cmd.arg));
                    }
                    break;
                case SmfCommand::Ls:           runner.HandleLs(); break;
                case SmfCommand::Mount:        runner.HandleMount(); break;
                }
            }
        } else {
            runner.FireScheduledEvent();
        }
        runner.CheckIoErrors();
        runner.ProcessPending();
    }
}
