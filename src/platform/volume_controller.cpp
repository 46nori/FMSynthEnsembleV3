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
constexpr int16_t kMinDbX2      = static_cast<int16_t>(VolumeController::kMinDb * 2);
constexpr int16_t kMaxDbX2      = static_cast<int16_t>(VolumeController::kMaxDb * 2);
constexpr uint8_t kControlZeroCrossOff = 0x00;  // A1/B1/G1/H1 select, ZC off
constexpr uint8_t kControlZeroCrossOn  = 0x01;  // A1/B1/G1/H1 select, ZC on

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

bool TryChipIndex(uint8_t chip_addr, size_t* chip_index) {
    if (chip_addr == NJU72343::CHIP_ADR0) {
        *chip_index = 0;
        return true;
    }
    if (chip_addr == NJU72343::CHIP_ADR1) {
        *chip_index = 1;
        return true;
    }
    return false;
}

bool IsValidChipAddress(uint8_t chip_addr) {
    size_t chip_index = 0;
    return TryChipIndex(chip_addr, &chip_index);
}

bool IsLineSignal(SignalType signal) {
    return signal == SignalType::LineMix || signal == SignalType::LineSample;
}

// YMF288 はFM・リズム・SSGをチップ内部でディジタルミックスしてFM-L/FM-Rの1系統ステレオ出力の
// み持つ（spec_opn.md参照）。SSG単独の出力はないため、モジュール側でSSG出力ピンをGNDレベルで
// 駆動しており、該当dockのSSG入力にはGNDレベルの信号が供給される（オープンではない）。
// YM2203はデフォルト設定でFM-RにFM-Lと同一信号を出力する（モジュール上でGNDレベルへ切替可能。
// design_volume_controller.md参照）。
bool IsAvailable(const SignalConnection& connection, const VolumeController::DockModuleTypes& types) {
    if (IsLineSignal(connection.signal)) {
        return true;
    }
    const auto module_type = types[connection.dock];
    return (module_type == VolumeController::DockModuleType::YM2608) ||
           (module_type == VolumeController::DockModuleType::YM2203) ||
           (module_type == VolumeController::DockModuleType::YMF288 &&
            connection.signal != SignalType::Ssg);
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
    // Clamp before the int16_t cast. Out-of-range floats (and NaN) must not
    // convert to int16_t; that conversion is undefined.
    if (!(scaled > static_cast<float>(kMinDbX2))) {
        return kMinDbX2;
    }
    if (scaled >= static_cast<float>(kMaxDbX2)) {
        return kMaxDbX2;
    }
    return static_cast<int16_t>((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
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
        if (IsAvailable(connection, dock_module_types_)) {
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
    if (!IsValidChipAddress(chip_addr) || channel >= kChannelCount) {
        return;
    }
    EnsureInitialized();
    nju_.send(chip_addr, channel, value);
    UpdateShadowFromRaw(chip_addr, channel, value);
}

void VolumeController::SetZeroCrossDetection(bool enabled) {
    EnsureInitialized();
    const uint8_t value = enabled ? kControlZeroCrossOn : kControlZeroCrossOff;
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
    if (!IsValidChipAddress(chip_addr) || channel >= kChannelCount) {
        return;
    }
    EnsureInitialized();
    nju_.send(chip_addr, channel, kMuteValue);
    UpdateShadowFromRaw(chip_addr, channel, kMuteValue);
}

void VolumeController::SetChannelVolumeDb(uint8_t chip_addr, uint8_t channel, float db) {
    if (!IsValidChipAddress(chip_addr) || channel >= kChannelCount) {
        return;
    }
    if (!IsChannelAvailable(chip_addr, channel)) {
        SetChannelMute(chip_addr, channel);
        return;
    }
    EnsureInitialized();
    const uint8_t value = DbToRegister(db);
    nju_.send(chip_addr, channel, value);
    UpdateShadowFromRaw(chip_addr, channel, value);
}

VolumeController::VolumeValue VolumeController::GetChannelVolume(uint8_t chip_addr, uint8_t channel) const {
    size_t chip_index = 0;
    if (!TryChipIndex(chip_addr, &chip_index) || channel >= kChannelCount) {
        return VolumeValue{true, 0};
    }
    return volume_table_[chip_index][channel];
}

bool VolumeController::IsChannelAvailable(uint8_t chip_addr, uint8_t channel) const {
    for (const auto& connection : kSignalConnections) {
        if (connection.chip_addr == chip_addr && connection.channel == channel) {
            return IsAvailable(connection, dock_module_types_);
        }
    }
    return false;
}

void VolumeController::UpdateShadowFromRaw(uint8_t chip_addr, uint8_t channel, uint8_t value) {
    size_t chip_index = 0;
    if (!TryChipIndex(chip_addr, &chip_index) || channel >= kChannelCount) {
        return;
    }

    auto& entry = volume_table_[chip_index][channel];
    if (value == 0x00 || value == 0xff) {
        entry.muted = true;
        entry.db_x2 = 0;
        return;
    }
    entry.muted = false;
    entry.db_x2 = static_cast<int16_t>(0x40) - static_cast<int16_t>(value);
}

}  // namespace Platform
