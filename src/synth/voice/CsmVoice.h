//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <array>
#include <cstdint>
#include "config.h"
#include "OpnBase.h"
#include "Voice.h"

// 音声データ
#include "csm/VOICE.dat"

class CsmVoice : public Voice {
private:
    std::array<OpnBase*, 4>& modules;
    int operators;                      // 使用するオペレータ数
    int num_modules;                    // 使用するFM音源モジュール数 (Init()で確定)
    int modTB;                          // Timer Bを使用するFM音源モジュールの物理Dockインデックス
    std::array<int, 4> dock_indices;    // 実装済みモジュールの物理Dockインデックス一覧 (コンストラクタで確定)

    int frame;         // 現在再生中のフレーム
    int interp_count;  // 現在の補間回数
    int lastFrame;     // 最終フレーム
    bool isLastFrame;  // 最終フレームフラグ

    struct frame_format data;  // CSM音声データ
    struct diff_format {
        int16_t Pitch;
        int16_t TL[CSM_N];
        int16_t Freq[CSM_N];
    };
    struct diff_format diff;  // フレーム補完用データ

    // CSM playback state is owned by CsmFrameTask.
    bool running;

    int irq_gpio_ = -1;
    static void IrqTickThunk(void* ctx);

public:
    /**
     * @brief コンストラクタ
     * @param modules   FM音源モジュールのリスト
     * @param id        Voice ID (for debug)
     */
    CsmVoice(std::array<OpnBase*, 4>& modules, int id);
    CsmVoice() = delete;

    /**
     * @brief デストラクタ
     */
    virtual ~CsmVoice();

    /**
     * @brief Voice内部状態をリセットする
     */
    void Reset() override;

    /**
     * @brief Module IDを返す
     * @return Module ID
     */
    int GetModuleId() override;

    /**
     * @brief MIDI Program(音色)のセット
     * @param no MIDI Bank/Program No.
     * @details 現在のProgram値から更新された場合に限り、音色パラメータをセットする
     */
    void SetProgram(int32_t no) override;

    /**
     * @brief MIDI Volumeのセット
     * @param vol MIDI Volume (0-127)
     * @details 現在のVolume値から更新された場合に限り、音量をセットする
     */
    void SetVolume(int vol) override;

    /**
     * @brief Note On
     * @param note    MIDI Note No.
     * @param program MIDI Bank/Program No.
     * @param volume  MIDI Volume (0-127)
     * @param effect  Voice effect
     * @param lr      Output Both(0xc0), Left(0x80), Right(0x40)
     * @details FM音源の発音を開始する。
     *          effectの設定値により、PitchBendやModulationを設定する
     */
    void NoteOn(int note, int32_t program, int volume, ChannelEffects& effect, uint8_t lr) override;

    /**
     * @brief CSM音素片を新しいNoteとして再開始する
     */
    bool TryRetrigger(int note, int32_t program, int volume, ChannelEffects& effect,
                      uint8_t lr) override;

    /**
     * @brief Note Off
     * @details Note FM音源の発音を停止する
     */
    void NoteOff() override;

    /**
     * @brief 現在のkeyを基準にPitchを設定する
     * @param effect Voice effect
     *               effect.pbv PitchBend値 (-8192-8191)
     *               effect.pbs PitchBend Sensitivity (0-127)
     * @details PitchBendを指定しない場合はeffect.pbv=0とする
     */
    void ApplyPitch(const ChannelEffects& fx, int16_t vib_cents,
                    bool allow_vib_dedup = false) override;

    void SetPan(uint8_t lr) override;

    /**
     * @brief CSMモードの初期化（GPIO26 IRQ はティック通知のみ。フレーム処理は CsmFrameTask）
     */
    void Init();

    /**
     * @brief 再生開始とフレームの更新処理
     * @param isFirst true:再生開始, false:フレーム更新(2回目以降)
     * @details 2回目以降はTimer Bのオーバーフローごとに呼び出す。
     */
    void UpdateFrame(bool isFirst);

    /**
     * @brief CsmFrameTask 上でCSM再生を開始
     */
    void Start(int note, int32_t program, int volume, uint8_t lr);

    /** 
     * @brief フレームオーバーの検出
     * @return true:フレームオーバー
     * @details Timer Bのオーバーフローをチェックする
     */
    bool IsFrameOver();

    /**
     * @brief 全てのTimer A/BとCH3の発音を停止
     */
    void Stop();

    // Debug
    void dump() override;

private:
    void stop_playback_locked();

    /**
     *  @brief CH3の初期化
     */
    void init_ch3(OpnBase& opn);

    /** 
     * @brief フレームパラメータの更新
     * @param isFirst true:最初のフレームから再生する
     * @return true:最終フレームを検出した
     * @details 最終フレームなら、次回のTimer Bのオーバーフローで再生を終了する
     */
    bool update(bool isFirst);

    void phraseSetupFromKey(bool isFirst);
    uint16_t loadFramePitchAndMarkLast();
#ifdef ENABLE_INTERPOLATION
    void interpolationAccumulate(uint16_t pitch_from_table);
#endif
    void writeFrameParametersToModules(uint16_t timer_a_pitch);
    void restartTimersAllDocks();
    void advanceContentStep();
};
