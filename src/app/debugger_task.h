//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <array>

#include "config.h"

class MidiChannel;

struct DebuggerTaskContext {
    std::array<MidiChannel*, MIDI_CHANNELS>* channels;
};

void DebuggerTask(void* param);
