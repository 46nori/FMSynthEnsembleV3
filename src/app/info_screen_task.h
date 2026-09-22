//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <array>

#include "MidiFactory.h"
#include "MidiPanelController.h"
#include "OpnBase.h"

/** @brief InfoScreenTask に渡すコンテキスト */
struct InfoScreenTaskContext {
    const std::array<OpnBase*, 4>* modules;  ///< Dockごとの検出済みFMモジュール（非所有）
    MidiPanelController* panel;              ///< MIDIパネルコントローラ（非所有）
    int midiPanelDock;                       ///< MIDIパネルが接続されるDock番号
    MidiFactory* factory;                    ///< Voice構成の参照元（非所有）
};

/**
 * @brief ディスプレイの画面制御タスク
 * @param [in] param InfoScreenTaskContext へのポインタ
 * @details LcdMenu（extern/LcdMenu）ベースのジョイスティック操作メニューを表示する。
 *          行0は演奏状態のステータス行として固定表示し、行1-3をLcdMenuのメニュー領域とする。
 */
void InfoScreenTask(void* param);

namespace InfoScreen {

/**
 * @brief 演奏イベントが発生したことを通知する（Playイベント）
 * @param [in] title 曲名が判明している場合はその文字列。未確定ならnullptr（既定値）
 * @details 呼び出し側は種類を問わず気軽に毎回呼んでよい。「最初の1件だけ実際に
 *          伝える」重複排除はInfoScreen内部（gPerformanceActiveゲート）で行う。
 *          ただしtitle指定時はゲートに関わらず常に伝える（曲名の後決め対応）。
 */
void NotifyPlay(const char* title = nullptr);

/**
 * @brief 曲の再生が始まったことを通知する（曲の切り替え時）
 * @details 前の曲の曲名表示を消し、新しい曲の曲名が判明するまでは`Playing`を表示する。
 */
void NotifyTrackStart();

/**
 * @brief 再生セッションが終了したことを通知する（Stopイベント）
 * @details 自然終了と手動停止のどちらでも呼ぶ。次の曲へ続く曲の終了では呼ばない。
 *          エラーで終わったセッションでは、エラー表示を残すため呼ばない。
 */
void NotifyStop();

/**
 * @brief エラーが発生したことを通知し、ステータス行に即座に表示する
 * @param [in] message 表示する短いメッセージ（20桁に収まる長さを想定）
 * @details 主にSDカードアクセス失敗（SmfPlayerTask）向け。Play同様、無操作が
 *          5秒続くとステータス行は自動的にクリアされる。
 */
void NotifyError(const char* message);

}  // namespace InfoScreen
