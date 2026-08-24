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
 * @brief MidiStreamAssembler が確定したイベントの送信先インターフェース
 * @details 実体は呼び出し側（app 層）が持つ。IPC キュー送信・タイムスタンプ付与・
 *          Debugger 呼び出し等の pico-sdk / FreeRTOS 依存はすべてここに閉じる。
 */
class IMidiStreamSink {
public:
    virtual ~IMidiStreamSink() = default;

    /** @brief Core1 へ転送すべき対応済み MidiEvent が確定した */
    virtual void OnMidiEvent(const MidiEvent& event) = 0;

    /** @brief GM/XG/GS プロファイルリセット SysEx を受信した */
    virtual void OnProfileReset() = 0;

    /** @brief ベンダー拡張 SysEx (F0 7D 46 4D ... F7) を受信した */
    virtual void OnVendorSysEx(const uint8_t* raw, uint16_t len) = 0;
};

/**
 * @brief USB MIDI から受信したバイト列を組み立て、イベント/SysEx を確定させる
 * @details runningStatus 管理・SysEx バッファリング・暗黙終了を扱うステートマシン。
 *          pico-sdk / FreeRTOS に依存しない（AGENTS.md の midi/ レイヤ制約）。
 */
class MidiStreamAssembler {
public:
    explicit MidiStreamAssembler(IMidiStreamSink& sink) : sink_(sink) {}

    /** @brief USB から受信した 1 バイトを処理する */
    void PushByte(uint8_t value);

private:
    void HandleStatusByte(uint8_t status);
    void HandleDataByte(uint8_t value);
    void ResetSysEx();
    void HandleCompleteSysEx();
    void EnqueueEventIfNeeded(const uint8_t* raw, uint8_t len);

    IMidiStreamSink& sink_;

    static constexpr uint16_t kMaxSysExLength = 256;
    bool     inSysEx_      = false;
    bool     sysExOverflow_ = false;
    uint16_t sysExLength_  = 0;
    uint8_t  sysEx_[kMaxSysExLength] = {0};

    uint8_t runningStatus_  = 0;
    uint8_t msg_[3]         = {0};
    uint8_t msgLength_      = 0;
    uint8_t expectedLength_ = 0;
};
