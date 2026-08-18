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
// PortA/PortB音外れ診断（sample_task.cpp）を再現させる場合も1にする
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
// 有効にすると十分な時間KeyOnをキープしないと発音し切らずに途切れてしまうため、通常は無効にする。
// All Sound Off / ForceOff / Reset / ボイス奪取では本フラグに関わらず常に停止する。
#define ENABLE_CSM_STOP_IMMEDIATE              0

// ------------------------------
// エフェクト関連
// ------------------------------
// COARSE TUNEの有効化
#define ENABLE_COARSE_TUNE                     1

// ビブラート（ソフトウェア LFO）
#define VIBRATO_PERIOD_MS            20
#define VIBRATO_DT_SEC               (VIBRATO_PERIOD_MS / 1000.0f)
#define VIBRATO_RATE_MIN_HZ          3.0f
#define VIBRATO_RATE_MAX_HZ          12.0f
#define VIBRATO_DEPTH_MAX_CENTS      50
#define VIBRATO_DELAY_MAX_MS         500

#define MIDI_EVENT_BATCH_MAX         32

// ------------------------------
// ミックス関連
// ------------------------------
// リズム音源 (RhythmChannel) の RTL/IL 追加減衰 step (1 step = 0.75 dB)。
// FM (NoteVoice) に対してリズムが前に出る場合に増やす。実行時はデバッガ `rmix` でも変更可。
#define RHYTHM_LEVEL_OFFSET          0
