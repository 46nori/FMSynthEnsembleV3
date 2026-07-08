//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>
#include "MidiChannel.h"

MidiChannel::MidiChannel(int no)
    : channel(no),
      bk_program(0),
      volume(-1),
      expression(-1),
      pan(-1),
      hold1(false),
      rpn_msb(127),
      rpn_lsb(127),
      nrpn_msb(127),
      nrpn_lsb(127),
      data_entry_addr_(DataEntryAddr::None),
      bk_stg_msb(0),
      bk_rx_msb(0),
      bk_rx_lsb(0),
      rel_success_count(0),
      rel_fail_count(0) {
    effect.Init();
    // 奇数MIDIチャンネルはL、偶数MIDIチャンネルはRに出力
    // (no, outputLRの基数は0なのに注意)
    outputLR = (no % 2 ? R : L);
}

MidiChannel::~MidiChannel() {
}

void MidiChannel::Reset() {
    AllNoteOff();
    ResetAllController();
    outputLR          = (channel % 2 ? R : L);
    volume            = -1;
    expression        = -1;
    pan               = -1;
    bk_program        = 0;
    bk_stg_msb        = 0;
    bk_rx_msb         = 0;
    bk_rx_lsb         = 0;

    rel_success_count = 0;
    rel_fail_count    = 0;
}

void MidiChannel::ResetAllController() {
    effect.Init();
    expression        = -1;
    hold1             = false;
    rpn_msb           = 127;
    rpn_lsb           = 127;
    nrpn_msb          = 127;
    nrpn_lsb          = 127;
    data_entry_addr_  = DataEntryAddr::None;
}

int MidiChannel::GetNumber() {
    return channel;
}

void MidiChannel::SetOutputLR(uint8_t lr) {
    outputLR = lr;
}

void MidiChannel::SetProgram(uint8_t no) {
    bk_program = (bk_program & 0xffff0000) | no;
}

uint32_t MidiChannel::GetProgram() {
    return bk_program;
}

void MidiChannel::SetVolume(int vol) {
    volume = vol;
}

void MidiChannel::SetExpression(int val) {
    expression = val;
}

int MidiChannel::EffectiveVolume(int velocity) const {
    if (volume < 0 && expression < 0 && velocity < 0) {
        return -1;
    }

    const int base_volume = volume < 0 ? 127 : volume;
    const int expr        = expression < 0 ? 127 : expression;
    const int vel         = velocity < 0 ? 127 : velocity;
    const int expressed   = (base_volume * expr + 63) / 127;
    return (expressed * vel + 63) / 127;
}

void MidiChannel::Hold1(int val) {
    hold1 = (val >= 64);
}

void MidiChannel::PitchBend(int16_t val) {
    effect.pbv = val;
}

void MidiChannel::NRPN_MSB(uint8_t val) {
    nrpn_msb = val;
    rpn_msb  = 127;
    rpn_lsb  = 127;
    data_entry_addr_ = DataEntryAddr::Nrpn;
}

void MidiChannel::NRPN_LSB(uint8_t val) {
    nrpn_lsb = val;
    rpn_msb  = 127;
    rpn_lsb  = 127;
    data_entry_addr_ = DataEntryAddr::Nrpn;
}

void MidiChannel::RPN_MSB(uint8_t val) {
    rpn_msb = val;
    nrpn_msb = 127;
    nrpn_lsb = 127;
    data_entry_addr_ = DataEntryAddr::Rpn;
}

void MidiChannel::RPN_LSB(uint8_t val) {
    rpn_lsb = val;
    nrpn_msb = 127;
    nrpn_lsb = 127;
    data_entry_addr_ = DataEntryAddr::Rpn;
}

void MidiChannel::DataEntry_MSB(uint8_t val) {
}

void MidiChannel::DataEntry_LSB(uint8_t val) {
}

void MidiChannel::SetModulation(uint8_t val) {
}

void MidiChannel::SetPan(uint8_t val) {
}

void MidiChannel::BankSelect_MSB(uint8_t val) {
    bk_stg_msb = val;
}

void MidiChannel::BankSelect_LSB(uint8_t val) {
    bk_program &= 0x0000ffff;
    bk_program |= ((int32_t)bk_stg_msb << 24) | ((int32_t)val << 16);
}

// Debug
void MidiChannel::dump() {
    std::printf("\nCH=%02d PG=%04x %04x VOL=%03d EXP=%03d LR=%02x hold=%d ct=%2d pbs=%2d pbv=%5d\n",
           channel, bk_program >> 16, bk_program & 0xffff, volume, expression, outputLR, hold1,
           effect.coarse_tune, effect.pbs, effect.pbv);
}

void MidiChannel::stats() {
    std::printf("CH=%02d Release Success=%d Failure=%d\n", channel, rel_success_count, rel_fail_count);
}
