//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "smf_sd_byte_source.h"

#include "init.h"

namespace Platform {

namespace {

// SDカード抜去等でFatFsがハードウェアと通信できない状態を示すエラーコード。
// hw_config.cにCard Detectピンの配線がなく能動検知ができないため、実際に
// I/Oが失敗した時点でリアクティブに再マウントを試みる。
bool IndicatesCardNotReady(FRESULT fr) {
    return fr == FR_DISK_ERR || fr == FR_NOT_READY;
}

}  // namespace

SmfSdByteSource::~SmfSdByteSource() {
    Close();
}

bool SmfSdByteSource::OpenAt(const char* path, uint32_t byte_offset) {
    Close();

    FRESULT fr = f_open(&fil_, path, FA_READ);
    if (fr != FR_OK) {
        if (IndicatesCardNotReady(fr) && RemountSdCard()) {
            fr = f_open(&fil_, path, FA_READ);
        }
        if (fr != FR_OK) {
            status_ = SmfByteSourceStatus::IoError;
            return false;
        }
    }
    open_ = true;

    if (byte_offset != 0 && f_lseek(&fil_, byte_offset) != FR_OK) {
        Close();
        status_ = SmfByteSourceStatus::IoError;
        return false;
    }

    status_ = SmfByteSourceStatus::Ok;
    return true;
}

void SmfSdByteSource::Close() {
    if (open_) {
        f_close(&fil_);
        open_ = false;
    }
}

bool SmfSdByteSource::ReadByte(uint8_t& out) {
    if (!open_) {
        status_ = SmfByteSourceStatus::IoError;
        return false;
    }

    UINT bytes_read = 0;
    if (f_read(&fil_, &out, 1, &bytes_read) != FR_OK) {
        status_ = SmfByteSourceStatus::IoError;
        return false;
    }
    if (bytes_read == 0) {
        status_ = SmfByteSourceStatus::EndOfFile;
        return false;
    }

    status_ = SmfByteSourceStatus::Ok;
    return true;
}

SmfByteSourceStatus SmfSdByteSource::LastStatus() const {
    return status_;
}

}  // namespace Platform
