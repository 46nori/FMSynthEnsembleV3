//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>
#include "CsmVoice.h"
#include "config.h"
#include "isr.h"
#include "csm_ipc.h"

//#define ENABLE_INTERPOLATION
#ifdef ENABLE_INTERPOLATION
constexpr int INTERPOLATE = 4;
#endif

namespace {

/** 話題ごとの frame_data シンボル区間（開始インデックスと長さ） */
struct VoiceSpan {
    int start;
    int length;
};

constexpr VoiceSpan kVoiceSpans[] = {
    {  3, 27}, // T
    { 27, 27}, // E
    { 50, 27}, // C
    { 78, 27}, // H
    { 95, 27}, // N
    {130, 27}, // O
    {158, 25}, // P
    {182, 27}, // O
    {210, 24}, // L
    {229, 27}, // I
    {256, 27}, // S
    {340, 48}, // TOKIO
};
constexpr int kVoiceSpanCount = static_cast<int>(sizeof(kVoiceSpans) / sizeof(kVoiceSpans[0]));

/** Timer B リロード値（VOICE.dat の FRAME_PERIOD と補間設定に連動） */
constexpr uint8_t TimerBReloadByte() {
#ifdef ENABLE_INTERPOLATION
    return static_cast<uint8_t>(256.f - 125.f * static_cast<float>(FRAME_PERIOD) /
                                                     (static_cast<float>(INTERPOLATE) * 36.f));
#else
    return static_cast<uint8_t>(256.f - 125.f * static_cast<float>(FRAME_PERIOD) / 36.f);
#endif
}

}  // namespace

CsmVoice::CsmVoice(std::array<OpnBase*, 4>& modules, int id)
    : Voice(true, id),  // CSM type
      modules(modules),
      num_modules(0),
      operators(0),
      modTB(0),
      dock_indices{},
      frame(0),
      interp_count(0),
      lastFrame(0),
      isLastFrame(false),
      running(false) {
    // 実装済みの物理Dockを先頭から順にスキャンし、dock_indicesに記録する
    // 中間Dockが欠けた構成 (例: Dock0=null, Dock1=null, Dock2=valid) にも対応
    for (int i = 0; i < static_cast<int>(modules.size()); ++i) {
        if (modules[i] != nullptr) {
            dock_indices[num_modules++] = i;
        }
    }
    // Timer Bは最初の実装済みDockが担当する
    modTB = (num_modules > 0) ? dock_indices[0] : 0;
    SetProgram(0);   // デフォルト音色
    SetVolume(100);  // デフォルト音量
}

void CsmVoice::IrqTickThunk(void* /*ctx*/) {
    CsmSignalFrameTick();
}

CsmVoice::~CsmVoice() {
    if (irq_gpio_ >= 0) {
        Platform::DisableIsrCallback(irq_gpio_);
        irq_gpio_ = -1;
    }
}

void CsmVoice::Reset() {
    // CSM再生状態はCsmFrameTaskが所有するため、停止は順序付きイベントで依頼する。
    CsmSignalStop();

    // コンストラクタと同じ設定にする
    SetNoteOnCount(0);
    SetChannel(-1);
    bk_program   = -1;
    volume       = -1;
    key          = -1;
    SetProgram(0);
    SetVolume(100);
}

int CsmVoice::GetModuleId() {
    // NoteVoiceと動作を合わせるために、便宜的にModule IDを返す
    return modules[modTB]->id;
}

void CsmVoice::SetProgram(int32_t no) {
    bk_program = no;
}

void CsmVoice::SetVolume(int vol) {
    volume = vol;
}

void CsmVoice::NoteOn(int note, int32_t bk_program, int volume, ChannelEffects& effect, uint8_t lr) {
    (void)effect;
    IncrementNoteOnCount();
    CsmSignalStart(note, bk_program, volume, lr);
}

bool CsmVoice::TryRetrigger(int note, int32_t program, int vol, ChannelEffects& effect, uint8_t lr) {
    (void)effect;
    key = note;
    SetProgram(program);
    SetVolume(vol);
    CsmSignalStart(note, program, vol, lr);
    return true;
}

void CsmVoice::NoteOff() {
    SetNoteOnCount(0);
#if ENABLE_CSM_STOP_IMMEDIATE != 0
    CsmSignalStop();
#endif
}

void CsmVoice::ApplyPitch(const ChannelEffects& /*fx*/, int16_t /*vib_cents*/,
                          bool /*allow_vib_dedup*/) {
    // ピッチベンド・ビブラートは非対応
}

void CsmVoice::SetPan(uint8_t lr) {
    for (int d = 0; d < num_modules; d++) {
        modules[dock_indices[d]]->fm_set_output_lr(2, lr);
    }
}

void CsmVoice::Init() {
    static_assert(CSM_N_MAX >= 1 && CSM_N_MAX <= 16, "CSM_N_MAX must be in range 1..16.");
    static_assert(CSM_N <= CSM_N_MAX, "CSM_N must not exceed CSM_N_MAX.");

    // CSM_N_MAXから予約モジュール数を算出し、実装済み数 (コンストラクタで確定) を上限とする
    const int required = (CSM_N_MAX - 1) / 4 + 1;
    num_modules = (required < num_modules) ? required : num_modules;

    // 実際に使用するオペレータ数
    operators = CSM_N;
    const int max_operators = num_modules * 4;
    if (operators > max_operators) {
        operators = max_operators;
    }
    if (operators < 4) {
        operators = 4;
    }

    // 使用するFM音源モジュールのCH3を初期化 (使用するモジュールのみ)
    for (int d = 0; d < num_modules; d++) {
        init_ch3(*modules[dock_indices[d]]);
    }

    // フレーム周期をTimer Bに設定
    modules[modTB]->set_timer_b(TimerBReloadByte());

    // FM /IRQ は Wired-OR で各 Dock とも同一 GPIO（isr.h）
    irq_gpio_ = FM_IRQ;
    Platform::AttachIsrCallback(FM_IRQ, &CsmVoice::IrqTickThunk, this);
}

void CsmVoice::UpdateFrame(bool isFirst) {
    if (isFirst) {
        running = true;
        update(true);
        return;
    }

    if (!running) {
        return;
    }

    if (isLastFrame) {
        // 最終フレームなのでTimerB割り込み信号の発生を停止
        stop_playback_locked();
        running = false;
    } else {
        // 2回目以降の更新処理
        update(false);
    }
}

void CsmVoice::Start(int note, int32_t program, int vol, uint8_t lr) {
    Stop();
    key = note;
    SetProgram(program);
    SetVolume(vol);
    SetPan(lr);
    UpdateFrame(true);
}

bool CsmVoice::IsFrameOver() {
    return modules[modTB]->read_status() & 0x02;
}

void CsmVoice::Stop() {
    stop_playback_locked();
    running = false;
}

void CsmVoice::stop_playback_locked() {
    for (OpnBase* opn : modules) {
        if (opn) {
            opn->fm_turnoff_key(2);
            opn->set_timer_mode(0x30);
        }
    }
    isLastFrame  = false;
    frame        = 0;
    interp_count = 0;
    lastFrame    = 0;
}

void CsmVoice::init_ch3(OpnBase& opn) {
    constexpr int ch3 = 2;  // CH3

    // CH3のKey ON時のエンベロープパターン
    constexpr OpnBase::fm_env env = {
        0x00,  // KS
        0x1f,  // AR max
        0x1f,  // DR max
        0x1f,  // SR max
        0x00,  // SL = 0 makes no DR effect
        0x0a,  // RR
    };

    opn.set_timer_mode(0x30);  // Timer A/B をリセット

    opn.set_fmch3_mode(2);                          // CH3をCSMモードにセット
    opn.fm_turnoff_key(ch3);                        // CH3の全オペレータをOFF
    opn.fm_set_algorithm(ch3, 0, 7);                // Feedback=0, Algorithm=7
    for (int op = 0; op < 4; op++) {                // Init Operaters
        opn.fm_set_detune_multiple(ch3, op, 0, 1);  // Detune=0, Multiple=1
        opn.fm_set_total_level(ch3, op, 0x7f);      // Total Level (-96dB)
        opn.fm_set_envelope(ch3, op, env);          // Envelope
        opn.fm_set_fnumber_ch3(op, 0, 0);           // Block/F-Number
    }
    opn.fm_set_output_lr(ch3, 0xc0);  // LR両方に出力(YM2608の場合)
}

void CsmVoice::phraseSetupFromKey(bool isFirst) {
    // 初回呼び出し処理
    if (!isFirst) {
        return;
    }
    interp_count = 0;
    const VoiceSpan& span = kVoiceSpans[key % kVoiceSpanCount];
    frame       = span.start;
    lastFrame   = frame + span.length;
    isLastFrame = false;
    data        = frame_format{};
}

uint16_t CsmVoice::loadFramePitchAndMarkLast() {
    uint16_t pitch = frame_data[frame].Pitch;
#if 1
    if (frame == lastFrame) {
        pitch &= 0x7fff;
        isLastFrame = true;
    }
#else
    // Pitchの最上位ビットが立っている場合は最終フレーム
    if (pitch & 0x8000) {
        pitch &= 0x7fff;
        isLastFrame = true;
    }
#endif
    return pitch;
}

#ifdef ENABLE_INTERPOLATION
void CsmVoice::interpolationAccumulate(uint16_t pitch_from_table) {
    if (interp_count == 0) {
        // 差分の計算
        diff.Pitch = (pitch_from_table - data.Pitch) / INTERPOLATE;
        for (int j = 0; j < operators; j++) {
            diff.TL[j]   = (frame_data[frame].TL[j] - data.TL[j]) / INTERPOLATE;
            diff.Freq[j] = (frame_data[frame].Freq[j] - data.Freq[j]) / INTERPOLATE;
        }
    }

    // 線形補間
    data.Pitch += diff.Pitch;
    for (int j = 0; j < operators; j++) {
        data.TL[j] += diff.TL[j];
        data.Freq[j] += diff.Freq[j];
    }
}
#endif

void CsmVoice::writeFrameParametersToModules(uint16_t timer_a_pitch) {
    // フレームパラメータの更新
    // dock_indices[d] が物理Dockインデックス (非nullのみ保証済み、nullチェック不要)
    for (int d = 0; d < num_modules; d++) {
        const int dock = dock_indices[d];
        modules[dock]->set_timer_a(timer_a_pitch);  // ピッチ (Timer A)

        for (uint8_t op = 0; op < 4; op++) {
            const int n = d * 4 + op;
            if (n >= operators) {
                continue;
            }

#ifdef ENABLE_INTERPOLATION
            const uint8_t tl = static_cast<uint8_t>(data.TL[n]);
#else
            const uint8_t tl = frame_data[frame].TL[n];
#endif
            modules[dock]->fm_set_total_level(2, op, tl);  // 振幅

            uint16_t fnum = 0;
            for (int blk = 5; blk >= 0; blk--) {
                // f = 72 * csm_f * (2**20) / 4000000 / (2 ** (blk - 1))
#ifdef ENABLE_INTERPOLATION
                const uint32_t f = ((uint32_t)38 * data.Freq[n]) >> blk;
#else
                const uint32_t f = ((uint32_t)38 * frame_data[frame].Freq[n]) >> blk;
#endif
                if (f < 2048) {
                    fnum = static_cast<uint16_t>(((blk << 11) | f) & 0x3fff);
                    break;
                }
            }
            modules[dock]->fm_set_fnumber_ch3(op, fnum >> 8, fnum & 0xff);  // 周波数
        }
    }
}

void CsmVoice::restartTimersAllDocks() {
    // Timer B(フレーム)とTimer A(ピッチ)をスタート
    // dock_indices[0] が modTB dock (Timer A & B)、それ以降 (Timer A のみ)
    for (int d = 0; d < num_modules; d++) {
        const int dock = dock_indices[d];
        if (d == 0) {
            modules[dock]->set_timer_mode(0x2b);  // Timer A & B
        } else {
            modules[dock]->set_timer_mode(0x01);  // Timer A
        }
    }
}

void CsmVoice::advanceContentStep() {
#ifdef ENABLE_INTERPOLATION
    if (interp_count++ == INTERPOLATE) {
        interp_count = 0;
        frame++;
    }
#else
    frame++;
#endif
}

bool CsmVoice::update(bool isFirst) {
    phraseSetupFromKey(isFirst);

    const uint16_t pitch_table = loadFramePitchAndMarkLast();

#ifdef ENABLE_INTERPOLATION
    interpolationAccumulate(pitch_table);
    writeFrameParametersToModules(data.Pitch);
#else
    writeFrameParametersToModules(pitch_table);
#endif

    restartTimersAllDocks();
    advanceContentStep();
    return isLastFrame;
}

void CsmVoice::dump() {
    std::printf("ID=%02d CH=%02d PG=%04x %04x VOL=%3d KEY=%3d TYPE=%s\n", id, GetChannel(),
           bk_program >> 16, bk_program & 0xffff, volume, GetKey(), GetType() ? "CSM " : "Note");
}
