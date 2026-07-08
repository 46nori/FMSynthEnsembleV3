//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "volume_controller.h"
#include "hardware/pio.h"

namespace Platform {
namespace {

constexpr uint8_t kClockFreqKHz = 100;   // Clock frequency: 100kHz
constexpr uint8_t kDataPin      = 27;    // Data pin for NJU72343 (GPIO27)
constexpr uint8_t kClockPin     = 28;    // Clock pin for NJU72343 (GPIO28)
constexpr uint8_t kMuteValue    = 0xff;  // Mute value: 0xff
constexpr int16_t kMinDbX2      = -190;  // Minimum dB value: -95.0dB
constexpr int16_t kMaxDbX2      = 63;    // Maximum dB value: +31.5dB
constexpr uint8_t kControlG1H1ZeroCrossOff = 0x00;  // G1/H1 select, ZC off
constexpr uint8_t kControlG1H1ZeroCrossOn  = 0x01;  // G1/H1 select, ZC on

constexpr uint8_t kChipAddresses[] = {
    NJU72343::CHIP_ADR0,
    NJU72343::CHIP_ADR1,
};

enum class SignalType : uint8_t {
    Ssg,         // SSG signal
    FmL,         // FM Left channel signal
    FmR,         // FM Right channel signal
    LineMix,     // LINE_MIX (G1) for analog mix bus
    LineSample,  // LINE_SAMPLE (H1) for A/D input path
};

struct SignalConnection {
    uint8_t     chip_addr;  // NJU72343::CHIP_ADR0 or NJU72343::CHIP_ADR1
    uint8_t     channel;    // CH A=0, B=1, C=2, D=3, E=4, F=5, G=6, H=7
    SignalType  signal;
    uint8_t     dock;       // Destination dock: 0-3
};

constexpr uint8_t kNoDock = 0xff;

constexpr SignalConnection kSignalConnections[] = {
    {NJU72343::CHIP_ADR0, 0, SignalType::Ssg,        0},
    {NJU72343::CHIP_ADR0, 1, SignalType::Ssg,        1},
    {NJU72343::CHIP_ADR0, 2, SignalType::FmL,        0},
    {NJU72343::CHIP_ADR0, 3, SignalType::FmL,        2},
    {NJU72343::CHIP_ADR0, 4, SignalType::FmL,        1},
    {NJU72343::CHIP_ADR0, 5, SignalType::FmL,        3},
    {NJU72343::CHIP_ADR0, 6, SignalType::LineMix,    kNoDock},
    {NJU72343::CHIP_ADR0, 7, SignalType::LineSample, kNoDock},
    {NJU72343::CHIP_ADR1, 0, SignalType::Ssg,        2},
    {NJU72343::CHIP_ADR1, 1, SignalType::Ssg,        3},
    {NJU72343::CHIP_ADR1, 2, SignalType::FmR,        0},
    {NJU72343::CHIP_ADR1, 3, SignalType::FmR,        2},
    {NJU72343::CHIP_ADR1, 4, SignalType::FmR,        1},
    {NJU72343::CHIP_ADR1, 5, SignalType::FmR,        3},
    {NJU72343::CHIP_ADR1, 6, SignalType::LineMix,    kNoDock},
    {NJU72343::CHIP_ADR1, 7, SignalType::LineSample, kNoDock},
};

size_t ChipIndex(uint8_t chip_addr) {
    return (chip_addr == NJU72343::CHIP_ADR1) ? 1 : 0;
}

bool IsLineSignal(SignalType signal) {
    return signal == SignalType::LineMix || signal == SignalType::LineSample;
}

int16_t ClampDbX2(int16_t db_x2) {
    if (db_x2 < kMinDbX2) {
        return kMinDbX2;
    }
    if (db_x2 > kMaxDbX2) {
        return kMaxDbX2;
    }
    return db_x2;
}

int16_t RoundDbToX2(float db) {
    const float scaled = db * 2.0f;
    const int16_t rounded = static_cast<int16_t>((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
    return ClampDbX2(rounded);
}

uint8_t DbToRegister(float db) {
    return static_cast<uint8_t>(0x40 - RoundDbToX2(db));
}

template <typename Fn>
void ForSignal(SignalType signal, Fn&& fn) {
    for (const auto& connection : kSignalConnections) {
        if (connection.signal == signal) {
            fn(connection);
        }
    }
}

}  // namespace

VolumeController& VolumeController::GetInstance() {
    static VolumeController instance;
    return instance;
}

void VolumeController::InitializeEarlyMute() {
    EnsureInitialized();
    SetZeroCrossDetection(true);
    MuteFmSsg();
    MuteLineIn();
}

void VolumeController::SetDockModuleTypes(const DockModuleTypes& types) {
    dock_module_types_ = types;
}

void VolumeController::MuteFmSsg() {
    EnsureInitialized();
    for (const auto& connection : kSignalConnections) {
        if (!IsLineSignal(connection.signal)) {
            SetChannelMute(connection.chip_addr, connection.channel);
        }
    }
}

void VolumeController::MuteLineIn() {
    MuteLineMix();
    MuteLineSample();
}

void VolumeController::MuteLineMix() {
    EnsureInitialized();
    ForSignal(SignalType::LineMix, [this](const SignalConnection& connection) {
        SetChannelMute(connection.chip_addr, connection.channel);
    });
}

void VolumeController::MuteLineSample() {
    EnsureInitialized();
    ForSignal(SignalType::LineSample, [this](const SignalConnection& connection) {
        SetChannelMute(connection.chip_addr, connection.channel);
    });
}

void VolumeController::SetFmSsgVolumeDb(float db) {
    EnsureInitialized();
    for (const auto& connection : kSignalConnections) {
        if (IsLineSignal(connection.signal)) {
            continue;
        }

        const auto module_type = dock_module_types_[connection.dock];
        const bool available = (module_type == DockModuleType::YM2608) ||
                               (module_type == DockModuleType::YM2203 && connection.signal != SignalType::FmR);
        if (available) {
            SetChannelVolumeDb(connection.chip_addr, connection.channel, db);
        } else {
            SetChannelMute(connection.chip_addr, connection.channel);
        }
    }
}

void VolumeController::SetLineMixVolumeDb(float db) {
    EnsureInitialized();
    ForSignal(SignalType::LineMix, [this, db](const SignalConnection& connection) {
        SetChannelVolumeDb(connection.chip_addr, connection.channel, db);
    });
}

void VolumeController::SetLineSampleVolumeDb(float db) {
    EnsureInitialized();
    ForSignal(SignalType::LineSample, [this, db](const SignalConnection& connection) {
        SetChannelVolumeDb(connection.chip_addr, connection.channel, db);
    });
}

void VolumeController::SetVolumeRaw(uint8_t chip_addr, uint8_t channel, uint8_t value) {
    EnsureInitialized();
    nju_.send(chip_addr, channel, value);
    UpdateShadowFromRaw(chip_addr, channel, value);
}

void VolumeController::SetZeroCrossDetection(bool enabled) {
    EnsureInitialized();
    const uint8_t value = enabled ? kControlG1H1ZeroCrossOn : kControlG1H1ZeroCrossOff;
    for (uint8_t chip : kChipAddresses) {
        nju_.send(chip, 0x09, value);
    }
}

void VolumeController::EnsureInitialized() {
    if (initialized_) {
        return;
    }

    nju_.init(kClockFreqKHz, kDataPin, kClockPin, pio1);
    initialized_ = true;
}

void VolumeController::SetChannelMute(uint8_t chip_addr, uint8_t channel) {
    nju_.send(chip_addr, channel, kMuteValue);
    UpdateShadowFromRaw(chip_addr, channel, kMuteValue);
}

void VolumeController::SetChannelVolumeDb(uint8_t chip_addr, uint8_t channel, float db) {
    const uint8_t value = DbToRegister(db);
    nju_.send(chip_addr, channel, value);
    UpdateShadowFromRaw(chip_addr, channel, value);
}

void VolumeController::UpdateShadowFromRaw(uint8_t chip_addr, uint8_t channel, uint8_t value) {
    if (channel >= kChannelCount) {
        return;
    }

    auto& entry = volume_table_[ChipIndex(chip_addr)][channel];
    if (value == 0x00 || value == 0xff) {
        entry.muted = true;
        entry.db_x2 = 0;
        return;
    }
    entry.muted = false;
    entry.db_x2 = static_cast<int16_t>(0x40) - static_cast<int16_t>(value);
}

}  // namespace Platform
