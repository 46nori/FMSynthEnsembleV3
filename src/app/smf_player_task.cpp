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
#include "fixtures/smf_test_fixtures.h"

namespace {

// 手持ちファイルの実測最大値(21)に余裕を見た値
constexpr uint8_t kMaxSmfTracks = 25;

// ---------------------------------------------------------------------------
// Debugger <-> SmfPlayerTask コマンド
// ---------------------------------------------------------------------------

enum class SmfCommand : uint8_t { Play, Stop, Pause, Resume, Ls, Mount };

struct SmfCommandMailbox {
    SmfCommand type;
    uint16_t   index;  // Play のときのみ有効
};

TaskHandle_t gSmfPlayerTaskHandle = nullptr;
SmfCommandMailbox gMailbox;

void SendMailbox(SmfCommand type, uint16_t index) {
    if (gSmfPlayerTaskHandle == nullptr) {
        return;  // SmfPlayerTask起動前は無視する
    }
    taskENTER_CRITICAL();
    gMailbox.type = type;
    gMailbox.index = index;
    taskEXIT_CRITICAL();
    xTaskNotifyGive(gSmfPlayerTaskHandle);
}

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
        (void)MidiIpcSendMidiEvent(evt);
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

// ---------------------------------------------------------------------------
// 再生ステートマシン
// ---------------------------------------------------------------------------

enum class PlayerState : uint8_t { Idle, Playing, Paused };

class SmfPlayerRunner {
public:
    explicit SmfPlayerRunner(IMidiStreamSink& sink) : assembler_(sink) {}

    void HandleLs() {
        LsContext ctx;
        Platform::ForEachSmfFile(LsVisitor, &ctx);
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

    void HandlePlay(uint16_t index) {
        if (index == 0) {
            std::printf("smf: invalid index 0\n");
            return;
        }
        StopInternal();

        FindContext ctx{index};
        Platform::ForEachSmfFile(FindVisitor, &ctx);
        if (ctx.found) {
            PlaySdFile(ctx.found_path);
            return;
        }

        // SD側で見つからなければ、組み込みフィクスチャ側のインデックスとみなす
        // （Lsが列挙する連番はSD側の後にフィクスチャが続く）
        const uint16_t sd_count = static_cast<uint16_t>(ctx.current_index - 1);
        if (index > sd_count) {
            const uint16_t fixture_index = static_cast<uint16_t>(index - sd_count);
            if (fixture_index >= 1 && fixture_index <= kFixtureCount) {
                PlayFixture(kFixtures[fixture_index - 1]);
                return;
            }
        }

        std::printf("smf: index %u not found\n", index);
    }

    void HandleStop() {
        StopInternal();
    }

    void HandlePause() {
        if (state_ != PlayerState::Playing) {
            return;
        }
        state_ = PlayerState::Paused;
        SendAllNotesOff();
    }

    void HandleResume() {
        if (state_ != PlayerState::Paused) {
            return;
        }
        state_ = PlayerState::Playing;
        scheduled_time_us_ = time_us_64();  // 無音から再開するので基準時刻を取り直す
    }

    // メインループのulTaskNotifyTake()に渡す待ちtick数
    TickType_t WaitTicks() const {
        if (state_ != PlayerState::Playing) {
            return portMAX_DELAY;
        }
        const uint64_t now = time_us_64();
        if (scheduled_time_us_ <= now) {
            return 0;
        }
        const uint64_t wait_us = scheduled_time_us_ - now;
        return pdMS_TO_TICKS(wait_us / 1000);
    }

    // ulTaskNotifyTake()がタイムアウトした（=次のイベント発火時刻に到達した）ときに呼ぶ
    void FireScheduledEvent() {
        if (state_ != PlayerState::Playing) {
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
        case SmfEventKind::EndOfTrack:
        case SmfEventKind::EndOfFile:
        case SmfEventKind::FormatError:
            break;
        }

        FetchAndSchedule();
    }

    // 再生中の各トラックのSmfByteSourceにI/Oエラーが出ていないか確認する
    void CheckIoErrors() {
        if (state_ != PlayerState::Playing) {
            return;
        }
        if (HasSdIoError()) {
            AbortWithError("smf: SD card I/O error");
        }
    }

private:
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

    void StopInternal() {
        if (state_ == PlayerState::Playing || state_ == PlayerState::Paused) {
            SendAllNotesOff();
        }
        state_ = PlayerState::Idle;
        CloseTracks();
    }

    void AbortWithError(const char* message) {
        std::printf("%s\n", message);
        StopInternal();
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

    void PlaySdFile(const char* path) {
        Platform::SmfSdByteSource header_source;
        if (!header_source.OpenAt(path, 0)) {
            std::printf("smf: cannot open %s\n", path);
            return;
        }

        SmfTrackInfo track_infos[kMaxSmfTracks];
        uint8_t track_count = 0;
        bool trailing_garbage = false;
        const SmfScanResult scan_result = parser_.ScanChunks(
            header_source, track_infos, kMaxSmfTracks, track_count, &trailing_garbage);
        header_source.Close();

        if (scan_result == SmfScanResult::TooManyTracks) {
            std::printf("smf: too many tracks (limit %u) in %s\n", kMaxSmfTracks, path);
            return;
        }
        if (scan_result != SmfScanResult::Ok) {
            std::printf("smf: format error in %s\n", path);
            return;
        }
        if (trailing_garbage) {
            std::printf("smf: warning: ignored malformed trailing data in %s\n", path);
        }

        SmfByteSource* sources[kMaxSmfTracks];
        for (uint8_t i = 0; i < track_count; ++i) {
            if (!sd_track_sources_[i].OpenAt(path, track_infos[i].start_offset)) {
                std::printf("smf: cannot open track %u of %s\n", i, path);
                for (uint8_t j = 0; j < i; ++j) {
                    sd_track_sources_[j].Close();
                }
                return;
            }
            sources[i] = &sd_track_sources_[i];
        }

        track_count_ = track_count;
        using_sd_ = true;
        StartPlayback(sources, track_count);
        std::printf("smf: playing %s (%u track%s)\n", path, track_count,
                    track_count == 1 ? "" : "s");
    }

    void PlayFixture(const SmfFixture& fixture) {
        SmfMemoryByteSource header_source(fixture.data, fixture.length);

        SmfTrackInfo track_infos[kMaxSmfTracks];
        uint8_t track_count = 0;
        bool trailing_garbage = false;
        const SmfScanResult scan_result = parser_.ScanChunks(
            header_source, track_infos, kMaxSmfTracks, track_count, &trailing_garbage);

        if (scan_result != SmfScanResult::Ok) {
            std::printf("smf: format error in builtin fixture %s\n", fixture.name);
            return;
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
        StartPlayback(sources, track_count);
        std::printf("smf: playing builtin fixture %s (%u track%s)\n", fixture.name, track_count,
                    track_count == 1 ? "" : "s");
    }

    void StartPlayback(SmfByteSource* const* sources, uint8_t track_count) {
        current_tempo_us_per_qn_ = kSmfDefaultTempoUsPerQuarterNote;
        if (!parser_.Begin(sources, track_count)) {
            std::printf("smf: no playable events\n");
            CloseTracks();
            state_ = PlayerState::Idle;
            return;
        }
        state_ = PlayerState::Playing;
        scheduled_time_us_ = time_us_64();
        FetchAndSchedule();
    }

    // parser_からpending_event_を1件取得し、その発火予定時刻(scheduled_time_us_)を
    // 現在の再生位置に積算する。EndOfFileならここで再生終了処理まで行う
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
            StopInternal();
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

    uint8_t  track_count_ = 0;
    bool     using_sd_ = false;
    Platform::SmfSdByteSource sd_track_sources_[kMaxSmfTracks];
    SmfMemoryByteSource       fixture_track_sources_[kMaxSmfTracks];

    SmfEvent pending_event_{};
    uint32_t current_tempo_us_per_qn_ = kSmfDefaultTempoUsPerQuarterNote;
    uint64_t scheduled_time_us_ = 0;
};

}  // namespace

void SmfPlayer::RequestPlay(uint16_t index)  { SendMailbox(SmfCommand::Play, index); }
void SmfPlayer::RequestStop()                { SendMailbox(SmfCommand::Stop, 0); }
void SmfPlayer::RequestPause()               { SendMailbox(SmfCommand::Pause, 0); }
void SmfPlayer::RequestResume()              { SendMailbox(SmfCommand::Resume, 0); }
void SmfPlayer::RequestLs()                  { SendMailbox(SmfCommand::Ls, 0); }
void SmfPlayer::RequestMount()               { SendMailbox(SmfCommand::Mount, 0); }

void SmfPlayerTask(void* /*param*/) {
    gSmfPlayerTaskHandle = xTaskGetCurrentTaskHandle();

    static SmfPlayerStreamSink sink;
    static SmfPlayerRunner runner(sink);

    for (;;) {
        const TickType_t wait = runner.WaitTicks();
        if (ulTaskNotifyTake(pdTRUE, wait) > 0) {
            SmfCommandMailbox cmd;
            taskENTER_CRITICAL();
            cmd = gMailbox;
            taskEXIT_CRITICAL();

            switch (cmd.type) {
            case SmfCommand::Play:   runner.HandlePlay(cmd.index); break;
            case SmfCommand::Stop:   runner.HandleStop(); break;
            case SmfCommand::Pause:  runner.HandlePause(); break;
            case SmfCommand::Resume: runner.HandleResume(); break;
            case SmfCommand::Ls:     runner.HandleLs(); break;
            case SmfCommand::Mount:  runner.HandleMount(); break;
            }
        } else {
            runner.FireScheduledEvent();
        }
        runner.CheckIoErrors();
    }
}
