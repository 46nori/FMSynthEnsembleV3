//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

/** @brief 再生モード（電源投入時はSingle） */
enum class PlaybackMode : uint8_t {
    Single,      ///< 1曲再生して停止（Next/Prevで手動移動した場合も終了後は停止）
    Continuous,  ///< 末尾まで自動連続再生（Repeatの設定に従う）
};

/** @brief リピートモード */
enum class RepeatMode : uint8_t {
    Off,   ///< 順序の最後の曲が終われば終了する
    One,   ///< 現在の曲を繰り返す
    Loop,  ///< 順序の最後の曲が終われば先頭へ戻る
};

/** @brief 順序を進めるトリガ */
enum class SequenceAdvance : uint8_t {
    TrackEnded,   ///< 曲がEnd of Fileに達した
    TrackFailed,  ///< 曲を開始または再生できなかった
    UserNext,     ///< Next操作
    UserPrev,     ///< Prev操作
};

/** @brief Advance()の結果 */
enum class SequenceStep : uint8_t {
    Moved,     ///< カーソルが動いた（または一巡して先頭へ戻った）。position()の曲を再生する
    Same,      ///< カーソルは動かない。現在の曲を最初から再生し直す
    Stay,      ///< 何もしない（再生中の曲をそのまま続ける）
    Finished,  ///< 順序の終わり。セッションは終了している
};

/**
 * @brief SMF再生の順序を決める純粋ロジック
 * @details 範囲の曲数・順序・カーソル・リピート・シャッフルを持ち、曲の終了やNext/Prevで
 *          次に再生する位置（1始まり）を決める。pico-sdk・FreeRTOS・ファイルI/Oに依存しない。
 *          乱数の種は呼び出し側が渡し、内部では時刻を読まない。
 *          リピートとシャッフルはセッションの有無に関わらず保持する（グローバル設定）。
 */
class PlaybackSequence {
public:
    /** @brief 1つの範囲に含められる最大曲数（位置番号をuint8_tで持つため） */
    static constexpr uint16_t kMaxCount = 255;

    void SetPlaybackMode(PlaybackMode mode) { playback_mode_ = mode; }
    PlaybackMode playback_mode() const { return playback_mode_; }

    void SetRepeat(RepeatMode mode) { repeat_ = mode; }
    RepeatMode repeat() const { return repeat_; }

    /**
     * @brief シャッフルを設定する
     * @param [in] seed 順序を作り直すときの乱数の種
     * @details セッション中に切り替えた場合、現在の曲を先頭に順序を作り直す（On）、
     *          または自然順に戻してカーソルを現在の曲へ合わせる（Off）。
     */
    void SetShuffle(bool on, uint32_t seed);
    bool shuffle() const { return shuffle_; }

    /**
     * @brief セッションを開始する
     * @param [in] count 範囲内の曲数（1〜kMaxCount）
     * @param [in] start_position 最初に再生する位置（1〜count）
     * @param [in] seed シャッフルOnのときの乱数の種
     * @return 引数が範囲外ならfalse（セッションは開始されない）
     */
    bool Begin(uint16_t count, uint16_t start_position, uint32_t seed);

    /** @brief セッションを終了する */
    void End() { active_ = false; }

    bool active() const { return active_; }
    uint16_t count() const { return count_; }

    /** @brief 現在の曲の位置（1始まり）。セッションが無いときは0 */
    uint16_t position() const;

    /**
     * @brief トリガに応じて順序を進める
     * @param [in] seed 一巡して順序を作り直すときの乱数の種
     * @details セッションが無いときはFinishedを返す。
     *          SingleモードではTrackEnded/TrackFailedで自動的にセッションを終了する
     *          （RepeatOne/LoopのときはTrackEndedで同曲繰り返し）。
     *          ContinuousモードではRepeatに従って連続再生し、TrackFailedは
     *          Repeatモードに関係なく次の候補へ進み末尾では先頭へ戻る。全曲失敗の
     *          打ち切りは呼び出し側が連続失敗数で判断する。
     */
    SequenceStep Advance(SequenceAdvance kind, uint32_t seed);

private:
    void SeedRng(uint32_t seed);
    uint32_t NextRandom();
    void BuildNatural();
    void BuildShuffledWithHead(uint16_t head);
    void WrapToStart(uint32_t seed);

    uint8_t order_[kMaxCount] = {};
    uint16_t count_ = 0;
    uint16_t cursor_ = 0;
    bool active_ = false;
    RepeatMode repeat_ = RepeatMode::Off;
    bool shuffle_ = false;
    PlaybackMode playback_mode_ = PlaybackMode::Single;
    uint32_t rng_ = 1;
};
