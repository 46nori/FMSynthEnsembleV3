//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

class Voice;

/**
 * @brief IVoiceReclaimable Interface
 * @details VoiceAllocatorからVoiceの回収を受け付けるインターフェース。
 *          voice/層に置くことで voice/ → channel/ の依存を排除する。
 */
class IVoiceReclaimable {
public:
    /**
     * @brief コンストラクタ
     */
    IVoiceReclaimable() = default;

    /**
     * @brief デストラクタ
     */
    virtual ~IVoiceReclaimable() = default;

    /**
     * @brief チャンネルに割り当てられたVoiceのうち未使用のものを解放する
     * @return 割り当てできない場合はnullptrを返す
     */
    virtual Voice* Reclaim(int mid, bool type) = 0;

    /**
     * @brief 割り当てられたVoiceをすべて解放する
     */
    virtual void ReclaimAll() = 0;
};

struct IVoiceReclaimableInfo {
    int channel;
    IVoiceReclaimable* reclaim_target;
};
