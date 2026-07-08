//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdio>
#include <iterator>
#include "NoteChannel.h"
#include "NoteVoice.h"
#include "VoiceLimits.h"
#include "config.h"
#include "debugger.h"

#include "FreeRTOS.h"
#include "task.h"

namespace {

// sin(2π·i/256)·32767, Q15
static constexpr int16_t kSinLut[256] = {
         0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
      6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
     12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
     18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
     23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
     27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
     30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
     32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
     32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
     32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
     30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
     27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
     23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
     18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
     12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
      6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
         0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
     -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
    -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
    -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
    -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
    -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
     -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
};

static uint32_t VibratoCalcPhaseInc(uint8_t vbrate) {
    const float rate_hz = VIBRATO_RATE_MIN_HZ
        + (VIBRATO_RATE_MAX_HZ - VIBRATO_RATE_MIN_HZ) * (vbrate / 127.0f);
    return static_cast<uint32_t>(rate_hz * VIBRATO_DT_SEC * 16777216.0f);
}

// GM bank (0:0) の判別
static bool IsGmBk(uint8_t msb, uint8_t lsb) {
    return msb == 0 && lsb == 0;
}

#if ENABLE_CSM != 0
static bool IsCsmBkMsb(uint8_t msb) {
    return msb == 0x03;
}
#endif

// Non-GM bankの識別子（デバッグ表示用)
static const char* ClassifyBankLabel(uint8_t msb, uint8_t lsb, int midi_ch) {
#if ENABLE_CSM != 0
    if (msb == 0x03) {
        return "CSM";
    }
#endif
    if (msb == 0x78 && lsb == 0) {
        return "GM2 Melody";
    }
    if (msb == 0x7f && lsb == 0) {
        return (midi_ch == 10) ? "GM2/GS/XG Rhythm" : "GM2/GS/XG Rhythm (melody ch)";
    }
    if (msb == 0x7f) {
        return "GS/XG Rhythm Var";
    }
    if (msb == 0 && lsb != 0) {
        return "XG Variation";
    }
    if ((msb == 0x40 || msb == 0x3f) && lsb == 0) {
        return "XG SFX";  // MSB 64:0 が一般的。63:0 も一部シーケンサで使用
    }
    if (msb >= 1 && msb <= 0x1f && lsb == 0) {
        return "GS Variation";
    }
    return "Unknown";
}

}  // namespace

volatile VibOverride g_vib_override = VibOverride::Auto;

NoteChannel::NoteChannel(int no) : MidiChannel(no), bCsmVoiceMode(false) {
    allocator = &VoiceAllocator::GetInstance();
    activeQueue.reserve(VoiceLimits::kMaxVoices);
    holdQueue.reserve(VoiceLimits::kMaxVoices);
    freeQueue.reserve(VoiceLimits::kMaxVoices);
    lfo_.phase            = 0;
    lfo_.phase_inc        = VibratoCalcPhaseInc(effect.vbrate);
    last_vib_cents_sent_  = 0;
}

NoteChannel::~NoteChannel() {
}

void NoteChannel::updateLfoPhaseInc() {
    lfo_.phase_inc = VibratoCalcPhaseInc(effect.vbrate);
}

void NoteChannel::Reset() {
    MidiChannel::Reset();
    bCsmVoiceMode        = false;
    last_vib_cents_sent_ = 0;
}

void NoteChannel::moveVoice(VoiceQueue& src, VoiceQueue::iterator it, VoiceQueue& dst) {
    dst.push_back(*it);
    src.erase(it);
}

void NoteChannel::moveAllVoices(VoiceQueue& src, VoiceQueue& dst) {
    dst.insert(dst.end(), src.begin(), src.end());
    src.clear();
}

void NoteChannel::releaseHoldQueue() {
    while (!holdQueue.empty()) {
        auto it = holdQueue.begin();
        finishNoteOff(holdQueue, it);
    }
}

void NoteChannel::finishNoteOff(VoiceQueue& queue, VoiceQueue::iterator it) {
    (*it)->SetNoteOnCount(0);
    (*it)->NoteOff();
    moveVoice(queue, it, freeQueue);
}

Voice* NoteChannel::stealVoiceFromQueue(VoiceQueue& queue, int mid, bool type) {
    Voice*                 fallback    = nullptr;
    VoiceQueue::iterator   fallback_it = queue.end();

    for (auto it = queue.begin(); it != queue.end(); ++it) {
        if ((*it)->GetType() != type) {
            continue;
        }
        if (mid != -1 && (*it)->GetModuleId() == mid) {
            Voice* voice = *it;
            voice->SetNoteOnCount(0);
            voice->NoteOff();
            queue.erase(it);
            return voice;
        }
        if (fallback == nullptr) {
            fallback    = *it;
            fallback_it = it;
        }
    }
    if (fallback == nullptr) {
        return nullptr;
    }
    fallback->SetNoteOnCount(0);
    fallback->NoteOff();
    queue.erase(fallback_it);
    return fallback;
}

bool NoteChannel::IsVoiceSounding(const Voice* voice) const {
    if (voice == nullptr) {
        return false;
    }
    for (const Voice* v : activeQueue) {
        if (v == voice) {
            return true;
        }
    }
    for (const Voice* v : holdQueue) {
        if (v == voice) {
            return true;
        }
    }
    return false;
}

Voice* NoteChannel::getFreeVoice(int mid, bool type, bool fromFirst) {
    // 最近使ったmoduleに属するVoiceを優先的に探す。
    // NoteVoiceをチャンネル内で再利用する場合は末尾から、解放する場合は先頭から探す。
    // CsmVoiceは頻度が少なく先頭に滞留する傾向がある前提で先頭から探す。

    if (freeQueue.empty()) {
        return nullptr;
    }

    Voice* voice = nullptr;
    if (type == true) {
        // CsmVoiceを先頭から探す
        for (auto it = freeQueue.begin(); it != freeQueue.end(); ++it) {
            if ((*it)->GetType() == type) {
                voice = *it;
                freeQueue.erase(it);
                return voice;
            }
        }
    } else if (fromFirst) {
        // 最近使ったmoduleに属するNoteVoiceを先頭から探す
        auto it        = freeQueue.begin();
        auto end       = freeQueue.end();
        auto candidate = it;
        for (; it != end; ++it) {
            if ((*it)->GetType() == type) {
                if (voice == nullptr) {
                    candidate = it;  // 先頭に最も近いVoice候補
                }
                voice = *it;  // イテレータが失効する前にVoiceを保存
                if (mid == -1 || voice->GetModuleId() == mid) {
                    freeQueue.erase(it);
                    return voice;
                }
            }
        }
        // なければ先頭に最も近い候補から取り出す
        if (voice) {
            voice = *candidate;
            freeQueue.erase(candidate);
            return voice;
        }
    } else {
        // 最近使ったmoduleに属するNoteVoiceを末尾から探す
        auto it        = freeQueue.rbegin();
        auto end       = freeQueue.rend();
        auto candidate = it;
        for (; it != end; ++it) {
            if ((*it)->GetType() == type) {
                if (voice == nullptr) {
                    candidate = it;  // 末尾に最も近いVoice候補
                }
                voice = *it;  // イテレータが失効する前にVoiceを保存
                if (mid == -1 || voice->GetModuleId() == mid) {
                    freeQueue.erase(std::next(it).base());
                    return voice;
                }
            }
        }
        // なければ末尾に最も近い候補から取り出す
        if (voice) {
            voice = *candidate;
            freeQueue.erase(std::next(candidate).base());
            return voice;
        }
    }
    // 再利用可能なVoiceが存在しない
    return nullptr;
}

Voice* NoteChannel::Reclaim(int mid, bool type) {
    Voice* voice = getFreeVoice(mid, type, true);
    if (voice == nullptr) {
        voice = stealVoiceFromQueue(holdQueue, mid, type);
    }
    if (voice == nullptr) {
        voice = stealVoiceFromQueue(activeQueue, mid, type);
    }
    if (voice) {
        ++rel_success_count;
    } else {
        ++rel_fail_count;
    }
    return voice;
}

void NoteChannel::ReclaimAll() {
    activeQueue.clear();
    holdQueue.clear();
    freeQueue.clear();
}

void NoteChannel::BankSelect_MSB(uint8_t val) {
    bk_rx_msb  = val;
    bk_stg_msb = val;
}

void NoteChannel::BankSelect_LSB(uint8_t val) {
    bk_rx_lsb = val;
#if ENABLE_CSM != 0
    if (IsCsmBkMsb(bk_stg_msb)) {
        bCsmVoiceMode = true;
        bk_program    = (bk_program & 0xffff) | (static_cast<int32_t>(0x03) << 24) |
                     (static_cast<int32_t>(val & 0x7f) << 16);
        return;
    }
    bCsmVoiceMode = false;
#endif
    // 非CSM bankは GM0:0 に正規化（program 部は維持）
    bk_program &= 0xffff;
}

void NoteChannel::SetProgram(uint8_t no) {
#if ENABLE_CSM != 0
    if (!IsGmBk(bk_rx_msb, bk_rx_lsb) && !IsCsmBkMsb(bk_rx_msb)) {
#else
    if (!IsGmBk(bk_rx_msb, bk_rx_lsb)) {
#endif
        DPRINTF(1, "Non-GM: MIDI ch=%2d bank=%3u:%3u program=%u [%s]\n", channel + 1,
                    static_cast<unsigned>(bk_rx_msb), static_cast<unsigned>(bk_rx_lsb),
                    static_cast<unsigned>(no), ClassifyBankLabel(bk_rx_msb, bk_rx_lsb, channel + 1));
    }
#if ENABLE_CSM != 0
    if (bCsmVoiceMode) {
        bk_program = (bk_program & 0xffff0000) | no;
        return;
    }
#endif
    bk_program = no;
}

void NoteChannel::SetVolume(int vol) {
    volume = vol;
    for (auto& voice : activeQueue) {
        voice->SetVolume(EffectiveVolume(voice->GetVelocity()));
    }
    for (auto& voice : holdQueue) {
        voice->SetVolume(EffectiveVolume(voice->GetVelocity()));
    }
}

void NoteChannel::SetExpression(int val) {
    expression = val;
    for (auto& voice : activeQueue) {
        voice->SetVolume(EffectiveVolume(voice->GetVelocity()));
    }
    for (auto& voice : holdQueue) {
        voice->SetVolume(EffectiveVolume(voice->GetVelocity()));
    }
}

void NoteChannel::ResetAllController() {
    releaseHoldQueue();
    MidiChannel::ResetAllController();
    lfo_.phase           = 0;
    last_vib_cents_sent_ = 0;
    updateLfoPhaseInc();
    for (auto& voice : activeQueue) {
        voice->SetVolume(EffectiveVolume(voice->GetVelocity()));
        voice->SetPan(outputLR);
    }
    for (auto& voice : holdQueue) {
        voice->SetVolume(EffectiveVolume(voice->GetVelocity()));
        voice->SetPan(outputLR);
    }
    ApplyPitchToVoices(0);
}

int16_t NoteChannel::ComputeVibCents() const {
    const uint8_t depth = EffectiveVbdepth(effect.vbdepth);
    if (depth == 0) {
        return 0;
    }
    const uint32_t index = (lfo_.phase >> 24) & 0xFFu;
    const int32_t peak_cents =
        (static_cast<int32_t>(depth) * VIBRATO_DEPTH_MAX_CENTS) / 127;
    return static_cast<int16_t>((peak_cents * kSinLut[index]) >> 15);
}

bool NoteChannel::IsVoiceInAttackDelay(const Voice* voice) const {
#if VIBRATO_ATTACK_DELAY_MS <= 0
    (void)voice;
    return false;
#else
    const TickType_t elapsed_ticks =
        xTaskGetTickCount() - static_cast<TickType_t>(voice->GetPitchAttackStartTick());
    return (elapsed_ticks * portTICK_PERIOD_MS) <
           static_cast<TickType_t>(VIBRATO_ATTACK_DELAY_MS);
#endif
}

bool NoteChannel::IsVoiceInVibratoFmDelay(const Voice* voice) const {
    const uint32_t delay_ms = VibratoFmStartDelayMs(voice);
    if (delay_ms == 0) {
        return false;
    }
    const TickType_t elapsed_ticks =
        xTaskGetTickCount() - static_cast<TickType_t>(voice->GetPitchAttackStartTick());
    return (elapsed_ticks * portTICK_PERIOD_MS) < static_cast<TickType_t>(delay_ms);
}

uint32_t NoteChannel::VibratoFmStartDelayMs(const Voice* voice) const {
    uint32_t delay_ms = static_cast<uint32_t>(VIBRATO_ATTACK_DELAY_MS);
#if VIBRATO_HIGH_NOTE_EXTRA_MS > 0
    const int key = const_cast<Voice*>(voice)->GetKey();
    if (key >= VIBRATO_HIGH_NOTE_KEY) {
        delay_ms += static_cast<uint32_t>(VIBRATO_HIGH_NOTE_EXTRA_MS);
    }
#if VIBRATO_HIGH_MIN_SOUNDING_MS > 0
    if (key >= VIBRATO_HIGH_NOTE_KEY &&
        delay_ms < static_cast<uint32_t>(VIBRATO_HIGH_MIN_SOUNDING_MS)) {
        delay_ms = static_cast<uint32_t>(VIBRATO_HIGH_MIN_SOUNDING_MS);
    }
#endif
#else
    (void)voice;
#endif
#if VIBRATO_MIN_SOUNDING_MS > 0
    if (delay_ms < static_cast<uint32_t>(VIBRATO_MIN_SOUNDING_MS)) {
        delay_ms = static_cast<uint32_t>(VIBRATO_MIN_SOUNDING_MS);
    }
#endif
    return delay_ms;
}

bool NoteChannel::ShouldAdvanceLfoPhase() const {
#if VIBRATO_ATTACK_DELAY_MS <= 0 && VIBRATO_MIN_SOUNDING_MS <= 0
    return true;
#else
    bool has_sounding = false;
    for (auto& voice : activeQueue) {
        has_sounding = true;
        const TickType_t elapsed_ticks =
            xTaskGetTickCount() - static_cast<TickType_t>(voice->GetPitchAttackStartTick());
        const uint32_t elapsed_ms = elapsed_ticks * portTICK_PERIOD_MS;
        if (elapsed_ms >= VibratoFmStartDelayMs(voice)) {
            return true;
        }
    }
    for (auto& voice : holdQueue) {
        has_sounding = true;
        const TickType_t elapsed_ticks =
            xTaskGetTickCount() - static_cast<TickType_t>(voice->GetPitchAttackStartTick());
        const uint32_t elapsed_ms = elapsed_ticks * portTICK_PERIOD_MS;
        if (elapsed_ms >= VibratoFmStartDelayMs(voice)) {
            return true;
        }
    }
    return !has_sounding;
#endif
}

void NoteChannel::ApplyPitchToVoices(int16_t vib_cents, bool skip_attack_voices) {
    for (auto& voice : activeQueue) {
        if (skip_attack_voices) {
            if (IsVoiceInVibratoFmDelay(voice)) {
                continue;
            }
            voice->ApplyPitch(effect, vib_cents, true);
            continue;
        }
        if (IsVoiceInAttackDelay(voice)) {
            voice->ApplyPitch(effect, 0);
            continue;
        }
        voice->ApplyPitch(effect, vib_cents);
    }
    for (auto& voice : holdQueue) {
        if (skip_attack_voices) {
            if (IsVoiceInVibratoFmDelay(voice)) {
                continue;
            }
            voice->ApplyPitch(effect, vib_cents, true);
            continue;
        }
        if (IsVoiceInAttackDelay(voice)) {
            voice->ApplyPitch(effect, 0);
            continue;
        }
        voice->ApplyPitch(effect, vib_cents);
    }
}

void NoteChannel::TickVibrato(uint32_t phase_ticks) {
    if (EffectiveVbdepth(effect.vbdepth) == 0 || !IsActive()) {
        return;
    }
    if (phase_ticks == 0) {
        phase_ticks = 1;
    }
    if (ShouldAdvanceLfoPhase()) {
        lfo_.phase += lfo_.phase_inc * phase_ticks;
    }
    const int16_t vib_cents = ComputeVibCents();
    ApplyPitchToVoices(vib_cents, true);
    last_vib_cents_sent_ = vib_cents;
}

int NoteChannel::NoteOn(int key, int velocity) {
    // Velocity=0なのでNoteOff処理
    if (velocity == 0) {
        return NoteOff(key);
    }

#if ENABLE_CSM != 0
    if (bCsmVoiceMode) {
        for (auto& voice : activeQueue) {
            if (voice->GetType() &&
                voice->TryRetrigger(key, bk_program, EffectiveVolume(velocity), effect, outputLR)) {
                voice->SetVelocity(velocity);
                DPRINTF(3, " C%02d ", voice->id);
                return 1;
            }
        }

        Voice* voice = getFreeVoice(-1, true, false);
        if (voice == nullptr) {
            voice = allocator->AllocateVoice(channel, -1, true);
            if (voice == nullptr) {
                ++rel_fail_count;
                DPRINTF(3, "!!!!!!!!!!");
                return -1;
            }
            DPRINTF(3, " N%02d ", voice->id);
        } else {
            DPRINTF(3, " F%02d ", voice->id);
        }

        voice->SetVelocity(velocity);
        voice->NoteOn(key, bk_program, EffectiveVolume(velocity), effect, outputLR);
        activeQueue.push_back(voice);
        return 1;
    }
#endif

    // D1: チャンネル無音（active + hold が空）からの Note On のみ位相リセット。
    // 和音中の新音で裏旋律など既存 Voice のビブラート位相を乱さない。
    if (activeQueue.empty() && holdQueue.empty()) {
        lfo_.phase           = 0;
        last_vib_cents_sent_ = 0;
    }

    int mid = -1;  // 最近使ったmoduleが不明

    // holdQueue内の同一keyのVoiceを探して再利用 (TryRetrigger)
    for (auto it = holdQueue.begin(); it != holdQueue.end(); ++it) {
        if ((*it)->GetKey() == key) {
            (*it)->MarkPitchAttackStart();
            if (!(*it)->TryRetrigger(key, bk_program, EffectiveVolume(velocity), effect,
                                     outputLR)) {
                continue;
            }
            // TryRetrigger成功時のみactiveQueueへ移動
            (*it)->SetVelocity(velocity);
            DPRINTF(3, " H%02d ", (*it)->id);
            moveVoice(holdQueue, it, activeQueue);  // activeQueueに移動
            last_vib_cents_sent_ = ComputeVibCents();
            ApplyPitchToVoices(last_vib_cents_sent_);
            return 1;
        }
        mid = (*it)->GetModuleId();  // 最近使ったmodule
    }
    // activeQueue内の同一keyのVoiceを探して再利用
    for (auto& voice : activeQueue) {
        if (voice->GetKey() == key) {
            voice->MarkPitchAttackStart();
            if (!voice->TryRetrigger(key, bk_program, EffectiveVolume(velocity), effect,
                                     outputLR)) {
                mid = voice->GetModuleId();
                continue;
            }
            // TryRetrigger成功時のみ再利用
            voice->SetVelocity(velocity);
            DPRINTF(3, " A%02d ", voice->id);
            last_vib_cents_sent_ = ComputeVibCents();
            ApplyPitchToVoices(last_vib_cents_sent_);
            return 1;
        }
        mid = voice->GetModuleId();  // 最近使ったmodule
    }

    // freeQueue内のVoiceを再利用
    //   なるべく最近使ったものから探す
    Voice* voice = getFreeVoice(mid, bCsmVoiceMode, false);
    if (voice == nullptr) {
        // 使用可能なVoiceがないので新規にAllocate
        voice = allocator->AllocateVoice(channel, mid, bCsmVoiceMode);
        if (voice == nullptr) {
            // AllocateできなかったのでNoteOn失敗
            ++rel_fail_count;
            DPRINTF(3, "!!!!!!!!!!");
            return -1;
        }
        // 新規にAllocateできた
        DPRINTF(3, " N%02d ", voice->id);
    } else {
        // 再利用
        DPRINTF(3, " F%02d ", voice->id);
    }
    // 新規にAllocateしたVoiceをActiveキューに追加
    voice->SetVelocity(velocity);
    voice->MarkPitchAttackStart();
    voice->NoteOn(key, bk_program, EffectiveVolume(velocity), effect, outputLR);
    activeQueue.push_back(voice);
    last_vib_cents_sent_ = ComputeVibCents();
    ApplyPitchToVoices(last_vib_cents_sent_);

    return 1;
}

int NoteChannel::NoteOff(int key) {
    for (auto it = activeQueue.begin(); it != activeQueue.end();) {
        if ((*it)->GetKey() == key) {
            if (hold1 == false) {
                DPRINTF(3, " -%02d ", (*it)->id);
                finishNoteOff(activeQueue, it);
            } else {
                // Hold状態なのでNoteOffを保留する
                DPRINTF(3, " H%02d ", (*it)->id);
                moveVoice(activeQueue, it, holdQueue);
            }
            return 0;
        }
        ++it;
    }

    if (!hold1) {
        for (auto it = holdQueue.begin(); it != holdQueue.end();) {
            if ((*it)->GetKey() == key) {
                DPRINTF(3, " -%02d ", (*it)->id);
                finishNoteOff(holdQueue, it);
                return 0;
            }
            ++it;
        }
    }

    DPRINTF(3, " -?? ");
    return -1;
}

void NoteChannel::AllNoteOff() {
    Hold1(0);
    while (!activeQueue.empty()) {
        auto it = activeQueue.begin();
        finishNoteOff(activeQueue, it);
    }
    for (auto& voice : freeQueue) {
        voice->SetNoteOnCount(0);
        if (!voice->GetType()) {
            static_cast<NoteVoice*>(voice)->NoteOff();
        } else {
            voice->NoteOff();
        }
    }
}

void NoteChannel::Hold1(int val) {
    if (val >= 64) {
        hold1 = true;
    } else {
        hold1 = false;
        releaseHoldQueue();
    }
}

void NoteChannel::DataEntry_MSB(uint8_t val) {
    bool pitch_changed = false;
    if (data_entry_addr_ == DataEntryAddr::Rpn && rpn_msb == 0) {
        if (rpn_lsb == 0) {
            // PitchBend Sensitivity (LSBは使用しない)
            effect.pbs = val;
            pitch_changed = true;
        } else if (rpn_lsb == 2) {
#if ENABLE_COARSE_TUNE == 1
            // Master Coarse Tuning (LSBは使用しない)
            effect.coarse_tune = static_cast<int>(val) - 64;
            pitch_changed = true;
#endif
        }
    } else if (data_entry_addr_ == DataEntryAddr::Nrpn && nrpn_msb == 1) {
        if (nrpn_lsb == 8) {
            // Vibrato rate (XG: 00H-40H-7FH = -64..0..+63 の相対値、64=変更なし)
            // プリセットレートを中央値(64)とみなし val をそのままレートに使う
            effect.vbrate = val;
            updateLfoPhaseInc();
        } else if (nrpn_lsb == 9) {
            // Vibrato depth (XG: 00H-40H-7FH = -64..0..+63 の相対値、64=変更なし)
            // 音色プリセットのビブラートを持たない(深さ0相当)ため、
            // 64以下は深さ0、65以上は超過分(+1..+63)を 0..126 へスケールする
            effect.vbdepth = (val > 64) ? static_cast<uint8_t>((val - 64) * 2) : 0;
            if (EffectiveVbdepth(effect.vbdepth) == 0) {
                last_vib_cents_sent_ = 0;
                ApplyPitchToVoices(0);
            } else {
                last_vib_cents_sent_ = ComputeVibCents();
                ApplyPitchToVoices(last_vib_cents_sent_);
            }
        }
    }
    if (pitch_changed) {
        last_vib_cents_sent_ = ComputeVibCents();
        ApplyPitchToVoices(last_vib_cents_sent_);
    }
}

void NoteChannel::PitchBend(int16_t val) {
    if (effect.pbv != val) {  // 変化があった時のみ適用
        effect.pbv = val;
        last_vib_cents_sent_ = ComputeVibCents();
        ApplyPitchToVoices(last_vib_cents_sent_);
    }
}

void NoteChannel::SetModulation(uint8_t val) {
    if (effect.vbdepth != val) {  // 変化があった時のみ適用
        effect.vbdepth = val;
        if (EffectiveVbdepth(effect.vbdepth) == 0) {
            last_vib_cents_sent_ = 0;
            ApplyPitchToVoices(0);
        } else {
            last_vib_cents_sent_ = ComputeVibCents();
            ApplyPitchToVoices(last_vib_cents_sent_);
        }
    }
}

void NoteChannel::SetPan(uint8_t val) {
    if (pan != val) {
        if (val < 43) {
            outputLR = MidiChannel::Output::L;
        } else if (val > 85) {
            outputLR = MidiChannel::Output::R;
        } else {
            outputLR = MidiChannel::Output::LR;
        }
        for (auto& voice : activeQueue) {
            voice->SetPan(outputLR);
        }
        for (auto& voice : holdQueue) {
            voice->SetPan(outputLR);
        }
        pan = val;
    }
}

// Debug
void NoteChannel::dump() {
    MidiChannel::dump();
    std::printf("  TYPE=%s\n", bCsmVoiceMode ? "CSM" : "Note");
    std::printf("  activeQ=%2zu :", activeQueue.size());
    for (auto& voice : activeQueue) {
        std::printf(" %2d", voice->id);
    }
    std::printf("\n    holdQ=%2zu :", holdQueue.size());
    for (auto& voice : holdQueue) {
        std::printf(" %2d", voice->id);
    }
    std::printf("\n    freeQ=%2zu :", freeQueue.size());
    for (auto& voice : freeQueue) {
        std::printf(" %2d", voice->id);
    }
    std::printf("\n");
}


