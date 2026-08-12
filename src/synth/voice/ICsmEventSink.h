//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

/**
 * @brief CSM Start/Stop イベントの送信先インターフェース
 * @details CsmVoice はこのインターフェース越しにのみ CsmFrameTask への
 *          Start/Stop 通知を行う。実体（csm_ipc 経由の実装）は app 層が持つ。
 *          FrameTick は FM /IRQ ISR からの低レイテンシ経路のため対象外
 *          （design_csm_frame.md 7章、CsmVoice::IrqTickThunk 参照）。
 */
class ICsmEventSink {
public:
    virtual ~ICsmEventSink() = default;

    /**
     * @brief CSM再生開始をCsmFrameTaskへ依頼
     */
    virtual void SignalCsmStart(int note, int32_t program, int volume, uint8_t lr) = 0;

    /**
     * @brief CSM再生停止をCsmFrameTaskへ依頼
     */
    virtual void SignalCsmStop() = 0;
};
