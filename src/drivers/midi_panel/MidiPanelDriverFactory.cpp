//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "MidiPanelDriverFactory.h"

#include <memory>

#include "NullMidiPanelDriver.h"

#if BUILD_MIDI_PANEL
#include "OpnMidiPanelDriver.h"
#endif

std::unique_ptr<IMidiPanelDriver> CreateMidiPanelDriver(OpnBase* opn) {
#if BUILD_MIDI_PANEL
    if (opn != nullptr) {
        return std::make_unique<OpnMidiPanelDriver>(*opn);
    }
#else
    (void)opn;
#endif
    return std::make_unique<NullMidiPanelDriver>();
}
