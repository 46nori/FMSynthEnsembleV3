//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "MidiRoutingPolicy.h"

#include <array>
#include <cstddef>

namespace {

template <std::size_t N>
bool equals_message(const uint8_t* raw, uint16_t len, const std::array<uint8_t, N>& target) {
    if (raw == nullptr || len != target.size()) {
        return false;
    }
    for (std::size_t i = 0; i < target.size(); ++i) {
        if (raw[i] != target[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace

MidiRouteDecision MidiRoutingPolicy::DecideForEvent(const MidiEvent& /*event*/) {
    return MidiRouteDecision::ForwardToEngine;
}

MidiRouteDecision MidiRoutingPolicy::DecideForSysEx(const uint8_t* raw, uint16_t len) {
    if (raw == nullptr || len < 6) {
        return MidiRouteDecision::Drop;
    }

    // F0 7D 46 4D <cmd> ... F7
    if (raw[0] == 0xf0 && raw[1] == 0x7d && raw[2] == 0x46 && raw[3] == 0x4d && raw[len - 1] == 0xf7) {
        return MidiRouteDecision::HandleOnCore0;
    }

    return MidiRouteDecision::Drop;
}

bool MidiRoutingPolicy::IsProfileResetSysEx(const uint8_t* raw, uint16_t len) {
    static constexpr std::array<uint8_t,  6> kGMSystemOn = {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
    static constexpr std::array<uint8_t,  9> kXGReset    = {0xf0, 0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7};
    static constexpr std::array<uint8_t, 11> kGSReset    = {0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};

    return equals_message(raw, len, kGMSystemOn) ||
           equals_message(raw, len, kXGReset)    ||
           equals_message(raw, len, kGSReset);
}
