//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "csm_ipc.h"

#include <atomic>
#include "config.h"

QueueHandle_t gCsmEventQueue = nullptr;

namespace {

constexpr UBaseType_t kCsmEventQueueLength = 16;

#if ENABLE_CSM_START_PREEMPT != 0
std::atomic<uint32_t> gCsmGeneration{0};
std::atomic<bool> gCsmStartPending{false};
#endif

bool send_csm_event(const CsmEvent& event) {
    return gCsmEventQueue != nullptr &&
           xQueueSendToBack(gCsmEventQueue, &event, 0) == pdTRUE;
}

}  // namespace

bool CsmIpcInitialize() {
    if (gCsmEventQueue != nullptr) {
        return true;
    }
    gCsmEventQueue = xQueueCreate(kCsmEventQueueLength, sizeof(CsmEvent));
    return gCsmEventQueue != nullptr;
}

bool CsmIpcReceive(CsmEvent& event, TickType_t wait_ticks) {
    return gCsmEventQueue != nullptr &&
           xQueueReceive(gCsmEventQueue, &event, wait_ticks) == pdTRUE;
}

void CsmIpcNotifyStartProcessed(uint32_t generation) {
#if ENABLE_CSM_START_PREEMPT != 0
    if (gCsmGeneration.load(std::memory_order_acquire) == generation) {
        gCsmStartPending.store(false, std::memory_order_release);
    }
#else
    (void)generation;
#endif
}

void CsmSignalFrameTick(void) {
    BaseType_t hpw = pdFALSE;
#if ENABLE_CSM_START_PREEMPT != 0
    if (gCsmEventQueue != nullptr && !gCsmStartPending.load(std::memory_order_acquire)) {
        const CsmEvent event{
            CsmEventType::FrameTick,
            gCsmGeneration.load(std::memory_order_acquire),
            0,
            0,
            0,
            0,
        };
        (void)xQueueSendToBackFromISR(gCsmEventQueue, &event, &hpw);
    }
#else
    if (gCsmEventQueue != nullptr) {
        const CsmEvent event{CsmEventType::FrameTick, 0, 0, 0, 0, 0};
        (void)xQueueSendToBackFromISR(gCsmEventQueue, &event, &hpw);
    }
#endif
    portYIELD_FROM_ISR(hpw);
}

void CsmSignalStop(void) {
#if ENABLE_CSM_START_PREEMPT != 0
    const CsmEvent event{CsmEventType::Stop,
                         gCsmGeneration.load(std::memory_order_acquire),
                         0,
                         0,
                         0,
                         0};
#else
    const CsmEvent event{CsmEventType::Stop, 0, 0, 0, 0, 0};
#endif
    (void)send_csm_event(event);
}

void CsmSignalStart(int note, int32_t program, int volume, uint8_t lr) {
    if (gCsmEventQueue == nullptr) {
        return;
    }

#if ENABLE_CSM_START_PREEMPT != 0
    const uint32_t generation = gCsmGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    gCsmStartPending.store(true, std::memory_order_release);

    const CsmEvent event{CsmEventType::Start, generation, note, program, volume, lr};
    if (xQueueSendToFront(gCsmEventQueue, &event, 0) == pdTRUE) {
        return;
    }

    // Start is a hard boundary: discard old queued ticks/control and retry.
    (void)xQueueReset(gCsmEventQueue);
    if (xQueueSendToFront(gCsmEventQueue, &event, 0) != pdTRUE) {
        gCsmStartPending.store(false, std::memory_order_release);
    }
#else
    const CsmEvent event{CsmEventType::Start, 0, note, program, volume, lr};
    (void)send_csm_event(event);
#endif
}
