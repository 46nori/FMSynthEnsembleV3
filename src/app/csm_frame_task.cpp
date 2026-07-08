//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "csm_frame_task.h"
#include "CsmVoice.h"
#include "config.h"
#include "csm_ipc.h"

void CsmFrameTask(void* param) {
    auto* voice = static_cast<CsmVoice*>(param);
#if ENABLE_CSM_START_PREEMPT != 0
    uint32_t activeGeneration = 0;
#endif

    for (;;) {
        CsmEvent event{};
        if (!CsmIpcReceive(event, portMAX_DELAY)) {
            continue;
        }

        switch (event.type) {
        case CsmEventType::FrameTick:
#if ENABLE_CSM_START_PREEMPT != 0
            if (event.generation == activeGeneration) {
                voice->UpdateFrame(false);
            }
#else
            voice->UpdateFrame(false);
#endif
            break;
        case CsmEventType::Start:
#if ENABLE_CSM_START_PREEMPT != 0
            if (event.generation >= activeGeneration) {
                activeGeneration = event.generation;
                voice->Start(event.note, event.program, event.volume, event.lr);
                CsmIpcNotifyStartProcessed(event.generation);
            }
#else
            voice->Start(event.note, event.program, event.volume, event.lr);
#endif
            break;
        case CsmEventType::Stop:
#if ENABLE_CSM_START_PREEMPT != 0
            if (event.generation == activeGeneration) {
                voice->Stop();
            }
#else
            voice->Stop();
#endif
            break;
        }
    }
}
