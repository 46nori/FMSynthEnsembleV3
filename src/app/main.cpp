//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>
#include <array>
#include <memory>

#include "config.h"
#include "init.h"
#include "volume_controller.h"
#include "MidiFactory.h"
#include "midi_ipc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "task_config.h"

#if ENABLE_FREERTOS_SAMPLE_TASK != 1
#include "MidiPanelDriverFactory.h"
#include "MidiProcessor.h"
#include "midi_engine_task.h"
#endif

#include "MidiPanelController.h"
#include "midi_panel_task.h"
#include "usb_midi_task.h"
#include "debugger_task.h"
#include "sample_task.h"

#if ENABLE_CSM
#include "csm_ipc.h"
#include "csm_frame_task.h"
#endif

int main()
{
    // Platform全体の初期化
    Platform::Initialize();

    std::printf("\n*** FMSynthEnsembleV3 ***\n");

    // FM音源モジュールの検出・初期化・インスタンス生成
    Platform::Error err = Platform::Error::None;
    auto fm_system = Platform::SetupFmModules(&err);
    if (!fm_system) {
        switch (err) {
        case Platform::Error::BusInitFailed:
            std::printf("SetupFmModules failed: FM bus init failed.\n");
            break;
        case Platform::Error::NoModuleFound:
            std::printf("SetupFmModules failed: no FM module found.\n");
            break;
        default:
            std::printf("SetupFmModules failed: unknown error.\n");
            break;
        }
        while(1);   // FMモジュールなしでは動作しないので先に進まない
    }
    auto& modules = fm_system->modules;

    // MIDIチャンネルのインスタンスとMIDI processorの生成
    auto factory = std::make_unique<MidiFactory>(modules);
#if ENABLE_FREERTOS_SAMPLE_TASK != 1
    auto mp = std::make_unique<MidiProcessor>(factory->GetChannels());

    // MidiEngineTask（TickVibrato 含む Single Writer）
    static MidiEngineTaskContext midiEngineCtx{mp.get(), &factory->GetChannels()};
#endif

    // DebuggerTaskのコンテキスト構築
    static DebuggerTaskContext debuggerCtx{&factory->GetChannels()};

    // MIDIパネルコントローラーの生成(modules[1]に接続する想定)
    auto panelDriver = CreateMidiPanelDriver(modules[3]);
    static MidiPanelController panelController(std::move(panelDriver));
    // MidiPanelTaskのコンテキスト構築
    static MidiPanelTaskContext midiPanelCtx{&panelController};

    // IPCの初期化
    if (!MidiIpcInitialize()) {
        std::printf("MIDI IPC initialization failed.\n");
        while (1);
    }
#if ENABLE_CSM
    if (!CsmIpcInitialize()) {
        std::printf("CSM IPC initialization failed.\n");
        while (1);
    }
#endif

    // 接続されているFM/SSG系入力を0dBに設定
    Platform::VolumeController::GetInstance().SetFmSsgVolumeDb(0);

    // FreeRTOSタスクの生成
    bool ok = true;

#if ENABLE_CSM
    ok = (xTaskCreateAffinitySet(CsmFrameTask,
                                 "CsmFrame",
                                 TASK_STACK_CSM,
                                 factory->GetCsmVoice(),
                                 TASK_PRIORITY_CSM,
                                 AFFINITY_CORE1,
                                 nullptr) == pdPASS) && ok;
#endif
#if ENABLE_FREERTOS_SAMPLE_TASK != 1
    ok = (xTaskCreateAffinitySet(MidiEngineTask,
                                 "MidiEngine",
                                 TASK_STACK_MIDI_ENGINE,
                                 &midiEngineCtx,
                                 TASK_PRIORITY_MIDI_ENGINE,
                                 AFFINITY_CORE1,
                                 nullptr) == pdPASS) && ok;
#endif

#if ENABLE_FREERTOS_SAMPLE_TASK != 1
    ok = (xTaskCreateAffinitySet(MidiPanelTask,
                                 "MidiPanel",
                                 TASK_STACK_MIDI_PANEL,
                                 &midiPanelCtx,
                                 TASK_PRIORITY_MIDI_PANEL,
                                 AFFINITY_CORE0,
                                 nullptr) == pdPASS) && ok;
#endif

    ok = (xTaskCreateAffinitySet(UsbMidiTask,
                                 "Usb",
                                 TASK_STACK_USB,
                                 nullptr,
                                 TASK_PRIORITY_USB,
                                 AFFINITY_CORE0,
                                 nullptr) == pdPASS) && ok;

    ok = (xTaskCreateAffinitySet(DebuggerTask,
                                 "Debug",
                                 TASK_STACK_DEBUG,
                                 &debuggerCtx,
                                 TASK_PRIORITY_DEBUG,
                                 AFFINITY_CORE0,
                                 nullptr) == pdPASS) && ok;

#if ENABLE_FREERTOS_SAMPLE_TASK == 1
    ok = (xTaskCreateAffinitySet(FreeRtosSampleTask,
                                 "RtSample",
                                 TASK_STACK_MIDI_ENGINE,
                                 &modules,
                                 TASK_PRIORITY_MIDI_ENGINE,
                                 AFFINITY_CORE1,
                                 nullptr) == pdPASS) && ok;
#endif

    if (!ok) {
        std::printf("Task creation failed.\n");
        while (1);
    }

    // スケジューラー開始
    vTaskStartScheduler();

    std::printf("Scheduler start failed.\n");
    while (1);
}
