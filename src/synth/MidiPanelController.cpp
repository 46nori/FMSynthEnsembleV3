//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "MidiPanelController.h"

MidiPanelController::MidiPanelController(std::unique_ptr<IMidiPanelDriver> driver)
    : driver_(std::move(driver)) {
    if (driver_ != nullptr && driver_->IsAvailable()) {
        driver_->Initialize();
    }
}

bool MidiPanelController::IsConnected() const {
    return driver_ != nullptr && driver_->IsAvailable();
}

void MidiPanelController::Tick(uint16_t midi_ch_active_bitmap) {
    if (!IsConnected()) {
        return;
    }
    driver_->SetLedBitmap(midi_ch_active_bitmap);
    driver_->Tick();
}

uint16_t MidiPanelController::GetChannelEnableBitmap() const {
    if (!IsConnected()) {
        return 0xffff;
    }
    return driver_->GetSwitchBitmap();
}

bool MidiPanelController::IsMidiReset() const {
    if (!IsConnected()) {
        return false;
    }
    return driver_->IsMidiReset();
}
