//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

enum class MidiSysExKind : uint8_t {
    Drop,
    ProfileReset,
    VendorDebug,
};

namespace MidiSysEx {

MidiSysExKind Classify(const uint8_t* raw, uint16_t len);

}  // namespace MidiSysEx
