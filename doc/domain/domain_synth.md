# synth ドメイン

MIDI メッセージをハードウェア非依存な形で FM 音源に変換するレイヤ（`src/synth/`）。FM デバイスへのアクセスは `drivers/fm` の `OpnBase`、パネルへのアクセスは `drivers/midi_panel` の `IMidiPanelDriver` を経由する。

関連設計書: [design_voice_allocation.md](../design_voice_allocation.md)、[design_pitch_effect.md](../design_pitch_effect.md)、[design_rhythm.md](../design_rhythm.md)、[design_csm_frame.md](../design_csm_frame.md)

## チャンネル・プロセッサ

```mermaid
classDiagram
    class MidiProcessor {
        -channels : MidiChannel*[16]
        -channel_enable_bits : uint16_t
        -note_on_bits : uint16_t
        +Exec(MidiEvent) uint16_t
        +SetChannelEnable(bits)
        +GetChannelEnableBits() uint16_t
        +Reset()
    }

    class MidiFactory {
        -modules_ : OpnBase*[4]
        -channels_ : MidiChannel*[16]
        +GetChannels() MidiChannel*[16]
        +GetCsmVoice() CsmVoice*
    }

    class MidiChannel {
        <<abstract>>
        #effect : ChannelEffects
        +NoteOn(key, velocity) int*
        +NoteOff(key) int*
        +AllNoteOff()*
        +SetProgram(no)
        +SetVolume(vol) / SetExpression(val)
        +PitchBend(val) / SetModulation(val) / SetPan(val)
        +RPN_MSB/LSB(val) / NRPN_MSB/LSB(val) / DataEntry_MSB(val)
        +Hold1(val) / ResetAllController() / Reset()
        +TickVibrato(phase_ticks)
        +RefreshPitch()
        +RefreshActiveFmVolume()
        +IsActive() bool
    }

    class IVoiceReclaimable {
        <<interface>>
        +Reclaim(mid, type) Voice*
        +ReclaimAll()*
    }

    class NoteChannel {
        -activeQueue / holdQueue / freeQueue : vector~Voice~
        -bCsmVoiceMode : bool
        -lfo_ : ChannelLfoState
        +NoteOn(key, velocity) int
        +NoteOff(key) int
        +ComputeVibCents() int16_t
        +ApplyPitchToVoices(vib_cents, allow_vib_dedup)
        +TickVibrato(phase_ticks)
        +Reclaim(mid, type) Voice*
    }

    class RhythmChannel {
        -modules : vector~OpnBase*~
        -inst_module_ : int8_t[6]
        -last_exclusive_note[6] / last_exclusive_module[6]
        -last_rtm_on_module : int16_t[4]
        +MIDI_RHYTHM_CHANNEL = 9$
        +NoteOn(key, velocity) int
        +NoteOff(key) int
        +AllNoteOff()
    }

    class MidiPanelController {
        -driver_ : unique_ptr~IMidiPanelDriver~
        +IsConnected() bool
        +Tick(midi_ch_active_bitmap)
        +GetChannelEnableBitmap() uint16_t
        +IsMidiReset() bool
    }

    MidiProcessor o-- MidiChannel : 16ch
    MidiFactory ..> MidiChannel : creates
    MidiFactory ..> Voice : creates
    MidiChannel <|-- NoteChannel
    MidiChannel <|-- RhythmChannel
    IVoiceReclaimable <|.. NoteChannel
    IVoiceReclaimable <|.. RhythmChannel
    NoteChannel --> VoiceAllocator : allocate request
    RhythmChannel --> OpnBase : rtm_*
    MidiPanelController --> IMidiPanelDriver
```

## Voice・アロケータ

```mermaid
classDiagram
    class Voice {
        <<abstract>>
        -type : bool
        -midi_ch : int
        #bk_program : int32_t
        #volume / velocity / key : int
        +NoteOff()*
        +ApplyPitch(fx, vib_cents)*
        +SetPan(lr)*
        +SetProgram(no)*
        +SetVolume(vol)*
        +GetModuleId() int*
        +RefreshVolume()
    }

    class NoteVoice {
        -module : OpnBase&
        -fm_ch : uint8_t
        +NoteOn(...)
        +ApplyPitch(fx, vib_cents)
    }

    class CsmVoice {
        -modules : OpnBase*[4]
        -frame / lastFrame : int
        -running : bool
        -event_sink_ : ICsmEventSink*
        +SetEventSink(sink)
        +NoteOn(...)  event_sink_->SignalCsmStart
        +AttachIrq() / Start() / UpdateFrame(first) / Stop()
        -IrqTickThunk(ctx)$
    }

    class ICsmEventSink {
        <<interface>>
        +SignalCsmStart(note, program, volume, lr)*
        +SignalCsmStop()*
    }

    class VoiceAllocator {
        <<singleton>>
        -voice_pool : vector~Voice*~
        -reclaim_targets : vector~IVoiceReclaimableInfo~
        +GetInstance() VoiceAllocator$
        +AddVoice(voice)
        +AddReclaimTarget(channel, target)
        +AllocateVoice(channel, mid, type) Voice*
        +Reset()
    }

    class VoiceLimits {
        <<namespace>>
        kMaxFmModules = 4$
        kMaxNoteVoices = 24$
        kMaxCsmVoices = 1$
    }

    Voice <|-- NoteVoice
    Voice <|-- CsmVoice
    VoiceAllocator o-- Voice : owns
    VoiceAllocator --> IVoiceReclaimable : cross-channel reclaim
    NoteVoice --> OpnBase : fm_set_*
    CsmVoice --> OpnBase : CH3 / Timer B
    CsmVoice --> ICsmEventSink : Start/Stop（app層が実装）
```

| 要素 | ファイル | 責務 |
|---|---|---|
| `MidiProcessor` | `MidiProcessor.h/cpp` | `MidiEvent` のディスパッチ、チャンネル有効管理 |
| `MidiFactory` | `MidiFactory.h/cpp` | チャンネルと Voice の生成・接続（静的ストレージ） |
| `MidiChannel` | `channel/MidiChannel.h/cpp` | チャンネル共通の CC / RPN / NRPN ステートマシン |
| `NoteChannel` | `channel/NoteChannel.h/cpp` | メロディ発音、Voice キュー、ソフトウェア LFO |
| `RhythmChannel` | `channel/RhythmChannel.h/cpp` | ch10 リズム。Voice を使わずチップを直接操作 |
| `Voice` / `NoteVoice` / `CsmVoice` | `voice/` | 発音単位の抽象と FM / CSM 実装 |
| `ICsmEventSink` | `voice/ICsmEventSink.h` | CSM Start/Stop 送信先インターフェース（実体は app 層の `CsmEventSink`、[design_csm_frame.md](../design_csm_frame.md)） |
| `VoiceAllocator` | `voice/VoiceAllocator.h/cpp` | Voice プール所有と横断調停（シングルトン） |
| `MidiPanelController` | `MidiPanelController.h/cpp` | `IMidiPanelDriver` API の仲介 |
