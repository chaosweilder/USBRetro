// psx_host.c - Native PS1 / PS2 controller host driver (PIO + DMA)
//
// The PSX SIO transaction is clocked entirely in hardware by a PIO state machine
// (see psx.pio) and read by a DMA channel, so the CPU never bit-bangs and never
// blocks — the input task just checks whether the latest transaction finished,
// parses it, and kicks off the next one. This keeps USB output smooth (an
// earlier CPU bit-bang blocked the main loop ~ms per poll -> very laggy).
//
// Poll response (0x01 0x42 0x00 ...):
//   buf[0]: header           buf[1]: ID (0x41 digital / 0x73 analog / 0x79 DS2)
//   buf[2]: 0x5A ready       buf[3]: buttons low   buf[4]: buttons high
//   buf[5..8] (analog only):  right X, right Y, left X, left Y

#include "psx_host.h"
#include "core/router/router.h"
#include "core/input_event.h"
#include "core/buttons.h"

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "psx.pio.h"
#include <string.h>

// ============================================================================
// CONFIG
// ============================================================================

#define PSX_DEV_ADDR     0xE1
#define PSX_HW_BYTES     9          // full transaction the PIO clocks each poll

#define PSX_ID_DIGITAL   0x41
#define PSX_ID_ANALOG    0x73
#define PSX_ID_PRESSURE  0x79
#define PSX_ID_NEGCON    0x23   // Namco neGcon: twist + analog I/II/L
#define NEGCON_BTN_THRESH 0x40  // I/II/L analog press level that latches a button

// ============================================================================
// STATE
// ============================================================================

static uint8_t pin_cmd = PSX_PIN_CMD;
static uint8_t pin_clk = PSX_PIN_CLK;
static uint8_t pin_att = PSX_PIN_ATT;
static uint8_t pin_dat = PSX_PIN_DAT;

static PIO       psx_pio = pio0;
static uint      psx_sm;
static int       psx_dma = -1;
static uint32_t  psx_rx[PSX_HW_BYTES];   // DMA target: one byte per word (bits 31-24)

// Filled by the DMA-completion ISR, consumed by the task.
static volatile uint8_t  psx_snap[PSX_HW_BYTES];
static volatile bool     psx_new;
static volatile bool     psx_busy;          // a transaction is in flight
static absolute_time_t   psx_next_poll;     // when the next poll may start
static absolute_time_t   psx_next_config;   // throttle analog-enable retries

// Real consoles poll the pad once per frame and leave ATT high (deselected) for
// milliseconds between polls. Hammering back-to-back (only ~32 us recovery) makes
// the controller return stale/garbage analog -> choppy. Pace polls like the
// reference (PS2X read_delay): ~3 ms gives ~1.7 ms of recovery per poll.
#define PSX_POLL_INTERVAL_US  3000

static bool      initialized = false;
static bool      connected   = false;
static uint8_t   last_id     = 0xFF;

// state-change suppression
static uint32_t  last_buttons = 0;
static uint8_t   last_lx = 128, last_ly = 128, last_rx = 128, last_ry = 128;
static bool      last_submitted = false;

// ============================================================================
// TRANSACTION
// ============================================================================

// Arm the DMA for one transaction and trigger the PIO. Non-blocking.
static void psx_start_xfer(void) {
    psx_busy = true;
    dma_channel_set_write_addr(psx_dma, psx_rx, false);
    dma_channel_set_trans_count(psx_dma, PSX_HW_BYTES, true);   // arm + start
    // Command word, shifted LSB-first by the PIO: byte0=0x01, byte1=0x42, rest 0.
    pio_sm_put(psx_pio, psx_sm, 0x01u | (0x42u << 8));
}

// ---------------------------------------------------------------------------
// Analog enable/lock (bit-banged config sequence)
// ---------------------------------------------------------------------------
// PS1/PS2 pads power up in digital mode (ID 0x41) until the host puts them in
// analog mode. Send the standard PS2X config sequence:
//   enter_config -> set_mode(analog=0x01, lock=0x03) -> exit_config
// The lock byte (0x03) stops the controller's ANALOG button from toggling it
// back to digital. Config is rare (connect / mode-drop), so a blocking bit-bang
// is fine. It runs only when the poll PIO is parked (psx_busy == false), so we
// just borrow the pins (PIO -> SIO -> PIO) without tearing the SM down.

static uint8_t psx_bb_byte(uint8_t out) {
    uint8_t in = 0;
    for (int i = 0; i < 8; i++) {
        gpio_put(pin_cmd, (out >> i) & 1);          // CMD bit, LSB first
        gpio_put(pin_clk, 0);                        // CLK falling edge
        busy_wait_us(6);                             // let DAT settle
        if (gpio_get(pin_dat)) in |= (1u << i);      // sample DAT
        gpio_put(pin_clk, 1);                         // CLK rising edge
        busy_wait_us(6);
    }
    gpio_put(pin_cmd, 1);
    busy_wait_us(14);                                // inter-byte recovery
    return in;
}

static void psx_bb_cmd(const uint8_t* cmd, int len) {
    gpio_put(pin_att, 0);                            // select
    busy_wait_us(14);
    for (int i = 0; i < len; i++) psx_bb_byte(cmd[i]);
    gpio_put(pin_att, 1);                            // deselect
    busy_wait_us(200);                               // inter-command recovery
}

static void psx_send_config(void) {
    static const uint8_t enter_config[] = {0x01, 0x43, 0x00, 0x01, 0x00};
    static const uint8_t set_mode[]     = {0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t exit_config[]  = {0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};

    // Borrow the pins from the (parked) PIO and drive them by hand.
    gpio_set_function(pin_clk, GPIO_FUNC_SIO);
    gpio_set_function(pin_cmd, GPIO_FUNC_SIO);
    gpio_set_function(pin_att, GPIO_FUNC_SIO);
    gpio_set_function(pin_dat, GPIO_FUNC_SIO);
    gpio_set_dir(pin_clk, true);
    gpio_set_dir(pin_cmd, true);
    gpio_set_dir(pin_att, true);
    gpio_set_dir(pin_dat, false);
    gpio_put(pin_clk, 1);
    gpio_put(pin_cmd, 1);
    gpio_put(pin_att, 1);
    busy_wait_us(100);

    psx_bb_cmd(enter_config, sizeof(enter_config));
    psx_bb_cmd(set_mode,     sizeof(set_mode));
    psx_bb_cmd(exit_config,  sizeof(exit_config));

    // Hand the pins back to the PIO (still parked at `pull`).
    pio_gpio_init(psx_pio, pin_clk);
    pio_gpio_init(psx_pio, pin_att);
    pio_gpio_init(psx_pio, pin_cmd);
    pio_gpio_init(psx_pio, pin_dat);
    pio_sm_set_consecutive_pindirs(psx_pio, psx_sm, pin_clk, 2, true);
    pio_sm_set_consecutive_pindirs(psx_pio, psx_sm, pin_cmd, 1, true);
    pio_sm_set_consecutive_pindirs(psx_pio, psx_sm, pin_dat, 1, false);
}

// DMA-completion ISR: snapshot the bytes and mark the transaction done. The
// task consumes psx_snap and PACES the next poll (recovery gap), so we do NOT
// re-fire here — back-to-back polling overwhelms the controller.
static void __isr psx_dma_isr(void) {
    if (!(dma_hw->ints0 & (1u << psx_dma))) return;   // not our channel
    dma_hw->ints0 = 1u << psx_dma;                      // clear

    for (int i = 0; i < PSX_HW_BYTES; i++) psx_snap[i] = (uint8_t)(psx_rx[i] >> 24);
    psx_new = true;
    psx_busy = false;   // transaction complete; task schedules the next one
}

// Decode PS1/PS2 button bytes (active-low: 0 = pressed) into JP_BUTTON_* bitmap.
static uint32_t decode_buttons(uint8_t lo, uint8_t hi) {
    uint32_t b = 0;
    if (!(lo & 0x01)) b |= JP_BUTTON_S1;   // SELECT
    if (!(lo & 0x02)) b |= JP_BUTTON_L3;
    if (!(lo & 0x04)) b |= JP_BUTTON_R3;
    if (!(lo & 0x08)) b |= JP_BUTTON_S2;   // START
    if (!(lo & 0x10)) b |= JP_BUTTON_DU;
    if (!(lo & 0x20)) b |= JP_BUTTON_DR;
    if (!(lo & 0x40)) b |= JP_BUTTON_DD;
    if (!(lo & 0x80)) b |= JP_BUTTON_DL;
    if (!(hi & 0x01)) b |= JP_BUTTON_L2;
    if (!(hi & 0x02)) b |= JP_BUTTON_R2;
    if (!(hi & 0x04)) b |= JP_BUTTON_L1;
    if (!(hi & 0x08)) b |= JP_BUTTON_R1;
    if (!(hi & 0x10)) b |= JP_BUTTON_B4;   // Triangle
    if (!(hi & 0x20)) b |= JP_BUTTON_B2;   // Circle
    if (!(hi & 0x40)) b |= JP_BUTTON_B1;   // Cross
    if (!(hi & 0x80)) b |= JP_BUTTON_B3;   // Square
    return b;
}

// ============================================================================
// PUBLIC API
// ============================================================================

void psx_host_init(void) {
    psx_host_init_pins(PSX_PIN_CMD, PSX_PIN_CLK, PSX_PIN_ATT, PSX_PIN_DAT);
}

void psx_host_init_pins(uint8_t cmd, uint8_t clk, uint8_t att, uint8_t dat) {
    if (initialized) return;
    pin_cmd = cmd; pin_clk = clk; pin_att = att; pin_dat = dat;

    // PIO pin groups: set = {CLK (base), ATT (base+1)}; out/sideset = CMD; in = DAT.
    uint offset = pio_add_program(psx_pio, &psx_program);
    psx_sm = pio_claim_unused_sm(psx_pio, true);
    pio_sm_config c = psx_program_get_default_config(offset);
    sm_config_set_set_pins(&c, pin_clk, 2);
    sm_config_set_out_pins(&c, pin_cmd, 1);
    sm_config_set_sideset_pins(&c, pin_cmd);
    sm_config_set_in_pins(&c, pin_dat);
    sm_config_set_out_shift(&c, true, false, 32);   // shift right (LSB first), manual pull
    sm_config_set_in_shift(&c, true, true, 8);      // shift right, autopush every byte

    pio_gpio_init(psx_pio, pin_clk);
    pio_gpio_init(psx_pio, pin_att);
    pio_gpio_init(psx_pio, pin_cmd);
    pio_gpio_init(psx_pio, pin_dat);
    gpio_pull_up(pin_dat);

    pio_sm_set_consecutive_pindirs(psx_pio, psx_sm, pin_clk, 2, true);   // CLK, ATT out
    pio_sm_set_consecutive_pindirs(psx_pio, psx_sm, pin_cmd, 1, true);   // CMD out
    pio_sm_set_consecutive_pindirs(psx_pio, psx_sm, pin_dat, 1, false);  // DAT in

    // 500 kHz instruction rate (2 us/cycle). With the [3] settle delay the DAT
    // line gets ~8 us to settle after each falling edge (the rig was clean at
    // ~10 us, corrupt at ~4 us), giving a clean ~60 kHz PSX clock and ~1.4 ms
    // per poll (~700 Hz). Non-blocking, so it stays snappy.
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / 500000.0f);

    pio_sm_init(psx_pio, psx_sm, offset, &c);
    pio_sm_set_enabled(psx_pio, psx_sm, true);

    // DMA: drain the PIO RX FIFO into psx_rx, paced by the RX DREQ.
    psx_dma = dma_claim_unused_channel(true);
    dma_channel_config dc = dma_channel_get_default_config(psx_dma);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, false);
    channel_config_set_write_increment(&dc, true);
    channel_config_set_dreq(&dc, pio_get_dreq(psx_pio, psx_sm, false));
    dma_channel_configure(psx_dma, &dc, psx_rx, &psx_pio->rxf[psx_sm], PSX_HW_BYTES, false);

    // Fire the ISR on each completed transaction so the next one starts with no
    // main-loop gap. Shared handler so it coexists with other DMA_IRQ_0 users.
    dma_channel_set_irq0_enabled(psx_dma, true);
    irq_add_shared_handler(DMA_IRQ_0, psx_dma_isr,
                           PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_0, true);

    initialized = true;
    psx_next_poll = get_absolute_time();   // task fires the first paced poll
}

static void psx_process(const uint8_t* buf);

void psx_host_task(void) {
    if (!initialized) return;

    // 1) Consume the latest completed transaction (if any).
    if (psx_new) {
        psx_new = false;
        uint8_t buf[PSX_HW_BYTES];
        for (int i = 0; i < PSX_HW_BYTES; i++) buf[i] = psx_snap[i];
        psx_process(buf);
    }

    // 2) If the pad is in digital mode, re-enable analog. Safe to bit-bang here:
    //    !psx_busy means the PIO is parked (no transaction clocking the pins).
    if (connected && last_id == PSX_ID_DIGITAL &&
        !psx_busy && time_reached(psx_next_config)) {
        psx_next_config = make_timeout_time_us(300000);   // retry at most ~3 Hz
        psx_send_config();
    }

    // 3) Pace the next poll: re-fire only after the recovery interval and when no
    //    transaction is in flight, giving the controller console-like recovery
    //    (ATT stays high in between) instead of being hammered back-to-back.
    if (!psx_busy && time_reached(psx_next_poll)) {
        psx_next_poll = make_timeout_time_us(PSX_POLL_INTERVAL_US);
        psx_start_xfer();
    }
}

// Decode one completed transaction and submit a router event on state change.
static void psx_process(const uint8_t* buf) {
    bool ok = (buf[1] != 0xFF && buf[1] != 0x00);

    if (ok) {
        if (!connected || buf[1] != last_id) { connected = true; last_id = buf[1]; }

        uint32_t buttons = decode_buttons(buf[3], buf[4]);
        uint8_t rx = 128, ry = 128, lx = 128, ly = 128;
        uint8_t pressure[12] = {0};   // up,right,down,left,l2,r2,l1,r1,tri,cir,cross,sq
        bool has_pressure = false;
        if (last_id == PSX_ID_NEGCON) {
            // neGcon: only the twist is a real axis -> left stick X (steering).
            // I, II, L are analog/pressure buttons: latch them past a threshold
            // for digital output (SInput) AND pass the analog level as DS3-style
            // pressure (PS3 output transmits it; SInput ignores it).
            // A/B/R/Start/d-pad already decode from buf[3]/buf[4].
            lx = buf[5];                                        // twist
            if (buf[6] > NEGCON_BTN_THRESH) buttons |= JP_BUTTON_B1;  // I  -> Cross
            if (buf[7] > NEGCON_BTN_THRESH) buttons |= JP_BUTTON_B3;  // II -> Square
            if (buf[8] > NEGCON_BTN_THRESH) buttons |= JP_BUTTON_L1;  // L  -> L1
            // DS3 has a pressure byte per face/d-pad button. I/II/L are analog;
            // the rest are digital, so report full-on-press for them.
            has_pressure = true;
            if (buttons & JP_BUTTON_DU) pressure[0] = 0xFF;
            if (buttons & JP_BUTTON_DR) pressure[1] = 0xFF;
            if (buttons & JP_BUTTON_DD) pressure[2] = 0xFF;
            if (buttons & JP_BUTTON_DL) pressure[3] = 0xFF;
            if (buttons & JP_BUTTON_R1) pressure[7] = 0xFF;   // R
            if (buttons & JP_BUTTON_B4) pressure[8] = 0xFF;   // B -> Triangle
            if (buttons & JP_BUTTON_B2) pressure[9] = 0xFF;   // A -> Circle
            pressure[10] = buf[6];   // I  -> Cross  (analog)
            pressure[11] = buf[7];   // II -> Square (analog)
            pressure[6]  = buf[8];   // L  -> L1     (analog)
        } else if (last_id == PSX_ID_ANALOG || last_id == PSX_ID_PRESSURE) {
            rx = buf[5]; ry = buf[6]; lx = buf[7]; ly = buf[8];
        }

        // Pressure controllers (neGcon) stream every poll so analog pressure
        // updates continuously, not just when a button bit toggles.
        bool changed = !last_submitted || has_pressure ||
            buttons != last_buttons ||
            lx != last_lx || ly != last_ly || rx != last_rx || ry != last_ry;
        if (changed) {
            last_buttons = buttons;
            last_lx = lx; last_ly = ly; last_rx = rx; last_ry = ry;
            last_submitted = true;

            input_event_t e;
            init_input_event(&e);
            e.dev_addr  = PSX_DEV_ADDR;
            e.instance  = 0;
            e.type      = INPUT_TYPE_GAMEPAD;
            e.transport = INPUT_TRANSPORT_NATIVE;
            e.layout    = LAYOUT_MODERN_4FACE;
            e.buttons   = buttons;
            e.analog[ANALOG_LX] = lx; e.analog[ANALOG_LY] = ly;
            e.analog[ANALOG_RX] = rx; e.analog[ANALOG_RY] = ry;
            if (has_pressure) {
                e.has_pressure = true;
                for (int i = 0; i < 12; i++) e.pressure[i] = pressure[i];
            }
            router_submit_input(&e);
        }
    } else if (connected) {
        connected = false;
        last_id = 0xFF;
    }
}

bool psx_host_is_connected(void) {
    return connected;
}

// ============================================================================
// INPUT INTERFACE
// ============================================================================

static uint8_t psx_get_device_count(void) {
    return connected ? 1 : 0;
}

const InputInterface psx_input_interface = {
    .name = "PS1/PS2",
    .source = INPUT_SOURCE_NATIVE_PSX,
    .init = psx_host_init,
    .task = psx_host_task,
    .is_connected = psx_host_is_connected,
    .get_device_count = psx_get_device_count,
};
