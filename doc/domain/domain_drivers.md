# drivers ドメイン

低レベルデバイスドライバレイヤ（`src/drivers/`）。個別 IC・バス・周辺機能を操作する再利用可能な部品を格納する。ボード固有のピン割り当て・初期化順・所有ポリシーは持たない（`platform` 側の責務）。`platform` に依存しない。

## drivers/fm — FM 音源ドライバ

```mermaid
classDiagram
    class OpnBase {
        <<abstract>>
        #dev : fm_device_t*
        +id : int
        +init()
        +fm_get_channels() int*
        +fm_set_tone(ch, no) / fm_set_algorithm(...)
        +fm_set_pitch(ch, p, oct, diff)
        +fm_turnon_key(ch, op) / fm_turnoff_key(ch)
        +fm_set_volume(ch, no, tl) / fm_set_total_level(...)
        +fm_set_output_lr(ch, lr)
        +fm_turnon_LFO(freq) / fm_turnoff_LFO()
        +rtm_turnon_key(rtm) / rtm_damp_key(rtm)
        +rtm_set_total_level(tl) / rtm_set_inst_level(rtm, tl, lr)
        +ssg_set_pitch(...) / ssg_set_volume(...)
        +set_timer_a/b(...) / set_timer_mode(mode)
        +set_fmch3_mode(mode)
        +write_port_a(data) / read_port_b()
        +read_status(a1) uint8_t
    }

    class YM2608 {
        +fm_get_channels() 6ch
        +init() : enable SCH / LFO off
        +rtm_*() : 6 rhythm instruments
        +fm_turnon_LFO / fm_set_LFO_PMS / fm_set_LFO_AMS
    }

    class YM2203 {
        +fm_get_channels() 3ch
        rtm_* / LFO are no-ops
    }

    class opn_piolib {
        <<C API>>
        +fm_bus_init(bus, pio, sm, pio_hz) int
        +fm_bus_deinit(bus)
        +fm_device_init(dev, bus, chip_id, type, clock) int
        +write_reg(dev, addr, a1, data)
        +read_status(dev, a1) uint8_t
        +read_reg(dev, addr, a1) uint8_t
        +fm_set_freq(dev, ch, block, fnum)
        +fm_set_freq_ch3(dev, slot, block, fnum)
    }

    class tone_table {
        <<data>>
        tone/tone_table.inc : GM 128 tones
    }

    OpnBase <|-- YM2608
    OpnBase <|-- YM2203
    OpnBase --> opn_piolib : bus access
    OpnBase ..> tone_table : references
```

`opn_piolib` は PIO0 上の単一ステートマシン（`fm_bus`）とスピンロックで FM バスのトランザクション境界を保証する。詳細は [piolib_spec.md](../../src/drivers/fm/opn_piolib/doc/piolib_spec.md)。

## drivers/midi_panel — MIDI パネルドライバ

```mermaid
classDiagram
    class IMidiPanelDriver {
        <<interface>>
        +IsAvailable() bool*
        +Initialize()*
        +SetLedBitmap(led_bitmap)*
        +GetSwitchBitmap() uint16_t*
        +Tick()*
        +IsMidiReset() bool*
    }

    class OpnMidiPanelDriver {
        -opn_ : OpnBase&
        -host_led_bitmap_ : uint16_t
        -switch_bitmap_ : uint16_t
        -long_press_bitmap_ : uint16_t
        -channels_[16] : debounce / toggle / long-press
        +Tick()  one column slot per call
    }

    class NullMidiPanelDriver {
        +IsAvailable() returns false
        +GetSwitchBitmap() returns 0xFFFF
    }

    class MidiPanelDriverFactory {
        +CreateMidiPanelDriver(opn) unique_ptr~IMidiPanelDriver~$
    }

    IMidiPanelDriver <|.. OpnMidiPanelDriver
    IMidiPanelDriver <|.. NullMidiPanelDriver
    MidiPanelDriverFactory ..> IMidiPanelDriver : creates
    OpnMidiPanelDriver --> OpnBase : PortA/B
```

設計は [design_midi_panel.md](../design_midi_panel.md)、ハードウェア仕様は [spec_midi_panel.md](../spec_midi_panel.md)。

## drivers/usb / drivers/storage

クラスを持たない設定・コールバック実装のみのパッケージ。

| パッケージ | ファイル | 責務 |
|---|---|---|
| `usb/` | `tusb_config.h` | TinyUSB 設定（USB MIDI デバイスクラス、OSAL モード） |
| `usb/` | `usb_descriptors.cpp` | USB ディスクリプタ（VID/PID・string） |
| `storage/` | `hw_config.c` | no-OS-FatFS 向け SD カード SPI ピン設定コールバック |

TinyUSB の `tud_task()` 呼び出しと MIDI ストリーム読み出しは `app/UsbMidiTask` の責務であり、ドライバ層は設定とディスクリプタのみを提供する。
