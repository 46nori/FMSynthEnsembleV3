//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <memory>

#include "IMidiPanelDriver.h"

class OpnBase;

/**
 * @brief 接続状態に応じた IMidiPanelDriver 実装を生成する
 * @param [in] opn FM モジュールへのポインタ。nullptr なら NullMidiPanelDriver
 * @return OpnMidiPanelDriver（BUILD_MIDI_PANEL かつ opn 非 null）または NullMidiPanelDriver
 */
std::unique_ptr<IMidiPanelDriver> CreateMidiPanelDriver(OpnBase* opn);
