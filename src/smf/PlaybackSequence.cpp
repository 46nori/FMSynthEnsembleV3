//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "PlaybackSequence.h"

void PlaybackSequence::SeedRng(uint32_t seed) {
    rng_ = seed ^ 0x9E3779B9u;
    if (rng_ == 0) {
        rng_ = 1;  // xorshiftは0から抜け出せない
    }
}

uint32_t PlaybackSequence::NextRandom() {
    uint32_t x = rng_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_ = x;
    return x;
}

void PlaybackSequence::BuildNatural() {
    for (uint16_t i = 0; i < count_; ++i) {
        order_[i] = static_cast<uint8_t>(i + 1);
    }
}

// headを先頭に置き、残りの位置をFisher-Yatesで並べ替える。カーソルは先頭(0)に置く
void PlaybackSequence::BuildShuffledWithHead(uint16_t head) {
    order_[0] = static_cast<uint8_t>(head);
    uint16_t n = 1;
    for (uint16_t p = 1; p <= count_; ++p) {
        if (p != head) {
            order_[n++] = static_cast<uint8_t>(p);
        }
    }
    for (uint16_t i = count_ - 1; i > 1; --i) {
        const uint16_t j = static_cast<uint16_t>(1 + NextRandom() % i);  // 1..i
        const uint8_t tmp = order_[i];
        order_[i] = order_[j];
        order_[j] = tmp;
    }
    cursor_ = 0;
}

// 順序の最後まで再生した後に先頭へ戻る。シャッフルOnなら順序を作り直し、
// 直前に再生していた曲が新しい先頭に来ないようにする
void PlaybackSequence::WrapToStart(uint32_t seed) {
    if (!shuffle_) {
        cursor_ = 0;
        return;
    }
    const uint16_t last_played = order_[cursor_];
    SeedRng(seed);
    // 全位置を並べ替える（先頭固定なし）
    for (uint16_t i = 0; i < count_; ++i) {
        order_[i] = static_cast<uint8_t>(i + 1);
    }
    for (uint16_t i = count_ - 1; i > 0; --i) {
        const uint16_t j = static_cast<uint16_t>(NextRandom() % (i + 1));  // 0..i
        const uint8_t tmp = order_[i];
        order_[i] = order_[j];
        order_[j] = tmp;
    }
    if (count_ > 1 && order_[0] == last_played) {
        const uint16_t j = static_cast<uint16_t>(1 + NextRandom() % (count_ - 1));  // 1..count-1
        const uint8_t tmp = order_[0];
        order_[0] = order_[j];
        order_[j] = tmp;
    }
    cursor_ = 0;
}

void PlaybackSequence::SetShuffle(bool on, uint32_t seed) {
    if (on == shuffle_) {
        return;
    }
    shuffle_ = on;
    if (!active_) {
        return;
    }
    const uint16_t current = order_[cursor_];
    if (on) {
        SeedRng(seed);
        BuildShuffledWithHead(current);
    } else {
        BuildNatural();
        cursor_ = static_cast<uint16_t>(current - 1);
    }
}

bool PlaybackSequence::Begin(uint16_t count, uint16_t start_position, uint32_t seed) {
    if (count < 1 || count > kMaxCount || start_position < 1 || start_position > count) {
        return false;
    }
    count_ = count;
    active_ = true;
    if (shuffle_) {
        SeedRng(seed);
        BuildShuffledWithHead(start_position);
    } else {
        BuildNatural();
        cursor_ = static_cast<uint16_t>(start_position - 1);
    }
    return true;
}

uint16_t PlaybackSequence::position() const {
    return active_ ? order_[cursor_] : 0;
}

SequenceStep PlaybackSequence::Advance(SequenceAdvance kind, uint32_t seed) {
    if (!active_) {
        return SequenceStep::Finished;
    }
    const bool has_next = static_cast<uint16_t>(cursor_ + 1) < count_;

    switch (kind) {
    case SequenceAdvance::TrackEnded:
        if (playback_mode_ == PlaybackMode::Single) {
            if (repeat_ == RepeatMode::Off) {
                active_ = false;
                return SequenceStep::Finished;
            }
            // Single + RepeatOne/Loop: 同曲繰り返し（1曲の範囲ではOneとLoopは同義）
            return SequenceStep::Same;
        }
        // Continuous mode: 既存の動作
        if (repeat_ == RepeatMode::One) {
            return SequenceStep::Same;
        }
        if (has_next) {
            ++cursor_;
            return SequenceStep::Moved;
        }
        if (repeat_ == RepeatMode::Loop) {
            WrapToStart(seed);
            return SequenceStep::Moved;
        }
        active_ = false;
        return SequenceStep::Finished;

    case SequenceAdvance::TrackFailed:
        if (playback_mode_ == PlaybackMode::Single) {
            // Single: 失敗しても他曲へ移動せず停止
            active_ = false;
            return SequenceStep::Finished;
        }
        // Continuous: 失敗時はRepeatモードに関係なく、範囲内の未試行曲を探すため先頭へ戻る。
        // 全曲失敗の打ち切りは呼び出し側の連続失敗数で行う。
        if (has_next) {
            ++cursor_;
            return SequenceStep::Moved;
        }
        WrapToStart(seed);
        return SequenceStep::Moved;

    case SequenceAdvance::UserNext:
        if (has_next) {
            ++cursor_;
            return SequenceStep::Moved;
        }
        if (repeat_ != RepeatMode::Off) {
            WrapToStart(seed);
            return SequenceStep::Moved;
        }
        return SequenceStep::Stay;

    case SequenceAdvance::UserPrev:
        if (cursor_ > 0) {
            --cursor_;
            return SequenceStep::Moved;
        }
        if (repeat_ != RepeatMode::Off) {
            cursor_ = static_cast<uint16_t>(count_ - 1);
            return SequenceStep::Moved;
        }
        return SequenceStep::Same;
    }
    return SequenceStep::Stay;
}
