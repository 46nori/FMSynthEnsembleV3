//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <array>
#include <cstdio>

#include "sample_task.h"
#include "OpnBase.h"

#include "FreeRTOS.h"
#include "task.h"

void FreeRtosSampleTask(void* param) {
    auto* modules = static_cast<std::array<OpnBase*, 4>*>(param);
    static constexpr uint8_t kPitchA = 9;   // A
    static constexpr uint8_t kOctave4 = 4;  // A4 = 440Hz
    static constexpr int kToneDefault = 0;
    static constexpr TickType_t kNoteOnTicks = pdMS_TO_TICKS(2000);
    static constexpr TickType_t kNoteOffGapTicks = pdMS_TO_TICKS(200);

    for (;;) {
        for (size_t dock = 0; dock < modules->size(); ++dock) {
            OpnBase* module = (*modules)[dock];
            if (module == nullptr) {
                std::printf("FM direct test: dock%zu none\n", dock);
                continue;
            }

            for (int ch = 0; ch < module->fm_get_channels(); ++ch) {
                std::printf("FM direct test: dock%zu FM CH%d A4\n", dock, ch);
                module->fm_turnoff_key(ch);
                module->fm_set_tone(ch, kToneDefault);
                module->fm_set_pitch(ch, kPitchA, kOctave4);
                module->fm_set_output_lr(ch, 0xc0);
                module->fm_turnon_key(ch);
                vTaskDelay(kNoteOnTicks);

                module->fm_turnoff_key(ch);
                vTaskDelay(kNoteOffGapTicks);
            }
        }
    }
}
