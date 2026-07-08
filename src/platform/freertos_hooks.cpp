//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//

// FreeRTOS hook functions required by FreeRTOSConfig.h
//   configCHECK_FOR_STACK_OVERFLOW = 2
//   configUSE_MALLOC_FAILED_HOOK   = 1
//
#include "FreeRTOS.h"
#include "task.h"
#include <cstdio>

extern "C" {

void vApplicationStackOverflowHook(TaskHandle_t /*xTask*/, char* pcTaskName) {
    std::printf("STACK OVERFLOW in task: %s\n", pcTaskName);
    for (;;);
}

void vApplicationMallocFailedHook() {
    for (;;);
}

}  // extern "C"
