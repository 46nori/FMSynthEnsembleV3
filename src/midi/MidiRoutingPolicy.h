//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

#include "MidiMessage.h"

/**
 * @brief MIDI メッセージの転送先を表す列挙型
 */
enum class MidiRouteDecision : uint8_t {
    ForwardToEngine,  ///< MidiEngineTask (Core1) へ転送する
    HandleOnCore0,    ///< UsbMidiTask (Core0) で処理する (ベンダー SysEx 等)
    Drop,             ///< 廃棄する
};

/**
 * @brief MIDI メッセージの転送先を決定するルーティングポリシー
 */
class MidiRoutingPolicy {
public:
    /**
     * @brief チャンネルイベントの転送先を決定する
     *
     * @param[in] event  パース済みの MidiEvent
     * @return 転送先の MidiRouteDecision
     * @note 現在は常に ForwardToEngine を返す
     */
    static MidiRouteDecision DecideForEvent(const MidiEvent& event);

    /**
     * @brief SysEx メッセージの転送先を決定する
     *
     * @param[in] raw  SysEx バイト列。raw[0]==0xF0、raw[len-1]==0xF7 であること
     * @param[in] len  バイト列の長さ (F0〜F7 を含む全バイト数)
     * @return ベンダー SysEx (F0 7D 46 4D ...) なら HandleOnCore0、それ以外は Drop
     */
    static MidiRouteDecision DecideForSysEx(const uint8_t* raw, uint16_t len);

    /**
     * @brief SysEx がプロファイルリセット (GM/XG/GS) か判定する
     *
     * @param[in] raw  SysEx バイト列。raw[0]==0xF0、raw[len-1]==0xF7 であること
     * @param[in] len  バイト列の長さ
     * @return GM System On / XG Reset / GS Reset のいずれかに一致すれば true
     */
    static bool IsProfileResetSysEx(const uint8_t* raw, uint16_t len);
};
