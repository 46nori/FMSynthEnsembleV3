//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>
#include "RhythmChannel.h"
#include "YM2608.h"

volatile int8_t g_rhythm_level_offset = RHYTHM_LEVEL_OFFSET;

namespace {

uint8_t RhythmLevelWithOffset(uint8_t level, uint8_t max_reg) {
    const int adjusted = static_cast<int>(level) - g_rhythm_level_offset;
    if (adjusted <= 0) {
        return 0;
    }
    return adjusted > max_reg ? max_reg : static_cast<uint8_t>(adjusted);
}

void DampInstrumentOnAllModules(const std::vector<OpnBase*>& modules, int rtm_inst) {
    for (auto* m : modules) {
        if (m != nullptr) {
            m->rtm_damp_key(rtm_inst);
        }
    }
}

constexpr int kAllRtmInst = YM2608::RtmInst::BD | YM2608::RtmInst::SD | YM2608::RtmInst::TOP |
                            YM2608::RtmInst::HH | YM2608::RtmInst::TOM | YM2608::RtmInst::RIM;

int InstSlotIndex(int rtm_inst) {
    switch (rtm_inst) {
    case YM2608::RtmInst::BD:
        return 0;
    case YM2608::RtmInst::SD:
        return 1;
    case YM2608::RtmInst::TOP:
        return 2;
    case YM2608::RtmInst::HH:
        return 3;
    case YM2608::RtmInst::TOM:
        return 4;
    case YM2608::RtmInst::RIM:
        return 5;
    default:
        return -1;
    }
}

void DampOnModule(OpnBase* module, int rtm_inst) {
    if (module != nullptr) {
        module->rtm_damp_key(rtm_inst);
    }
}

void DampAllOnModule(OpnBase* module) {
    DampOnModule(module, kAllRtmInst);
}

static constexpr int kRtmInstList[] = {
    YM2608::RtmInst::BD,  YM2608::RtmInst::SD,  YM2608::RtmInst::TOP,
    YM2608::RtmInst::HH,  YM2608::RtmInst::TOM, YM2608::RtmInst::RIM,
};

const char* RtmInstName(int rtm_inst) {
    switch (rtm_inst) {
    case YM2608::RtmInst::BD:
        return "BD";
    case YM2608::RtmInst::SD:
        return "SD";
    case YM2608::RtmInst::TOP:
        return "TOP";
    case YM2608::RtmInst::HH:
        return "HH";
    case YM2608::RtmInst::TOM:
        return "TOM";
    case YM2608::RtmInst::RIM:
        return "RIM";
    default:
        return "?";
    }
}

// 発音チップ上の IL を対象のみ有効にし、他楽器は 0 (mute) にする。
// init_volume で全種 IL が上がっているため、SD 発音時に TOM の IL が残ると音色が置き換わって聞こえる。
void SetInstLevelsOnModule(OpnBase* module, int rtm_inst, uint8_t il) {
    if (module == nullptr) {
        return;
    }
    for (int inst : kRtmInstList) {
        const uint8_t level = (inst == rtm_inst) ? il : 0;
        module->rtm_set_inst_level(inst, level);
    }
}

// 同一チップでの発音準備。毎回 DampAll すると連打時に誤音色になることがある。
void PrepareRhythmHit(OpnBase* module, int rtm_inst, uint8_t il, int16_t prev_on_chip) {
    if (module == nullptr) {
        return;
    }
    if (prev_on_chip < 0 || prev_on_chip != rtm_inst) {
        if (prev_on_chip >= 0) {
            DampOnModule(module, prev_on_chip);
        }
        SetInstLevelsOnModule(module, rtm_inst, il);
        DampOnModule(module, rtm_inst);
    } else {
        module->rtm_set_inst_level(rtm_inst, il);
        DampOnModule(module, rtm_inst);
    }
    module->rtm_turnon_key(rtm_inst);
}

}  // namespace

//
// GM Percussion map
// YM2608のリズム音源は6種類のみなので、基本的なドラムセットを優先して割り当てる。
// 代替音が破綻しやすい小物・効果音系は無理に鳴らさない。
//
static constexpr YM2608::RtmInst percussion_map[54] = {
    // GM1
    YM2608::RtmInst::BD,    // #35(B0)   アコースティック・バスドラム(Acoustic Bass Drum)
    YM2608::RtmInst::BD,    // #36(C1)   バスドラム1(Bass Drum 1)
    YM2608::RtmInst::RIM,   // #37(C#1)  サイドスティック(Side Stick)
    YM2608::RtmInst::SD,    // #38(D1)   アコースティック・スネア(Acoustic Snare)
    YM2608::RtmInst::SD,    // #39(D#1)  手拍子(Hand Clap)
    YM2608::RtmInst::SD,    // #40(E1)   エレクトリック・スネア(Electric Snare)
    YM2608::RtmInst::TOM,   // #41(F1)   ロー・フロア・タム(Low Floor Tom)
    YM2608::RtmInst::HH,    // #42(F#1)  クローズド・ハイハット(Closed Hi-hat)
    YM2608::RtmInst::TOM,   // #43(G1)   ハイ・フロア・タム(High Floor Tom)
    YM2608::RtmInst::HH,    // #44(G#1)  ペダル・ハイハット(Pedal Hi-hat)
    YM2608::RtmInst::TOM,   // #45(A1)   ロー・タム(Low Tom)
    YM2608::RtmInst::HH,    // #46(A#1)  オープン・ハイハット(Open Hi-hat)
    YM2608::RtmInst::TOM,   // #47(B1)   ロー・ミッド・タム(Low-Mid Tom)
    YM2608::RtmInst::TOM,   // #48(C2)   ハイ・ミッド・タム(High Mid Tom)
    YM2608::RtmInst::TOP,   // #49(C#2)  クラッシュ・シンバル1(Crash Cymbal 1)
    YM2608::RtmInst::TOM,   // #50(D2)   ハイ・タム(High Tom)
    YM2608::RtmInst::TOP,   // #51(D#2)  ライド・シンバル1(Ride Cymbal 1)
    YM2608::RtmInst::TOP,   // #52(E2)   チャイニーズ・シンバル(Chinese Cymbal)
    YM2608::RtmInst::TOP,   // #53(F2)   ライド・ベル(Ride Bell)
    YM2608::RtmInst::HH,    // #54(F#2)  タンバリン(Tambourine)
    YM2608::RtmInst::TOP,   // #55(G2)   スプラッシュ・シンバル(Splash Cymbal)
    YM2608::RtmInst::NONE,  // #56(G#2)  カウベル(Cowbell)
    YM2608::RtmInst::TOP,   // #57(A2)   クラッシュ・シンバル2(Crash Cymbal 2)
    YM2608::RtmInst::NONE,  // #58(A#2)  ヴィブラ・スラップ(Vibra-slap)
    YM2608::RtmInst::TOP,   // #59(B2)   ライドシンバル2(Ride Cymbal 2)
    YM2608::RtmInst::TOM,   // #60(C3)   ハイ・ボンゴ(High Bongo)
    YM2608::RtmInst::TOM,   // #61(C#3)  ロー・ボンゴ(Low Bongo)
    YM2608::RtmInst::RIM,   // #62(D3)   ミュート・ハイ・コンガ(Mute Hi Conga)
    YM2608::RtmInst::TOM,   // #63(D#3)  オープン・ハイ・コンガ(Open Hi Conga)
    YM2608::RtmInst::TOM,   // #64(E3)   ロー・コンガ(Low Conga)
    YM2608::RtmInst::RIM,   // #65(F3)   ハイ・ティンバレ(High Timbale)
    YM2608::RtmInst::TOM,   // #66(F#3)  ロー・ティンバレ(Low Timbale)
    YM2608::RtmInst::NONE,  // #67(G3)   ハイ・アゴゴ(High Agogo)
    YM2608::RtmInst::NONE,  // #68(G#3)  ロー・アゴゴ(Low Agogo)
    YM2608::RtmInst::NONE,  // #69(A3)   カバサ(Cabasa)
    YM2608::RtmInst::NONE,  // #70(A#3)  マラカス(Maracas)
    YM2608::RtmInst::NONE,  // #71(B3)   ショート・ホイッスル(Short Whistle)
    YM2608::RtmInst::NONE,  // #72(C4)   ロング・ホイッスル(Long Whistle)
    YM2608::RtmInst::NONE,  // #73(C#4)  ショート・ギロ(Short Guiro)
    YM2608::RtmInst::NONE,  // #74(D4)   ロング・ギロ(Long Guiro)
    YM2608::RtmInst::NONE,  // #75(D#4)  クラベス(Claves)
    YM2608::RtmInst::NONE,  // #76(E4)   ハイ・ウッドブロック(Hi Wood Block)
    YM2608::RtmInst::NONE,  // #77(F4)   ロー・ウッドブロック(Low Wood Block)
    YM2608::RtmInst::NONE,  // #78(F#4)  ミュート・クイーカ(Mute Cuica)
    YM2608::RtmInst::NONE,  // #79(G4)   オープン・クイーカ(Open Cuica)
    YM2608::RtmInst::HH,    // #80(G#4)  ミュート・トライアングル(Mute Triangle)
    YM2608::RtmInst::HH,    // #81(A4)   オープン・トライアングル(Open Triangle)
    // GM2
    YM2608::RtmInst::HH,    // #82(A#4)  シェイカー(Shaker)
    YM2608::RtmInst::NONE,  // #83(B4)   ジングルベル(Jingle Bell)
    YM2608::RtmInst::NONE,  // #84(C5)   ベルツリー(Bell Tree)
    YM2608::RtmInst::NONE,  // #85(C#5)  カスタネット(Castanets)
    YM2608::RtmInst::NONE,  // #86(D5)   ミュート・スルド(Mute Surdo)
    YM2608::RtmInst::NONE,  // #87(D#5)  オープン・スルド(Open Surdo)
    YM2608::RtmInst::NONE,  // #88(E5)
};

// 排他ノート管理マップ（ノート範囲42～81）
static constexpr int exclusive_map[81 - 42 + 1] = {
    // 排他グループID:
    //   42/44/46 => 1: ハイハット クローズド/ペダル/オープン
    //   71/72    => 2: ショート/ロングホイッスル
    //   73/74    => 3: ショート/ロングギロ
    //   78/79    => 4: ミュート/オープンクイーカ
    //   80/81    => 5: ミュート/オープントライアングル
    //   上記以外  => 0: 排他なし
    /* 42-46 */ 1, 0, 1, 0, 1,
    /* 47-70 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 71-72 */ 2, 2,
    /* 73-74 */ 3, 3,
    /* 75-77 */ 0, 0, 0,
    /* 78-79 */ 4, 4,
    /* 80-81 */ 5, 5
};

//  RTL(Rhythm Total Level)音量テーブル
//  MIDI volume(x:0-127)を RTL(y:0-63)に変換する。
//          y = 63 - round(20*log10(127/x)/0.75), x=0は無音。
static constexpr uint8_t RTLvolume[128] = {
    0,  7,  15, 20, 23, 26, 28, 29, 31, 32, 34, 35, 36, 37, 37, 38,
    39, 40, 40, 41, 42, 42, 43, 43, 44, 44, 45, 45, 45, 46, 46, 47,
    47, 47, 48, 48, 48, 49, 49, 49, 50, 50, 50, 50, 51, 51, 51, 51,
    52, 52, 52, 52, 53, 53, 53, 53, 54, 54, 54, 54, 54, 55, 55, 55,
    55, 55, 55, 56, 56, 56, 56, 56, 56, 57, 57, 57, 57, 57, 57, 58,
    58, 58, 58, 58, 58, 58, 58, 59, 59, 59, 59, 59, 59, 59, 60, 60,
    60, 60, 60, 60, 60, 60, 60, 61, 61, 61, 61, 61, 61, 61, 61, 61,
    62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 63, 63, 63, 63, 63, 63,
};

//  IL(Instrument Level)音量テーブル
//  MIDI NoteOn velocity(x:0-127)を IL(y:0-31)に変換する。
//          y = 31 - round(20*log10(127/x)/0.75), x=0は無音。
static constexpr uint8_t ILvolume[128] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  3,  4,  5,  5,  6,
    7,  8,  8,  9,  10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15,
    15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19,
    20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 22, 23, 23, 23,
    23, 23, 23, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 26,
    26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31, 31,
};

RhythmChannel::RhythmChannel(std::array<OpnBase*, 4>& input_modules)
    : MidiChannel(MIDI_RHYTHM_CHANNEL) {
    // 使用可能なYM2608をmodulesに追加
    // (不本意ながらfm_get_channels()==6でOPNAを判別)
    for (auto* m : input_modules) {
        if (m != nullptr && m->fm_get_channels() == 6) {
            modules.push_back(m);
        }
    }

    SetOutputLR(LR);        // L/R両チャンネル出力に設定
    init_volume(100, 127);  // デフォルト音量
}

RhythmChannel::~RhythmChannel() {
}

void RhythmChannel::ResetRouting() {
    for (auto& note : last_exclusive_note) {
        note = -1;
    }
    for (auto& module : last_exclusive_module) {
        module = 0;
    }
    for (auto& prev : last_rtm_on_module) {
        prev = -1;
    }
    for (auto& slot : inst_module_) {
        slot = -1;
    }
    next_assign_module_ = 0;
}

void RhythmChannel::Reset() {
    MidiChannel::Reset();
    SetOutputLR(LR);
    init_volume(100, 127);
    ResetRouting();
}

uint8_t RhythmChannel::ResolvePlayModule(int rtm_inst) {
    if (modules.empty()) {
        return 0;
    }
    const int slot = InstSlotIndex(rtm_inst);
    if (slot < 0) {
        return 0;
    }
    if (inst_module_[slot] < 0) {
        int8_t chosen = -1;
        // 可能なら未使用チップ、または同じ打楽器種別が鳴っているチップを優先
        for (size_t i = 0; i < modules.size() && i < last_rtm_on_module.size(); ++i) {
            const int16_t prev = last_rtm_on_module[i];
            if (prev == -1 || prev == rtm_inst) {
                chosen = static_cast<int8_t>(i);
                break;
            }
        }
        if (chosen < 0) {
            chosen = static_cast<int8_t>(next_assign_module_);
            next_assign_module_ =
                static_cast<uint8_t>((next_assign_module_ + 1) % modules.size());
        }
        inst_module_[slot] = chosen;
    }
    return static_cast<uint8_t>(inst_module_[slot] % modules.size());
}

void RhythmChannel::RefreshRhythmLevels() {
    const int effective_volume = EffectiveVolume();
    if (effective_volume < 0) {
        return;
    }
    const uint8_t rtl = RhythmLevelWithOffset(RTLvolume[effective_volume], 63);
    for (auto* m : modules) {
        if (m != nullptr) {
            m->rtm_set_total_level(rtl);
        }
    }
}

void RhythmChannel::ResetAllController() {
    MidiChannel::ResetAllController();

    RefreshRhythmLevels();
    ResetRouting();
}

void RhythmChannel::init_volume(uint8_t rtl, uint8_t il) {
    SetVolume(rtl);
    const uint8_t il_reg = RhythmLevelWithOffset(ILvolume[il], 31);

    for (auto *m : modules) {
        if (m != nullptr) {
            m->rtm_set_inst_level(YM2608::RtmInst::BD, il_reg);
            m->rtm_set_inst_level(YM2608::RtmInst::SD, il_reg);
            m->rtm_set_inst_level(YM2608::RtmInst::TOP, il_reg);
            m->rtm_set_inst_level(YM2608::RtmInst::HH, il_reg);
            m->rtm_set_inst_level(YM2608::RtmInst::TOM, il_reg);
            m->rtm_set_inst_level(YM2608::RtmInst::RIM, il_reg);
        }
    }
}

void RhythmChannel::SetVolume(int vol) {
    if (!modules.empty() && volume != vol) {
        volume = vol;
        RefreshRhythmLevels();
    }
}

void RhythmChannel::SetExpression(int val) {
    if (!modules.empty() && expression != val) {
        expression = val;
        RefreshRhythmLevels();
    }
}

int RhythmChannel::NoteOn(int key, int velocity) {
    if (modules.empty()) {
        return -1;      // 使用可能なYM2608がない
    }

    if (key >= 35 && key < sizeof(percussion_map) / sizeof(YM2608::RtmInst) + 35) {
        YM2608::RtmInst note = percussion_map[key - 35];
        if (note != YM2608::RtmInst::NONE) {
            if (velocity == 0) {
                // GM: Note Off 相当だがリズムは減衰任せ。LED ホールドは vel>0 で延長済みのため無視。
                return 0;
            } else {
                int eff_vol = EffectiveVolume(velocity);
                if (eff_vol <= 0) {
                    eff_vol = 1;
                }

                const int rtm_inst = static_cast<int>(note);
                int       group    = 0;
                if (key >= 42 && key <= 81) {
                    group = exclusive_map[key - 42];
                }

                uint8_t play_module = ResolvePlayModule(rtm_inst);

                // 排他ノート: 同一グループの前の音を全モジュールで damp、同じ play_module で再生
                if (group > 0) {
                    const int group_index = group - 1;
                    int16_t&  last_note   = last_exclusive_note[group_index];
                    if (last_note != -1 && last_note != key) {
                        DampInstrumentOnAllModules(modules,
                                                   percussion_map[last_note - 35]);
                    }
                    if (last_note == -1) {
                        last_exclusive_module[group_index] = play_module;
                    } else {
                        play_module = last_exclusive_module[group_index];
                    }
                    last_note = key;
                }

                OpnBase* chip = modules[play_module];
                const int16_t prev_chip = (play_module < last_rtm_on_module.size())
                                              ? last_rtm_on_module[play_module]
                                              : static_cast<int16_t>(-1);

                uint8_t il = RhythmLevelWithOffset(ILvolume[eff_vol], 31);
                if (il == 0) {
                    il = 1;  // velocity>0 なら無音化しない（IL テーブル低域 + rmix 対策）
                }
                PrepareRhythmHit(chip, rtm_inst, il, prev_chip);

                if (play_module < last_rtm_on_module.size()) {
                    last_rtm_on_module[play_module] = static_cast<int16_t>(rtm_inst);
                }
                if (group > 0) {
                    last_exclusive_module[group - 1] = play_module;
                }
            }
            return 1;
        }
    }
    return -1;
}

int RhythmChannel::NoteOff(int key) {
    // GM 準拠: 個別 Note Off は音響に影響しない（シンバル等はチップ減衰で自然終了）。
    // チョークは排他グループの Note On 切替、CC#120/#123 の AllNoteOff のみ。
    (void)key;
    return 0;
}

void RhythmChannel::AllNoteOff() {
    for (auto* m : modules) {
        if (m != nullptr) {
            m->rtm_damp_key(kAllRtmInst);
        }
    }
    for (auto& prev : last_rtm_on_module) {
        prev = -1;
    }
}

Voice* RhythmChannel::Reclaim(int mid, bool type) {
    return nullptr;
}

void RhythmChannel::ReclaimAll() {
}

// Debug
void RhythmChannel::dump() {
    MidiChannel::dump();
    std::printf("  TYPE=RTM\n");
    std::printf("  OPNA modules=%zu next_assign=%u\n", modules.size(), next_assign_module_);
    for (size_t i = 0; i < modules.size(); ++i) {
        const int16_t prev =
            (i < last_rtm_on_module.size()) ? last_rtm_on_module[i] : static_cast<int16_t>(-1);
        std::printf("    mod[%zu] chip_id=%d last_rtm=0x%02x(%s)\n", i,
                    modules[i] != nullptr ? modules[i]->id : -1, (prev < 0) ? 0 : prev,
                    (prev < 0) ? "-" : RtmInstName(prev));
    }
    static const char* slot_names[kRtmInstSlots] = {"BD", "SD", "TOP", "HH", "TOM", "RIM"};
    for (int s = 0; s < kRtmInstSlots; ++s) {
        if (inst_module_[s] < 0) {
            std::printf("    bind[%s] -> (unassigned)\n", slot_names[s]);
        } else {
            std::printf("    bind[%s] -> mod[%d]\n", slot_names[s],
                        static_cast<int>(inst_module_[s]));
        }
    }
}
