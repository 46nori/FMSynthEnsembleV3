//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "MidiPanelDriverFactory.h"

#include <cstdio>
#include <memory>

#include "NullMidiPanelDriver.h"
#include "OpnBase.h"

#if BUILD_MIDI_PANEL
#include "OpnMidiPanelDriver.h"

namespace {

const char* ChipKindName(ChipKind kind) {
    switch (kind) {
    case ChipKind::YM2203: return "YM2203";
    case ChipKind::YM2608: return "YM2608";
    case ChipKind::YMF288: return "YMF288";
    }
    return "Unknown";
}

}  // namespace
#endif

std::unique_ptr<IMidiPanelDriver> CreateMidiPanelDriver(OpnBase* opn) {
#if BUILD_MIDI_PANEL
    if (opn != nullptr) {
        if (auto* io = opn->io_port()) {
            return std::make_unique<OpnMidiPanelDriver>(*io);
        }
        std::printf(
            "MIDI Panel: Dock %d is %s, which has no I/O port.\n",
            opn->id, ChipKindName(opn->chip_kind()));
    }
#else
    (void)opn;
#endif
    return std::make_unique<NullMidiPanelDriver>();
}
