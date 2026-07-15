//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "Voice.h"

#include "FreeRTOS.h"
#include "task.h"

Voice::Voice(bool type, int id)
    : note_on_count(0), type(type), midi_ch(-1), pitch_attack_start_tick_(0), bk_program(-1),
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
    key                      = -1;
    note_on_count            = 0;
    pitch_attack_start_tick_ = 0;
}

void Voice::MarkPitchAttackStart() {
    pitch_attack_start_tick_ = static_cast<uint32_t>(xTaskGetTickCount());
}

uint32_t Voice::GetPitchAttackStartTick() const {
    return pitch_attack_start_tick_;
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

void Voice::SetNoteOnCount(int val) {
    note_on_count = val;
}

int Voice::GetNoteOnCount() {
    return note_on_count;
}

int Voice::IncrementNoteOnCount() {
    return ++note_on_count;
}

int Voice::DecrementNoteOnCount() {
    if (--note_on_count < 0) {
        note_on_count = 0;
    }
    return note_on_count;
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
