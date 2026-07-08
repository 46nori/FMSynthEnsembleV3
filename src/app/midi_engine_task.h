//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <array>
#include "config.h"
#include "MidiChannel.h"
#include "MidiProcessor.h"

struct MidiEngineTaskContext {
    MidiProcessor* processor;
    std::array<MidiChannel*, MIDI_CHANNELS>* channels;
};

void MidiEngineTask(void* param);
