//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "Voice.h"

Voice::Voice(bool type, int id)
    : type(type), midi_ch(-1), bk_program(-1),
      volume(-1), velocity(-1), key(-1), id(id) {
}

Voice::~Voice() {
}

void Voice::Reset() {
    NoteOff();
    midi_ch       = -1;
    bk_program    = -1;
    volume        = -1;
    velocity      = -1;
    key           = -1;
}

bool Voice::GetType() {
    return type;
}

bool Voice::IsFree() {
    return midi_ch == -1;
}

int Voice::GetChannel() {
    return midi_ch;
}

void Voice::SetChannel(int channel) {
    midi_ch = channel;
}

int Voice::GetKey() const {
    return key;
}

void Voice::SetVelocity(int val) {
    velocity = val;
}

int Voice::GetVelocity() {
    return velocity;
}

void Voice::ForceOff() {
    NoteOff();
}

bool Voice::TryRetrigger(int note, int32_t bk_program, int volume, ChannelEffects& effect,
                         uint8_t lr) {
    (void)note;
    (void)bk_program;
    (void)volume;
    (void)effect;
    (void)lr;
    return false;
}

void Voice::dump() {
    // Debug implementation
}
