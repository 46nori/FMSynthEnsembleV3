//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
// MIDI パネルの音外れ診断用タスク。
// config.h の ENABLE_FREERTOS_SAMPLE_TASK を 1 にしてビルドすると、通常の
// MidiEngineTask/MidiPanelTask の代わりにこのタスクが起動する。
//
// 検証完了(全dockで音外れなしを確認): 真因は W1（アドレスライト→データライト間）の
// 不足で、W2 は 0 のままで問題ない。fm_bus.c は 0x0e/0x0f への書き込みに W1=17 のみを
// 恒久適用している。W1掃引診断（候補0-17サイクル）ではW1=3から聞こえなくなったが、
// 確率的な現象のため境界の確度は高くなく、また17はFMレジスタと同じ安全な既知値である
// ため、本番のW1はマージンを取って17のまま確定とした。これまでの検証経緯はドキュメント
// 側に記録済み。
//
// 現在の実装は「PortA書き込みのみ」と「PortB書き込みのみ」を W1=17/W2=0 で走らせる
// 回帰確認版。
//
// 段階を追加/差し替える場合は AccessStage 配列と対応する Step 関数を書き換える
#include <array>
#include <cstdio>

#include "sample_task.h"
#include "OpnBase.h"

#include "FreeRTOS.h"
#include "task.h"
#include "pico/time.h"

namespace {

// PortA/B（I/Oポート、addr 0x0e/0x0f）への高頻度アクセスは、FM書き込みとの時間的近接や
// ガード時間の有無に関わらず音外れを引き起こす一方、同じSSGアドレス空間・同じウェイト設定の
// 他レジスタ（トーン周期など）への同条件のアクセスでは一切再現しないことを確認済み。
// トリガはポートデータレジスタへのデータライトそのもの（値・方向・ピン駆動・PortA/Bの
// 別は無関係、リードは無害）であることまで確定済み。ここでは既知の再現条件を
// W1=17/W2=0 で走らせる（自ロック版APIのみ使用、Core間排他機構には一切触れない）。
constexpr uint32_t kPortSettleUs = 100;  // OpnMidiPanelDriverのsettle_usと同値
constexpr uint8_t kBlankPortA = 0x0F;
constexpr uint8_t kColumnPortA[4] = {0x0E, 0x0D, 0x0B, 0x07};

void PortAWriteOnlyStep(OpnBase* module, uint8_t& col) {
    module->write_port_a(kColumnPortA[col]);
    busy_wait_us(kPortSettleUs);
    module->write_port_a(kBlankPortA);
    col = static_cast<uint8_t>((col + 1) % 4);
}

// PortBデータレジスタ(0x0f)への書き込み。IOB=入力のままなのでラッチ書き込みのみ
void PortBWriteOnlyStep(OpnBase* module, uint8_t& col) {
    module->write_port_b(kColumnPortA[col]);
    busy_wait_us(kPortSettleUs);
    module->write_port_b(kBlankPortA);
    col = static_cast<uint8_t>((col + 1) % 4);
}

// 指定周期(period_us)で1回ずつstepを実行し、残り時間は待つ。
// period_us=4000（実機のMidiPanelTask周期相当）なら、旧テストと同じデューティ比になる。
void DutyCycledAccess(OpnBase* module, uint32_t duration_ms, uint32_t period_us,
                       void (*step)(OpnBase*, uint8_t&)) {
    uint8_t col = 0;
    const absolute_time_t deadline = make_timeout_time_ms(duration_ms);
    while (!time_reached(deadline)) {
        const absolute_time_t step_start = get_absolute_time();
        step(module, col);
        const int64_t elapsed_us = absolute_time_diff_us(step_start, get_absolute_time());
        const int64_t remain_us = static_cast<int64_t>(period_us) - elapsed_us;
        if (remain_us > 0) {
            busy_wait_us(static_cast<uint32_t>(remain_us));
        }
    }
}

constexpr uint32_t kRealisticPeriodUs = 4000;  // 実機MidiPanelTask周期相当

struct AccessStage {
    const char* label;
    void (*step)(OpnBase*, uint8_t&);
};

constexpr AccessStage kAccessStages[] = {
    {"PortA write-only (W1=17,W2=0)", PortAWriteOnlyStep},
    {"PortB write-only (W1=17,W2=0)", PortBWriteOnlyStep},
};
constexpr size_t kNumAccessStages = sizeof(kAccessStages) / sizeof(kAccessStages[0]);
constexpr uint32_t kPassesPerStage = 3;  // 各段階でドレミファソラシドを演奏する回数

}  // namespace

// 診断用: MIDIパネルもMidiEngineTaskも介さず、単一タスク・単一コア内で
// FM書き込み（ドレミファソラシド、テンポ100）とPortA書き込みのみ／PortB書き込みのみを
// 実機相当デューティ比で隣接させ、恒久対策（W1=17/W2=0）で音外れが再現しないことを
// 確認する回帰テスト。
void FreeRtosSampleTask(void* param) {
    auto* modules = static_cast<std::array<OpnBase*, 4>*>(param);
    // ドレミファソラシド（C D E F G A B C+1oct）: fm_pitch_table のインデックスに対応
    static constexpr uint8_t kScale[] = {0, 2, 4, 5, 7, 9, 11, 12};
    static constexpr uint8_t kOctave4 = 4;
    static constexpr int kToneDefault = 0;
    static constexpr int kFmCh = 0;
    static constexpr uint32_t kNoteMs = 600;    // テンポ100 (四分音符 = 60000/100 ms)
    static constexpr uint32_t kEdgeBurstMs = 20;  // キーオフ直後/ピッチ設定直後のバースト長

    // access段階を外側、dockを内側にする: 同一段階で全dockを確認してから次の段階へ進む。
    for (;;) {
        for (const AccessStage& access : kAccessStages) {
            for (size_t dock = 0; dock < modules->size(); ++dock) {
                OpnBase* module = (*modules)[dock];
                if (module == nullptr) {
                    std::printf("Panel/FM burst test: dock%zu none\n", dock);
                    continue;
                }

                std::printf("Panel/FM burst test: dock%zu FM CH%d scale start [%s]\n",
                            dock, kFmCh, access.label);
                module->set_port_direction(true, false);  // IOA=output, IOB=input
                module->write_port_a(kBlankPortA);
                module->fm_set_tone(kFmCh, kToneDefault);
                module->fm_set_output_lr(kFmCh, 0xc0);

                for (uint32_t pass = 0; pass < kPassesPerStage; ++pass) {
                    for (uint8_t note : kScale) {
                        const uint8_t pitch = note % 12;
                        const uint8_t oct = static_cast<uint8_t>(kOctave4 + note / 12);

                        module->fm_turnoff_key(kFmCh);
                        DutyCycledAccess(module, kEdgeBurstMs, kRealisticPeriodUs, access.step);
                        module->fm_set_pitch(kFmCh, pitch, oct);
                        DutyCycledAccess(module, kEdgeBurstMs, kRealisticPeriodUs, access.step);
                        module->fm_turnon_key(kFmCh);
                        DutyCycledAccess(module, kNoteMs - 2 * kEdgeBurstMs, kRealisticPeriodUs,
                                         access.step);
                    }
                }
                module->fm_turnoff_key(kFmCh);
            }
        }
    }
}
