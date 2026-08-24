//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "ff.h"
#include "SmfByteSource.h"

namespace Platform {

/**
 * @brief SDカード上のファイルをラップする SmfByteSource 実装
 * @details FatFsの`FIL`はトラックごとに1個ずつ生成する。位置決めは OpenAt() 内で
 *          1回だけ行い、以降 ReadByte() は純粋な順方向の f_read() のみ。
 *          FatFsへの実アクセスはこのクラスに閉じ込める。呼び出しは app 層の
 *          SmfPlayerTaskに一元化されており、複数タスクからの同時利用は想定しない。
 */
class SmfSdByteSource : public SmfByteSource {
public:
    SmfSdByteSource() = default;
    ~SmfSdByteSource() override;

    SmfSdByteSource(const SmfSdByteSource&) = delete;
    SmfSdByteSource& operator=(const SmfSdByteSource&) = delete;

    /**
     * @brief ファイルを開き、指定バイトオフセットへ1回だけ位置決めする
     * @param path SDボリューム上のパス（"0:/..."）
     * @param byte_offset 開始位置。0ならシークしない
     * @return 成功すればtrue
     */
    bool OpenAt(const char* path, uint32_t byte_offset);

    /** @brief 開いていれば閉じる。デストラクタからも呼ばれる */
    void Close();

    bool IsOpen() const { return open_; }

    bool ReadByte(uint8_t& out) override;
    SmfByteSourceStatus LastStatus() const override;

private:
    FIL fil_{};
    bool open_ = false;
    SmfByteSourceStatus status_ = SmfByteSourceStatus::Ok;
};

}  // namespace Platform
