//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

// アプリケーション（独自に設計した部分）のビルド時設定。どのレイヤから include してもよい。
//
// - ここに置くもの: 独自コンポーネントの有無、動作方針のフラグ、数値の上限・レンジ、
//   診断用の切り替え。値（#define / constexpr）だけを置き、他のプロジェクトヘッダに依存しない
// - ここに置かないもの: 外部ライブラリ・SDK の構成・特定ハードウェアに依存するコンポーネントの
//   有無。これはルート CMakeLists.txt の fmse_build_option() で宣言し、build_config.h に生成される
// - タスクの優先度・スタックは app/task_config.h、ピン割り当ては platform/ に置く

// 構造に関するオプション（BUILD_SD_CARD など。CMake が生成）
#include "build_config.h"

// ------------------------------
//  MIDI基本設定関連
// ------------------------------
// システムでサポートするMIDIチャンネル数(1-16)
constexpr int MIDI_CHANNELS = 16;

// ------------------------------
//  LCDメニュー関連
// ------------------------------
// Play SMF画面に並べるSMFファイルの最大数(1-255)
// SDカード上でこれを超えて見つかったファイルはメニューに出ない（シリアルデバッガの
// `smf play <番号>`からは再生できる）。
// 上限が255なのは、LcdMenuの項目位置(カーソル・表示位置)がuint8_tで、1画面の項目数が
// 最大256のため。256でも収まる計算だが、境界(uint8_tの折り返し)を実機で検証していない
// ので1件の余裕を残している。
// 1件あたり、約21バイトのRAM(ファイル名バッファ)、約12バイトのフラッシュ
// (再生コールバック表と再生関数)、メニュー項目のヒープ数十バイトを使う。
constexpr int MENU_MAX_SMF_FILES = 255;

// ------------------------------
// デバッグ関連
// ------------------------------
// デバッグモードの有効化
#define ENABLE_DEBUG_PRINT                     1

// MIDI キュー滞留・イベント実行時間の詳細計測 (1:有効, 0:無効)
// 診断ビルド用。有効にすると debugger の stats に滞留・実行時間・上位イベントが追加表示される
#define ENABLE_MIDI_TIMING_STATS               0

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
#define VIBRATO_PERIOD_MS                      20
#define VIBRATO_DT_SEC                         (VIBRATO_PERIOD_MS / 1000.0f)
#define VIBRATO_RATE_MIN_HZ                    3.0f
#define VIBRATO_RATE_MAX_HZ                    12.0f
#define VIBRATO_DEPTH_MAX_CENTS                50
#define VIBRATO_DELAY_MAX_MS                   500

// トレモロ（ビブラートと同じソフトウェア LFO を共有）
#define TREMOLO_DEPTH_MAX_STEPS                8    // trdepth=127 のときの最大追加減衰（0.75dB/step、約6dB）

#define MIDI_EVENT_BATCH_MAX                   32

// ------------------------------
// ミックス関連
// ------------------------------
// リズム音源 (RhythmChannel) の RTL/IL 追加減衰 step (1 step = 0.75 dB)。
// FM (NoteVoice) に対してリズムが前に出る場合に増やす。実行時はデバッガ `rmix` / LCD の Settings > RhythmVol でも変更可。
#define RHYTHM_LEVEL_OFFSET                    0
