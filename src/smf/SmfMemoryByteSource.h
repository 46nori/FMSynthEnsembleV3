//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "SmfByteSource.h"

/**
 * @brief 固定バイト列をラップするだけの SmfByteSource 実装
 * @details ポインタ演算のみで、ヒープもFatFsも使わない。ホストユニットテストと
 *          実機組み込みフィクスチャ再生の両方で使う。
 */
class SmfMemoryByteSource : public SmfByteSource {
public:
    /** @brief 空のソース（即EOF）。固定長配列の要素初期化用 */
    SmfMemoryByteSource() : SmfMemoryByteSource(nullptr, 0) {}

    /**
     * @param data 読み取り対象のバイト列（呼び出し元が寿命を保証すること）
     * @param length dataの長さ
     * @param start_offset 読み取り開始位置（Format 1で個々のトラック開始位置に使う）
     */
    SmfMemoryByteSource(const uint8_t* data, uint32_t length, uint32_t start_offset = 0);

    bool ReadByte(uint8_t& out) override;
    SmfByteSourceStatus LastStatus() const override;

private:
    const uint8_t* data_;
    uint32_t length_;
    uint32_t pos_;
    SmfByteSourceStatus status_ = SmfByteSourceStatus::Ok;
};
