//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

/**
 * @brief SmfByteSource::ReadByte() が false を返した理由
 */
enum class SmfByteSourceStatus : uint8_t {
    Ok,
    EndOfFile,  // 正常終端
    IoError,    // SDカード抜去等の読み取りエラー
};

/**
 * @brief SmfParser にバイト列を供給する抽象インターフェース
 * @details トラック切替のたびにシークする設計は、シーク先までのFATチェーン走査
 *          コストとバッファ内容の使い捨てが常時発生しうるため採らない。Seek() は
 *          含めず、トラック開始位置への位置決めは実装ごとの初期化手順
 *          （SmfSdByteSource::OpenAt() 等）に閉じ込める。
 */
class SmfByteSource {
public:
    virtual ~SmfByteSource() = default;

    /**
     * @brief 現在位置から1バイト読む
     * @param[out] out 読み取ったバイト
     * @return 読めればtrue、読めなければfalse（理由はLastStatus()参照）
     */
    virtual bool ReadByte(uint8_t& out) = 0;

    /** @brief 直前のReadByte()がfalseを返した理由 */
    virtual SmfByteSourceStatus LastStatus() const = 0;
};
