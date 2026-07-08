//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "isr.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"

namespace Platform {

namespace {

// ----------------------------------------------------------------------------
// ISR state & handler
// ----------------------------------------------------------------------------

void (* volatile g_isr_callback)(void*) = nullptr;
void* volatile g_isr_context = nullptr;

void IsrHandler(uint gpio, uint32_t events) {
    if (g_isr_callback) {
        g_isr_callback(g_isr_context);
    }
    gpio_acknowledge_irq(gpio, GPIO_IRQ_EDGE_FALL);
}

}  // namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

/**
 * @brief 割り込みコールバック登録
 */
void AttachIsrCallback(int gpio, void (*func)(void*), void* context) {
    g_isr_callback = func;
    g_isr_context  = context;
    gpio_set_irq_enabled_with_callback(gpio,
                                       GPIO_IRQ_EDGE_FALL,
                                       true,
                                       &IsrHandler);
}

/**
 * @brief 割り込み有効化
 */
void EnableIsrCallback(int gpio) {
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, true);
}

/**
 * @brief 割り込み無効化
 */
void DisableIsrCallback(int gpio) {
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, false);
}

}  // namespace Platform
