/*
 * fm_bus.c — FM bus initialization, register write/read, and raw PIO helpers.
 *
 * Single-SM unified PIO program (fm_bus): write, read_status, and read_reg
 * without disabling the state machine or swapping CS at handoff.
 */

#include "fm_bus_internal.h"
#include "fm_bus.pio.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
/* pio_encode_jmp() omits sideset → side 0 (/WR=L,/RD=L). Use fm_bus_encode_jmp_side3. */
#include "hardware/sync.h"
#include "hardware/clocks.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define FM_BUS_INIT_MAGIC  0xB0410FE0u

/* GPIO2-15 driven by the bus SM */
static uint32_t fm_bus_pin_mask(void)
{
    return (((1u << 14) - 1u) << FM_GPIO_D0);
}

/* Snapshot FM bus pins from SIO (call before pio_gpio_init). */
static uint32_t fm_bus_capture_sio_pins(void)
{
    uint32_t pins = 0u;
    for (uint i = FM_GPIO_D0; i <= FM_GPIO_RD; i++) {
        if (gpio_get(i)) {
            pins |= (1u << i);
        }
    }
    return pins;
}

static pio_sm_config fm_bus_sm_config(fm_bus_t *bus, float clkdiv)
{
    pio_sm_config cfg = fm_bus_program_get_default_config(bus->offset_bus);

    sm_config_set_out_pins(&cfg, FM_GPIO_D0, 12);
    sm_config_set_in_pins(&cfg, FM_GPIO_D0);
    sm_config_set_sideset_pins(&cfg, FM_GPIO_WR);
    sm_config_set_out_shift(&cfg, true, false, 32);
    sm_config_set_in_shift(&cfg, false, false, 8);
    sm_config_set_clkdiv(&cfg, clkdiv);
    return cfg;
}

/* JMP with side 3 (/WR=H,/RD=H). Required on every exec while CS may be low. */
static inline uint32_t fm_bus_encode_jmp_side3(uint32_t abs_pc)
{
    return pio_encode_jmp(abs_pc) | pio_encode_sideset(2u, 3u);
}

static void fm_bus_jmp_entry(fm_bus_t *bus, uint entry_offset)
{
    uint32_t insn = fm_bus_encode_jmp_side3(bus->offset_bus + entry_offset);
    pio_sm_exec_wait_blocking(bus->pio, bus->sm, insn);
}

/* SET PINDIRS with side 3 (/WR=H,/RD=H). pio_sm_set_consecutive_pindirs()
 * omits side bits, which glitches /WR=/RD=L on this side_set-mandatory
 * program. Use this whenever forcing D-pin direction while the bus is live. */
static inline uint32_t fm_bus_encode_set_pindirs_side3(uint32_t value)
{
    return pio_encode_set(pio_pindirs, value) | pio_encode_sideset(2u, 3u);
}

/* Reimplements pico-sdk's pio_sm_set_consecutive_pindirs() with side 3
 * forced on every injected SET PINDIRS instruction. */
static void fm_bus_set_consecutive_pindirs_side3(PIO pio, uint sm, uint pin,
                                                  uint count, bool is_out)
{
    pin -= pio_get_gpio_base(pio);
    uint32_t pinctrl_saved = pio->sm[sm].pinctrl;
    uint32_t execctrl_saved = pio->sm[sm].execctrl;
    hw_clear_bits(&pio->sm[sm].execctrl, 1u << PIO_SM0_EXECCTRL_OUT_STICKY_LSB);
    uint32_t pindir_val = is_out ? 0x1fu : 0u;
    while (count > 5u) {
        pio->sm[sm].pinctrl = (5u << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
                               (pin << PIO_SM0_PINCTRL_SET_BASE_LSB);
        pio_sm_exec(pio, sm, fm_bus_encode_set_pindirs_side3(pindir_val));
        count -= 5u;
        pin = (pin + 5u) & 0x1fu;
    }
    pio->sm[sm].pinctrl = (count << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
                           (pin << PIO_SM0_PINCTRL_SET_BASE_LSB);
    pio_sm_exec(pio, sm, fm_bus_encode_set_pindirs_side3(pindir_val));
    pio->sm[sm].pinctrl = pinctrl_saved;
    pio->sm[sm].execctrl = execctrl_saved;
}

/* SM must be idle (TXSTALL on main_entry pull) before dispatching a read path. */
static void fm_bus_begin_read(fm_bus_t *bus, uint entry_offset)
{
    fm_bus_wait_write_idle(bus);
    fm_bus_jmp_entry(bus, entry_offset);
}

static void fm_bus_restore_d_output(fm_bus_t *bus)
{
    fm_bus_set_consecutive_pindirs_side3(bus->pio, bus->sm, FM_GPIO_D0, 8, true);
}

/* ---------------------------------------------------------------------------
 * fm_bus_init
 * --------------------------------------------------------------------------*/
int fm_bus_init(fm_bus_t *bus, PIO pio, uint sm, uint32_t pio_hz)
{
    memset(bus, 0, sizeof(*bus));
    bus->pio    = pio;
    bus->sm     = sm;
    bus->pio_hz = pio_hz;

    int sn = (int)spin_lock_claim_unused(false);
    if (sn < 0) {
        return -1;
    }
    bus->spinlock_num = (uint)sn;

    if (!pio_can_add_program(pio, &fm_bus_program)) {
        spin_lock_unclaim(bus->spinlock_num);
        bus->spinlock_num = 0u;
        return -2;
    }
    int off = pio_add_program(pio, &fm_bus_program);
    if (off < 0) {
        spin_lock_unclaim(bus->spinlock_num);
        bus->spinlock_num = 0u;
        return -2;
    }
    bus->offset_bus = (uint)off;

    uint func_pio = (pio == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1;

    for (uint i = FM_GPIO_D0; i <= FM_GPIO_RD; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
    }
    gpio_put(FM_GPIO_WR, 1);
    gpio_put(FM_GPIO_RD, 1);

    uint32_t bus_pins = fm_bus_capture_sio_pins();

    for (uint i = FM_GPIO_D0; i <= FM_GPIO_RD; i++) {
        pio_gpio_init(pio, i);
        gpio_set_function(i, func_pio);
    }

    float clkdiv = (float)clock_get_hz(clk_sys) / (float)pio_hz;
    pio_sm_config cfg = fm_bus_sm_config(bus, clkdiv);
    pio_sm_init(pio, sm, bus->offset_bus + fm_bus_offset_main_entry, &cfg);
    fm_bus_set_consecutive_pindirs_side3(pio, sm, FM_GPIO_D0, 14, true);
    pio_sm_set_pins_with_mask(pio, sm, bus_pins, fm_bus_pin_mask());
    pio_sm_set_enabled(pio, sm, true);

    bus->init_magic = FM_BUS_INIT_MAGIC;
    return 0;
}

void fm_bus_deinit(fm_bus_t *bus)
{
    if (bus == NULL || bus->init_magic != FM_BUS_INIT_MAGIC) {
        return;
    }

    pio_sm_set_enabled(bus->pio, bus->sm, false);
    pio_remove_program(bus->pio, &fm_bus_program, bus->offset_bus);
    spin_lock_unclaim(bus->spinlock_num);
    bus->init_magic = 0u;
}

/* ---------------------------------------------------------------------------
 * fm_device_init
 * --------------------------------------------------------------------------*/
static uint32_t fm_wait_count(uint32_t n_phi_m, uint32_t pio_hz,
                               uint32_t master_clock_hz)
{
    if (n_phi_m == 0u || master_clock_hz == 0u) {
        return 0u;
    }
    uint64_t count = ((uint64_t)n_phi_m * (uint64_t)pio_hz) / (uint64_t)master_clock_hz;
    return (count > 0u) ? (uint32_t)(count - 1u) : 0u;
}

int fm_device_init(fm_device_t *dev, fm_bus_t *bus,
                   fm_chip_id_t chip_id, fm_device_type_t device_type,
                   fm_master_clock_hz_t master_clock_hz)
{
    if (chip_id > 3u) return -1;
    if (device_type != FM_DEVICE_YM2203 && device_type != FM_DEVICE_YM2608) return -1;
    if (master_clock_hz == 0u) return -1;

    dev->chip_id         = chip_id;
    dev->device_type     = device_type;
    dev->master_clock_hz = master_clock_hz;
    dev->bus             = bus;

    uint32_t ph = bus->pio_hz;
    uint32_t mh = master_clock_hz;
    dev->wait_table.w0   = fm_wait_count(  0u, ph, mh);
    dev->wait_table.w17  = fm_wait_count( 17u, ph, mh);
    dev->wait_table.w47  = fm_wait_count( 47u, ph, mh);
    dev->wait_table.w83  = fm_wait_count( 83u, ph, mh);
    dev->wait_table.w576 = fm_wait_count(576u, ph, mh);

    return 0;
}

void fm_write_reg_raw(fm_bus_t *bus, uint32_t addr_word, uint32_t data_word)
{
    /* Returns once both words are queued; does not wait for W2 to elapse.
     * The next raw primitive's leading fm_bus_wait_write_idle() re-syncs on
     * the SM before touching it. The FM Key On/Off "gate" register (0x28)
     * additionally waits here — see write_reg()'s fm_addr_is_gate_reg() call
     * and fm_bus_internal.h for why. */
    fm_bus_wait_write_idle(bus);
    pio_sm_put_blocking(bus->pio, bus->sm, addr_word);
    pio_sm_put_blocking(bus->pio, bus->sm, data_word);
}

/* Key On/Off registers gate the envelope generator's internal state
 * transition rather than just latching a parameter value. Real-hardware
 * testing (2026-08-15, stuck notes after playback Stop) showed that
 * deferring the post-write idle wait for reg 0x28 is not safe, even
 * though it is for plain parameter registers (freq/TL/tone/pan etc.)
 * which only get read on the next internal sample tick. Root cause at
 * the silicon level is unconfirmed; treat this as empirically required,
 * not derived from the datasheet.
 *
 * Rhythm KeyOn/Damp (reg 0x10, A1=0) is the same register class but is
 * NOT included here: it was never exercised by the real-hardware test
 * that found the 0x28 problem (the failing song had no rhythm activity
 * at that point), and unlike NoteVoice, a rhythm hit self-decays via its
 * envelope rather than sustaining until an explicit off — so the same
 * underlying race, if it exists here too, would show up as degraded
 * Damp/retrigger quality during playback, not as a note stuck on. This
 * is deliberately unverified; if rhythm-heavy playback later shows Damp
 * artifacts, add `a1 == 0u && addr == 0x10u` back here first. */
static bool fm_addr_is_gate_reg(uint8_t addr, uint8_t a1)
{
    (void)a1;
    if (addr == 0x28u) return true;  /* FM Key On/Off (all channels) */
    return false;
}

uint8_t fm_read_status_raw(fm_bus_t *bus, uint8_t chip_id,
                           uint8_t a1, uint32_t tacc_count)
{
    fm_bus_begin_read(bus, fm_bus_offset_status_entry);
    /* Match read_reg path: D0-D7 must be input during status read. */
    fm_bus_set_consecutive_pindirs_side3(bus->pio, bus->sm, FM_GPIO_D0, 8, false);
    pio_sm_put_blocking(bus->pio, bus->sm,
                        fm_make_read_status_word(chip_id, a1, tacc_count));

    uint32_t val = pio_sm_get_blocking(bus->pio, bus->sm);
    fm_bus_restore_d_output(bus);
    fm_bus_wait_write_idle(bus);

    return (uint8_t)(val & 0xFFu);
}

uint8_t fm_read_reg_raw(fm_bus_t *bus, uint8_t addr, uint8_t chip_id,
                        uint8_t a1, uint32_t tacc_count)
{
    fm_bus_begin_read(bus, fm_bus_offset_reg_entry);
    pio_sm_put_blocking(bus->pio, bus->sm,
                        fm_make_read_reg_word1(addr, chip_id, a1));
    /* Addr phase done (SM stalled on Word2 pull); then D input, then data phase. */
    fm_bus_wait_write_idle(bus);
    fm_bus_set_consecutive_pindirs_side3(bus->pio, bus->sm, FM_GPIO_D0, 8, false);
    pio_sm_put_blocking(bus->pio, bus->sm,
                        fm_make_read_reg_word2(chip_id, a1, tacc_count));

    uint32_t val = pio_sm_get_blocking(bus->pio, bus->sm);
    fm_bus_restore_d_output(bus);
    fm_bus_wait_write_idle(bus);

    return (uint8_t)(val & 0xFFu);
}

uint8_t fm_read_reg_data_raw(fm_bus_t *bus, uint8_t chip_id,
                             uint8_t a1, uint32_t tacc_count)
{
    fm_bus_begin_read(bus, fm_bus_offset_data_read_entry);
    fm_bus_set_consecutive_pindirs_side3(bus->pio, bus->sm, FM_GPIO_D0, 8, false);
    pio_sm_put_blocking(bus->pio, bus->sm,
                        fm_make_read_reg_word2(chip_id, a1, tacc_count));

    uint32_t val = pio_sm_get_blocking(bus->pio, bus->sm);
    fm_bus_restore_d_output(bus);
    fm_bus_wait_write_idle(bus);

    return (uint8_t)(val & 0xFFu);
}

static uint32_t get_w1_count(const fm_device_t *dev, uint8_t addr, uint8_t a1)
{
    /* I/O PortA/B (0x0e/0x0f) はデータシート上SSG扱い(W1=W2=0)だが、実機では
       アドレスライト直後(W1=0)のデータライトが動作中のFM発音のピッチを乱す
       （Undocumentedな仕様。FM相当のW1で解消、W2は0のままで問題ないことを
       実機で確認済み。実測での最小必要値は3サイクルだが、FMレジスタと同じ
       17サイクルを安全マージン込みで採用する）。FM相当のW1を常時適用する
    */
    if (a1 == 0u && (addr == 0x0eu || addr == 0x0fu)) {
        return dev->wait_table.w17;
    }
    if (addr <= 0x0fu) {
        return dev->wait_table.w0;
    }
    if (a1 == 1u && addr == 0x10u) {
        return dev->wait_table.w0;
    }
    if (a1 == 0u && addr >= 0x10u && addr <= 0x1du) {
        return dev->wait_table.w17;
    }
    if (addr >= 0x21u && addr <= 0xb6u) {
        return dev->wait_table.w17;
    }
    return dev->wait_table.w0;
}

static uint32_t get_w2_count(const fm_device_t *dev, uint8_t addr, uint8_t a1)
{
    /* I/O PortA/B (0x0e/0x0f) の W2 は 0 のままで問題ないことを実機で確認済み
       （W1 の例外については get_w1_count を参照）
    */
    if (addr <= 0x0fu) {
        return dev->wait_table.w0;
    }
    if (a1 == 1u && addr <= 0x10u) {
        return dev->wait_table.w0;
    }
    if (a1 == 0u && addr == 0x10u) {
        return dev->wait_table.w576;
    }
    if (a1 == 0u && addr >= 0x11u && addr <= 0x1du) {
        return dev->wait_table.w83;
    }
    if (addr >= 0x21u && addr <= 0x9eu) {
        return dev->wait_table.w83;
    }
    if (addr >= 0xa0u && addr <= 0xb6u) {
        return dev->wait_table.w47;
    }
    return dev->wait_table.w0;
}

void write_reg(const fm_device_t *dev, uint8_t addr, uint8_t a1, uint8_t data)
{
    assert(dev != NULL && dev->bus != NULL);
    assert(addr <= 0xb6u);
    assert(a1 <= 1u);
    assert(dev->device_type == FM_DEVICE_YM2608 || a1 == 0u);

    uint32_t w1 = get_w1_count(dev, addr, a1);
    uint32_t w2 = get_w2_count(dev, addr, a1);

    uint32_t addr_word = fm_make_write_word(addr, dev->chip_id, 0u, a1, w1);
    uint32_t data_word = fm_make_write_word(data, dev->chip_id, 1u, a1, w2);

    uint32_t irq_state = fm_bus_lock(dev->bus);
    fm_write_reg_raw(dev->bus, addr_word, data_word);
    if (fm_addr_is_gate_reg(addr, a1)) {
        fm_bus_wait_write_idle(dev->bus);
    }
    fm_bus_unlock(dev->bus, irq_state);
}

void write_reg_data(const fm_device_t *dev, uint8_t a1, uint8_t data)
{
    assert(dev != NULL && dev->bus != NULL);
    assert(a1 <= 1u);
    assert(dev->device_type == FM_DEVICE_YM2608 || a1 == 0u);

    /* Pattern-2 data phase (A0=1). W_count follows latched ADPCM reg (0x08): W2=0. */
    const uint32_t w2 = get_w2_count(dev, 0x08u, a1);
    const uint32_t data_word = fm_make_write_word(data, dev->chip_id, 1u, a1, w2);

    uint32_t irq_state = fm_bus_lock(dev->bus);
    fm_bus_wait_write_idle(dev->bus);
    pio_sm_put_blocking(dev->bus->pio, dev->bus->sm, data_word);
    fm_bus_wait_write_idle(dev->bus);
    fm_bus_unlock(dev->bus, irq_state);
}

uint8_t read_status(const fm_device_t *dev, uint8_t a1)
{
    assert(dev != NULL && dev->bus != NULL);
    assert(a1 <= 1u);
    assert(dev->device_type == FM_DEVICE_YM2608 || a1 == 0u);

    const uint32_t ns_min = (a1 == 0u) ? 250u : 380u;  /* Status1 (A1=1): Tacc 380 ns */
    uint32_t tacc = fm_tacc_count(ns_min, dev->bus->pio_hz);

    uint32_t irq_state = fm_bus_lock(dev->bus);
    uint8_t val = fm_read_status_raw(dev->bus, dev->chip_id, a1, tacc);
    fm_bus_unlock(dev->bus, irq_state);

    return val;
}

uint8_t read_reg(const fm_device_t *dev, uint8_t addr, uint8_t a1)
{
    assert(dev != NULL && dev->bus != NULL);
    assert(a1 <= 1u);
    assert(dev->device_type == FM_DEVICE_YM2608 || a1 == 0u);
    assert((a1 == 0u && addr <= 0x0fu) || (a1 == 0u && addr == 0xffu) ||
           (a1 == 1u && (addr == 0x08u || addr == 0x0fu)));

    uint32_t ns_min = (a1 == 0u) ? 400u : 380u;
    uint32_t tacc   = fm_tacc_count(ns_min, dev->bus->pio_hz);

    uint32_t irq_state = fm_bus_lock(dev->bus);
    uint8_t val = fm_read_reg_raw(dev->bus, addr, dev->chip_id, a1, tacc);
    fm_bus_unlock(dev->bus, irq_state);

    return val;
}

uint8_t read_reg_data(const fm_device_t *dev, uint8_t a1)
{
    assert(dev != NULL && dev->bus != NULL);
    assert(a1 <= 1u);
    assert(dev->device_type == FM_DEVICE_YM2608 || a1 == 0u);

    const uint32_t ns_min = (a1 == 0u) ? 400u : 380u;
    const uint32_t tacc = fm_tacc_count(ns_min, dev->bus->pio_hz);

    uint32_t irq_state = fm_bus_lock(dev->bus);
    uint8_t val = fm_read_reg_data_raw(dev->bus, dev->chip_id, a1, tacc);
    fm_bus_unlock(dev->bus, irq_state);

    return val;
}
