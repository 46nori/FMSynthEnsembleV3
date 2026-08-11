//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "OpnFeatures.h"
#include "opn_piolib.h"

/**
 * @brief SSG レジスタ経由の I/O ポート実装（IIoPort フィーチャ）
 * @details SSG PORT A (0x0e) / PORT B (0x0f)、
 */
class OpnSsgIoPort final : public IIoPort {
public:
    explicit OpnSsgIoPort(const fm_device_t* dev) : dev_(dev) {}

    void set_port_direction(bool pa, bool pb) override {
        uint8_t data = read_reg(dev_, 0x07, 0) & 0x3f;
        // D6=IOA, D7=IOB: 1=output, 0=input
        if (pa) {
            data |= 0x40;
        } else {
            data &= 0xbf;
        }
        if (pb) {
            data |= 0x80;
        } else {
            data &= 0x7f;
        }
        write_reg(dev_, 0x07, 0, data);
    }

    void write_port_a(uint8_t data) override {
        write_reg(dev_, 0x0e, 0, data);
    }

    void write_port_b(uint8_t data) override {
        write_reg(dev_, 0x0f, 0, data);
    }

    uint8_t read_port_a() override {
        return read_reg(dev_, 0x0e, 0);
    }

    uint8_t read_port_b() override {
        return read_reg(dev_, 0x0f, 0);
    }

private:
    const fm_device_t* dev_;
};
