//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "FreeRTOS.h"
#include "task.h"

// ----------------------------------------------------------------------------
// Task priorities
//   configMAX_PRIORITIES = 32 (FreeRTOSConfig.h)
//   Higher number = higher priority
// ----------------------------------------------------------------------------
// CsmFrameTask は MidiEngineTask より高く（フレーム処理を MIDI に潰されない）
static constexpr UBaseType_t TASK_PRIORITY_CSM         = configMAX_PRIORITIES - 1; // 31 (Core1)
static constexpr UBaseType_t TASK_PRIORITY_MIDI_ENGINE = configMAX_PRIORITIES - 2; // 30 (Core1)
static constexpr UBaseType_t TASK_PRIORITY_USB         = configMAX_PRIORITIES - 3; // 29 (Core0)
static constexpr UBaseType_t TASK_PRIORITY_MIDI_PANEL  = configMAX_PRIORITIES - 4; // 28 (Core0)
static constexpr UBaseType_t TASK_PRIORITY_DEBUG       = 1;                        //  1 (Core0)

// ----------------------------------------------------------------------------
// Task stack sizes (in words; 1 word = 4 bytes on Cortex-M33)
// ----------------------------------------------------------------------------
static constexpr uint32_t TASK_STACK_CSM         = 512; // 2KB
static constexpr uint32_t TASK_STACK_MIDI_ENGINE = 640; // 2.5KB（MIDI + TickVibrato）
static constexpr uint32_t TASK_STACK_USB         = 768; // 3KB
static constexpr uint32_t TASK_STACK_MIDI_PANEL  = 256; // 1KB
static constexpr uint32_t TASK_STACK_DEBUG       = 384; // 1.5KB

// ----------------------------------------------------------------------------
// Core affinity masks (RP2350 dual-core SMP)
//
// After vTaskStartScheduler(): CsmFrameTask / MidiEngineTask are pinned to core 1
// and can drive the FM bus (PIO SM_WRITE). If the debugger halts only core 0 while
// core 1 still runs, /WR can continue — fix SMP halt (OpenOCD rp2350.cfg USE_CORE=SMP).
// Before the scheduler starts (e.g. SetupFmModules failure in main), those tasks do
// not exist; any /WR activity while halted there is not "MIDI in the background".
// CPU debug halt never freezes PIO; an SM only stops issuing strobes when stalled
// (e.g. SM_WRITE on pull block with an empty TX FIFO).
// ----------------------------------------------------------------------------
static constexpr UBaseType_t AFFINITY_CORE0 = (1u << 0);
static constexpr UBaseType_t AFFINITY_CORE1 = (1u << 1);

// ----------------------------------------------------------------------------
// MidiPanel fixed period
// ----------------------------------------------------------------------------
static constexpr uint32_t MIDI_PANEL_PERIOD_MS = 4;
