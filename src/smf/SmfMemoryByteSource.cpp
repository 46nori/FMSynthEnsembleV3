//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "SmfMemoryByteSource.h"

SmfMemoryByteSource::SmfMemoryByteSource(const uint8_t* data, uint32_t length, uint32_t start_offset)
    : data_(data), length_(length), pos_(start_offset) {
}

bool SmfMemoryByteSource::ReadByte(uint8_t& out) {
    if (pos_ >= length_) {
        status_ = SmfByteSourceStatus::EndOfFile;
        return false;
    }
    out = data_[pos_++];
    status_ = SmfByteSourceStatus::Ok;
    return true;
}

SmfByteSourceStatus SmfMemoryByteSource::LastStatus() const {
    return status_;
}
