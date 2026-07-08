//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include "NoteVoice.h"
#include "config.h"
#include "debugger.h"

/**
 * @brief  OPN TL(Total Level)追加減衰テーブル
 * @details MIDI volume(x:0-127)をTL step(y:127-0)に変換する。
 *          y = round(20*log10(127/x)/0.75), x=0は無音。
 */
static constexpr uint8_t opn_attenuation[128] = {
    127, 56, 48, 43, 40, 37, 35, 34, 32, 31, 29, 28, 27, 26, 26, 25,
    24,  23, 23, 22, 21, 21, 20, 20, 19, 19, 18, 18, 18, 17, 17, 16,
    16,  16, 15, 15, 15, 14, 14, 14, 13, 13, 13, 13, 12, 12, 12, 12,
    11,  11, 11, 11, 10, 10, 10, 10, 9,  9,  9,  9,  9,  8,  8,  8,
    8,   8,  8,  7,  7,  7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  5,
    5,   5,  5,  5,  5,  5,  5,  4,  4,  4,  4,  4,  4,  4,  3,  3,
    3,   3,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0};

/**
 * @brief PB・coarse tune・ビブラートを合成してピッチを設定する
 * @details PitchBend を指定しない場合は fx.pbv=0 とする。
 *  以下の理由で key == -1 のチェックは不要
 *  - NoteOn() では key をセットした後、NoteChannel が ApplyPitch を呼ぶ。
 *  - NoteChannel からは、発音中の Voice に対して ApplyPitch が呼ばれる。
 */
static constexpr int PBS_MARGIN = 2;
static constexpr uint16_t fnum[12 + PBS_MARGIN * 2] = {
    // pbs <= 2の場合の参照用
    0x0226,  // A# (-2)
    0x0247,  // B  (-1)
    // 通常使用する1オクターブ分のF-Number
    // OpnBase.hのfm_pitch_table[]と同じ
    0x0269,  // C
    0x028e,  // C#
    0x02b4,  // D
    0x02de,  // D#
    0x0309,  // E
    0x0338,  // F
    0x0369,  // F#
    0x039c,  // G
    0x03d3,  // G#
    0x040e,  // A
    0x044b,  // A#
    0x048d,  // B
    // pbs <= 2の場合の参照用
    0x04d3,  // C  (+1)
    0x051c,  // C# (+2)
};

/** @brief ビブラート偏差（セント）を F-Number 偏差に変換。
 *  PB と同じ fnum[] テーブル・索引 k を使い、100 cents = 1 半音の線形近似で求める */
static int16_t PitchCalcVibDiff(int k, int16_t vib_cents) {
    if (vib_cents == 0) {
        return 0;
    }
    if (k < PBS_MARGIN || k >= 12 + PBS_MARGIN) {
        return 0;
    }
    const int semitone = static_cast<int>(fnum[k + 1]) - static_cast<int>(fnum[k]);
    return static_cast<int16_t>(static_cast<int32_t>(semitone) * vib_cents / 100);
}

NoteVoice::NoteVoice(OpnBase& module, uint8_t ch, int id)
    : Voice(false, id),  // NoteType
      module(module),
      fm_ch(ch),
      hw_key_off_sent_(false),
      last_fm_vib_cents_(INT16_MIN) {
    SetProgram(0);   // デフォルト音色
    SetVolume(100);  // デフォルト音量
}

NoteVoice::~NoteVoice() {
}

void NoteVoice::Reset() {
    hw_key_off_sent_     = false;
    last_fm_vib_cents_   = INT16_MIN;
    // コンストラクタと同じ設定にする
    // 外部キーボードから使用するときなどのために音色をデフォルトに戻しておく
    Voice::Reset();
    SetProgram(0);
    SetVolume(100);
}

int NoteVoice::GetModuleId() {
    return module.id;
}

void NoteVoice::SetProgram(int32_t no) {
    if (bk_program != no) {
        module.fm_set_tone(fm_ch, no & 0xff);
        bk_program = no;
        volume     = -1;  // 音色TLが戻るため、同じMIDI volumeでも再適用する
        DPRINTF(4, " P%04x:%d ", no >> 16, no & 0xff);
    }
}

void NoteVoice::SetVolume(int vol) {
    if (vol < 0) return;  // volume=-1はデフォルトTLを維持（MidiChannelの初期値）
    if (volume != vol) {
        module.fm_set_volume(fm_ch, bk_program, opn_attenuation[vol]);
        volume = vol;
    }
}

void NoteVoice::RefreshVolume() {
    if (volume < 0) {
        return;
    }
    const int cached = volume;
    volume           = -1;
    SetVolume(cached);
}

void NoteVoice::NoteOn(int note, int32_t bk_program, int volume, ChannelEffects& effect,
                        uint8_t lr) {
    SetProgram(bk_program);
    SetVolume(volume);  // must be after SetProgram()
    key = note;
    ClearHardwareKeyOffSent();
    last_fm_vib_cents_ = INT16_MIN;
    module.fm_turnoff_key(fm_ch);
    // Keep at least one FM register write between KeyOff and KeyOn so the EG restart is stable.
    module.fm_set_output_lr(fm_ch, lr);
    // KeyOn 前に基準ピッチ（PB・coarse tune のみ）を設定し、立ち上がりの音程ジャンプを防ぐ
    ApplyPitch(effect, 0);
    module.fm_turnon_key(fm_ch);
    // ビブラートは NoteChannel::ApplyPitchToVoices(ComputeVibCents()) が KeyOn 後に適用する
    IncrementNoteOnCount();
}

void NoteVoice::NoteOff() {
    module.fm_turnoff_key(fm_ch);
    MarkHardwareKeyOffSent();
    last_fm_vib_cents_ = INT16_MIN;
    SetNoteOnCount(0);
}

void NoteVoice::ForceSilenceHardwareKey() {
    module.fm_turnoff_key(fm_ch);
    MarkHardwareKeyOffSent();
    last_fm_vib_cents_ = INT16_MIN;
}

bool NoteVoice::ShouldReconcileSilence() const {
    // NoteOff 済み Voice は減衰を自然終了させる（Reconcile による打ち切り防止）
    return !hw_key_off_sent_;
}

bool NoteVoice::WasHardwareKeyOffSent() const {
    return hw_key_off_sent_;
}

void NoteVoice::MarkHardwareKeyOffSent() {
    hw_key_off_sent_ = true;
}

void NoteVoice::ClearHardwareKeyOffSent() {
    hw_key_off_sent_ = false;
}

bool NoteVoice::TryRetrigger(int note, int32_t bk_program, int volume, ChannelEffects& effect,
                             uint8_t lr) {
    SetProgram(bk_program);
    SetVolume(volume);
    key = note;
    ClearHardwareKeyOffSent();
    last_fm_vib_cents_ = INT16_MIN;
    module.fm_turnoff_key(fm_ch);
    // Keep at least one FM register write between KeyOff and KeyOn so the EG restart is stable.
    module.fm_set_output_lr(fm_ch, lr);
    ApplyPitch(effect, 0);
    module.fm_turnon_key(fm_ch);
    return true;
}

void NoteVoice::ApplyPitch(const ChannelEffects& fx, int16_t vib_cents, bool allow_vib_dedup) {
    if (allow_vib_dedup && vib_cents == last_fm_vib_cents_) {
        return;
    }
    last_fm_vib_cents_ = vib_cents;
    //  以下の理由でkey == -1のチェックは不要
    //  - NoteOn()/TryRetrigger() では key セット後に ApplyPitch(0) を呼んでから KeyOn する。
    //  - NoteChannel からは発音中 Voice や KeyOn 直後に ApplyPitch が呼ばれる。

#if ENABLE_COARSE_TUNE == 1
    // coarse_tune エフェクト適用後の値（以降 adjusted_key として PB 計算に使用）
    const int adjusted_key = Voice::key + fx.coarse_tune;
#else
    const int adjusted_key = Voice::key;
#endif
    const uint8_t pbs = fx.pbs;
    const int16_t pbv = fx.pbv;

    //
    // PitchBendなし
    //
    if (pbs == 0 || pbv == 0) {
        int16_t diff_vib;
        if (adjusted_key < 12) {
            module.fm_set_pitch(fm_ch, 0, 0, 0);
        } else if (adjusted_key > 107) {
            // PitchBend計算用のマージンを使ってkey=108までサポート
            const int k = 11 + PBS_MARGIN;
            diff_vib      = PitchCalcVibDiff(k, vib_cents);
            module.fm_set_pitch(fm_ch, 11, 7, static_cast<int16_t>(fnum[14] - fnum[13]) + diff_vib);
        } else {
            const int k = adjusted_key % 12 + PBS_MARGIN;
            diff_vib    = PitchCalcVibDiff(k, vib_cents);
            module.fm_set_pitch(fm_ch, adjusted_key % 12, adjusted_key / 12 - 1, diff_vib);
        }
        return;
    }

    //
    // PitchBendあり
    //
    int8_t oct;
    int16_t pbkey;
    int16_t diff_pb;
    int k;
    int32_t bend_rem = 0;  // pbkeyからの半音内残差(8191分率、符号はpbvに従う)

    if (pbs <= PBS_MARGIN) {
        // 現在のNote番号基準
        pbkey = adjusted_key;
    } else {
        // PitchBend位置に最も近いNote番号基準
        // pbkeyの切り捨てと残差を同じ scaled から導出する
        // (別基数で残差を計算すると半音境界近傍で最大1半音の不整合が出る)
        const int32_t scaled = static_cast<int32_t>(pbv) * pbs;
        pbkey    = scaled / 8191 + adjusted_key;
        bend_rem = scaled % 8191;
    }

    //   k : 基準NoteのPitch Tableのインデックス
    // oct : 基準Noteのオクターブ(Block Number)
    if (pbkey < 12) {
        k   = 2;
        oct = 0;
    } else if (pbkey > 107) {
        k   = 11 + PBS_MARGIN;
        oct = 7;
    } else {
        k   = pbkey % 12 + PBS_MARGIN;
        oct = pbkey / 12 - 1;
    }

    // diff_pb : PitchBend位置での、基準NoteからのF-Numberの偏差
    if (pbs <= PBS_MARGIN) {
        // pbs=2(デフォルト)以下の場合(特別処理)
        //   現在のNote基準に+/-2半音までのF-Numberを線形補完
        if (pbv > 0) {
            diff_pb = static_cast<int32_t>(fnum[k + pbs] - fnum[k]) * pbv / 8191;
        } else {
            diff_pb = static_cast<int32_t>(fnum[k] - fnum[k - pbs]) * pbv / 8192;
        }
    } else {
        // pbs>2の場合(汎用処理)
        //   pbkeyを基準Noteに+/-1半音内のF-Numberを線形補完
        // key   pbkey        pbkey+1 : 0-127
        //  v      v            v
        //  |--..--+------------+-------->| 8191
        //  |--..--------->| pbv      : pb値: -8192 - +8191
        //         |------>| a        : pbkeyからの偏差
        //         |----------->| b   : 1半音あたりのpb値
        //  |--..--|-------o----|--
        //        fnum[k]    fnum[k+1]: F-Number
        //         |------>| diff_pb  : fnum[k]からの偏差(求める値)
        //
        //      a/b = bend_rem / 8191  (bend_rem = (pbv*pbs) % 8191)
        // diff_pb = (fnum[k+1] - fnum[k]) * a/b
        const float a_b = static_cast<float>(bend_rem) / 8191.0f;
        if (pbv > 0) {
            diff_pb = static_cast<int16_t>((fnum[k + 1] - fnum[k]) * a_b);
        } else {
            diff_pb = static_cast<int16_t>((fnum[k] - fnum[k - 1]) * a_b);
        }
    }

    const int16_t diff_vib = PitchCalcVibDiff(k, vib_cents);
    module.fm_set_pitch(fm_ch, k - PBS_MARGIN, oct, diff_pb + diff_vib);
    DPRINTF(5, " PB k=%d, diff=%d \n", pbkey, diff_pb);
}

void NoteVoice::SetPan(uint8_t lr) {
    module.fm_set_output_lr(fm_ch, lr);
}

// Debug
void NoteVoice::dump() {
    std::printf("ID=%02d CH=%02d PG=%04x %04x VOL=%3d KEY=%3d TYPE=%s OPN=%d-%d\n", id, GetChannel(),
           bk_program >> 16, bk_program & 0xffff, volume, GetKey(), GetType() ? "CSM " : "Note",
           module.id, fm_ch);
}
