//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

// ------------------------------
//  MIDI基本設定関連
// ------------------------------
// システムでサポートするMIDIチャンネル数(1-16)
constexpr int MIDI_CHANNELS = 16;

// ------------------------------
// デバッグ関連
// ------------------------------
// デバッグモードの有効化
#define ENABLE_DEBUG_PRINT                     1

// FreeRTOS 基本動作確認用のサンプルタスク (1:有効, 0:無効)
#define ENABLE_FREERTOS_SAMPLE_TASK            0

// ------------------------------
//  CSM関連
// ------------------------------
// CSMボイスの有効化
#define ENABLE_CSM                             1

// CSM_Nの最大値(1-16)
// (CSM_N_MAX-1) / 4 + 1個のモジュールのCH3を使用するので、その分NoteVoiceの最大数が減る
#define CSM_N_MAX                              12

// CSM NoteOn時に現在の発音と未処理FrameTickを捨てて、新しい発音を優先開始する
// 無効にすると、古い発音が残っている間は新しい発音を開始できなくなるため、通常は有効にする
#define ENABLE_CSM_START_PREEMPT               1

// CSM NoteOffでCSM Voiceの発音を直ちに止める
// 有効にすると十分な時間KeyOnをキープしないと発音し切らずに途切れてしまうため、通常は無効にする
#define ENABLE_CSM_STOP_IMMEDIATE              0

// ------------------------------
// エフェクト関連
// ------------------------------
// COARSE TUNEの有効化
#define ENABLE_COARSE_TUNE                     1

// ビブラート（ソフトウェア LFO）
#define VIBRATO_PERIOD_MS            20
#define VIBRATO_DT_SEC               (VIBRATO_PERIOD_MS / 1000.0f)
#define VIBRATO_TICK_HZ              (1000 / VIBRATO_PERIOD_MS)
#define VIBRATO_RATE_MIN_HZ          3.0f
#define VIBRATO_RATE_MAX_HZ          12.0f
#define VIBRATO_DEPTH_MAX_CENTS      50
// Attack 遅延: 発音直後 TickVibrato の FM 書き込みを抑制（0=無効）
#define VIBRATO_ATTACK_DELAY_MS      12
// このキー以上は上記に加えて FM 書き込み開始を遅らせる（短い高音向け）
#define VIBRATO_HIGH_NOTE_KEY        72
#define VIBRATO_HIGH_NOTE_EXTRA_MS   16
// これより短い単音には TickVibrato で FM を書かない（vib 0 と同等）
#define VIBRATO_MIN_SOUNDING_MS      50
#define VIBRATO_HIGH_MIN_SOUNDING_MS 75

#define MIDI_NOTE_BATCH_MAX          32
#define MIDI_EFFECT_BATCH_MAX        8
#define MIDI_NOTE_DRAIN_MAX          128

// ------------------------------
// ミックス関連
// ------------------------------
// リズム音源 (RhythmChannel) の RTL/IL 追加減衰 step (1 step = 0.75 dB)。
// FM (NoteVoice) に対してリズムが前に出る場合に増やす。実行時はデバッガ `rmix` でも変更可。
#define RHYTHM_LEVEL_OFFSET          6
