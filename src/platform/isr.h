//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

// GPIO pin assignments for FM /IRQ (Wired-OR, all chips share GPIO26)
#define FM_IRQ  26

namespace Platform {

/**
 * @brief 割り込みコールバック登録
 * @param gpio     GPIO pin (FM_IRQ等)
 * @param func     コールバック関数 (ISR 内から呼ばれる)
 * @param context  func に渡す任意のポインタ
 */
void AttachIsrCallback(int gpio, void (*func)(void*), void* context);

/**
 * @brief 割り込みを有効化
 * @param gpio  GPIO pin
 */
void EnableIsrCallback(int gpio);

/**
 * @brief 割り込みを無効化
 * @param gpio  GPIO pin
 */
void DisableIsrCallback(int gpio);

}  // namespace Platform
