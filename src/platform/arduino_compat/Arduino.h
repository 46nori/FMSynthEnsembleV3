//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
// extern/LcdMenu（Arduinoライブラリ）をpico-sdk単体でビルドするための最小シム。
// DEBUGマクロは定義しない前提のため、String/Serial/log()関数本体は不要
// （LOGマクロがno-opになりコンパイル対象から外れる）。一方、F()/__FlashStringHelper/
// booleanはDEBUGガードの外（widget/ItemToggle等）でも使われるため、コア
// (byte/millis/constrain)とあわせて用意する。
#pragma once

#include <cstdint>
#include <cstring>

#include "pico/time.h"

typedef uint8_t byte;
typedef bool boolean;

inline uint32_t millis() {
    return to_ms_since_boot(get_absolute_time());
}

#define constrain(amt, low, high) \
    ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

// Arduinoのフラッシュ格納文字列マーカー型。RP2350にPROGMEM/フラッシュ領域の区別は
// 無いため実体は持たず、F()がポインタをreinterpret_castするためのタグ型として扱う。
class __FlashStringHelper;
#define F(stringLiteral) (reinterpret_cast<const __FlashStringHelper*>(stringLiteral))

// LcdMenuのutils/custom_printf.hはAVR等向けの簡易printf実装(snprintf_等へのマクロ置換)を
// 既定で使う。newlibのstd::snprintf等をそのまま使いたいため無効化する。
#define USE_CUSTOM_PRINTF 0
