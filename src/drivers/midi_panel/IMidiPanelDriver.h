//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

/**
 * @brief MIDI Panel デバイス抽象インターフェース
 * @details synth / app は PA・PB・マトリックス・PortA を知らない。
 *          具象実装（OPN 直結・シリアル等）を差し替え可能にする。
 */
class IMidiPanelDriver {
public:
    virtual ~IMidiPanelDriver() = default;

    /**
     * @brief パネルが利用可能か（接続・初期化済み）
     * @return 利用可能なら true
     */
    virtual bool IsAvailable() const = 0;

    /**
     * @brief 初回利用前に 1 回呼ぶ
     */
    virtual void Initialize() = 0;

    /**
     * @brief ホスト側が LED に反映してほしいビットマップを渡す
     * @param [in] led_bitmap bit i = CH(i+1)。1=点灯希望, 0=消灯希望
     * @details 具象ドライバはハードの LED モードに応じて使用するか無視する。
     *          マルチプレックス表示は Tick() で行う。
     */
    virtual void SetLedBitmap(uint16_t led_bitmap) = 0;

    /**
     * @brief 現在のスイッチ状態（ソフトトグル後）を返す
     * @return bit i = CH(i+1)。1=ON, 0=OFF
     */
    virtual uint16_t GetSwitchBitmap() const = 0;

    /**
     * @brief 周期処理（マトリックススキャン・LED 刷新・トグル更新等）
     * @details MidiPanelTask から定期的に呼ぶ。1 回の呼び出し = 1 列スロット。
     */
    virtual void Tick() = 0;

    /**
     * @brief MIDI リセット入力の状態を返す
     * @return リセット成立中なら true（レベル）、それ以外は false
     * @details 成立判定は Tick() 内で更新する想定。Tick() の後に呼ぶこと。
     *          具象ドライバは未実装の場合は常に false を返す。
     */
    virtual bool IsMidiReset() const = 0;
};
