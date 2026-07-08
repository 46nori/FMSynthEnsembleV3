//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>
#include <memory>

#include "IMidiPanelDriver.h"

/**
 * @brief MIDI Panel のオーケストレーション層
 * @details ドライバ差し替えを吸収し、app 向け API を提供する。
 *          マトリックス制御・LED モード・トグル FSM はドライバ内部の責務。
 */
class MidiPanelController {
public:
    /**
     * @brief コンストラクタ
     * @param [in] driver パネルドライバ（所有権を移す）
     * @details driver が利用可能なら Initialize() を呼ぶ。
     */
    explicit MidiPanelController(std::unique_ptr<IMidiPanelDriver> driver);

    /**
     * @brief パネルが物理的に接続されているか
     * @return 接続済みなら true
     */
    bool IsConnected() const;

    /**
     * @brief 1 周期分の処理
     * @param [in] midi_ch_active_bitmap gLastNoteOnBitmap。ドライバがモード B のとき LED に使用
     * @details SetLedBitmap → Tick() の順でドライバを呼ぶ。
     */
    void Tick(uint16_t midi_ch_active_bitmap);

    /**
     * @brief MIDI エンジン向け CH 有効マスク
     * @return ドライバのトグル状態ビットマップ。未接続時は 0xffff
     */
    uint16_t GetChannelEnableBitmap() const;

    /**
     * @brief MIDI リセット入力の状態
     * @return driver_->IsMidiReset() の委譲。未接続時は false
     */
    bool IsMidiReset() const;

private:
    std::unique_ptr<IMidiPanelDriver> driver_;
};
