# app ドメイン

アプリケーションレイヤ（`src/app/`）。エントリポイント、FreeRTOS タスク、Core 間 IPC、デバッガを持つ。ハードウェアは直接操作せず、`Platform::*` と `synth` の API のみ使用する。

関連設計書: [design_concurrency.md](../design_concurrency.md)、[design_midi_message.md](../design_midi_message.md)、[design_csm_frame.md](../design_csm_frame.md)

## タスクと IPC

app ドメインの中心はクラスではなく、タスク関数と IPC モジュールである。

```mermaid
classDiagram
    class main {
        <<entry point>>
        +main() int
        Platform init / Module detect
        MidiFactory / MidiProcessor setup
        MidiIpcInitialize / CsmIpcInitialize
        xTaskCreateAffinitySet × 5
        vTaskStartScheduler
    }

    class midi_ipc {
        <<module>>
        +gMidiNoteQueue : QueueHandle_t
        +gMidiEventQueue : QueueHandle_t
        +gMidiControlQueue : QueueHandle_t
        +gPanelChannelBitmap : volatile uint16_t
        +gLastNoteOnBitmap : volatile uint16_t
        +gPendingReset : atomic~bool~
        +MidiIpcInitialize() bool
        +MidiIpcSendMidiEvent(event) bool
        +MidiIpcSendMidiNoteEvent(event) bool
        +MidiIpcSendMidiControl(event) bool
        +MidiIpcDrainPendingNoteOffs(fn, ctx) size_t
        +MidiIpcGetStats() MidiIpcStats
    }

    class csm_ipc {
        <<module>>
        +gCsmEventQueue : QueueHandle_t
        +CsmIpcInitialize()
        +CsmIpcReceive(event, ticks) bool
        +CsmSignalFrameTick()  ISR only
        +CsmSignalStart() / CsmSignalStop()
    }

    class UsbMidiTask {
        <<task Core0>>
        TinyUSB keep-alive / Stream assemble
        MidiParser / MidiRoutingPolicy
        Enqueue
    }

    class MidiEngineTask {
        <<task Core1>>
        Batch drain queues
        MidiProcessor::Exec
        Periodic TickVibrato
    }

    class MidiPanelTask {
        <<task Core0>>
        MIDI_PANEL_PERIOD_MS period
        MidiPanelController::Tick
        Update gPanelChannelBitmap
    }

    class CsmFrameTask {
        <<task Core1>>
        Wait on gCsmEventQueue
        CsmVoice Start/UpdateFrame/Stop
    }

    class DebugTask {
        <<task Core0>>
        Debugger console
    }

    class Debugger {
        <<module>>
        +HandleSysEx(data, len)
        Console cmds (stats / dump / rmix / trim ...)
    }

    class config_h {
        <<constants>>
        MIDI_CHANNELS / ENABLE_CSM
        VIBRATO_* / RHYTHM_LEVEL_OFFSET
        MIDI_NOTE_BATCH_MAX ...
    }

    class task_config_h {
        <<constants>>
        TASK_PRIORITY_* / TASK_STACK_* constants
        AFFINITY_CORE0 / AFFINITY_CORE1
        MIDI_PANEL_PERIOD_MS
    }

    main ..> UsbMidiTask : creates
    main ..> MidiEngineTask : creates
    main ..> MidiPanelTask : creates
    main ..> CsmFrameTask : creates
    main ..> DebugTask : creates
    UsbMidiTask --> midi_ipc : enqueue
    MidiEngineTask --> midi_ipc : drain
    MidiPanelTask --> midi_ipc : bitmap / Reset
    CsmFrameTask --> csm_ipc : wait
    UsbMidiTask --> Debugger : SysEx
    DebugTask --> Debugger
```

| 要素 | ファイル | 責務 |
|---|---|---|
| `main` | `main.cpp` | 初期化・マスターボリューム復帰・タスク生成・スケジューラ起動 |
| `midi_ipc` | `midi_ipc.h/cpp` | MIDI 用 Core 間キュー、NoteOff 保護、統計 |
| `csm_ipc` | `csm_ipc.h/cpp` | CSM フレームイベントキューとシグナル API |
| 各タスク | `*_task.h/cpp` | [design_concurrency.md](../design_concurrency.md) のタスク構成を実装 |
| `Debugger` | `debugger.h/cpp` | 対話型デバッガ・独自 SysEx 処理 |
| `config.h` / `task_config.h` | — | 実行時ポリシー定数とタスク設定の唯一の定義元 |
