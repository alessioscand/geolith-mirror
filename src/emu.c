/*
 * Gngeo JG
 *
 * Copyright (c) 2001-2015 Mathieu Peponas
 * Copyright (c) 2022-2023 Rupert Carmichael
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdint.h>
#include <stdlib.h>

#include "emu.h"
#include "gngeo_m68k.h"
#include "gngeo_mixer.h"
#include "gngeo_z80.h"
#include "pd4990a.h"

#include "ymfm/ymfm_shim.h"

#define DIV_M68K 2
#define DIV_Z80 6

#define MCYC_PER_FRAME 405504
#define MCYC_PER_LINE 1536

// Log callback
void (*gn_log)(int, const char *, ...);

// Input poll callbacks
unsigned (*gn_input_cb[NUMINPUTS_NG])(unsigned);
unsigned (*gn_input_stat_cb[NUMINPUTS_NG])(void);

struct _conf_t conf;

neo_mem memory;

uint8_t reg_dipsw = 0;

// Timer Interrupt (IRQ2)
uint8_t irq2_ctrl = 0;
uint32_t irq2_reload = 0;
int64_t irq2_counter = 0;
static unsigned irq2_frags = 0;

static unsigned fc;

// Extra cycles stored at the end of scanlines/frames
static uint32_t mextcycs = 0;
static uint32_t lextcycs = 0;
static uint32_t zextcycs = 0;

static uint32_t ymcycs = 0;
static uint32_t ymsamps = 0;

static unsigned mcycs = 0;
static unsigned zcycs = 0;
static unsigned icycs = 0;
static unsigned lcycs = 0;

// Set the log callback
void gn_log_set_callback(void (*cb)(int, const char *, ...)) {
    gn_log = cb;
}

// Set the Input Callbacks to allow the emulator to strobe the input state
void gn_input_set_callback(unsigned port, unsigned (*cb)(unsigned)) {
    gn_input_cb[port] = cb;
}
void gn_input_stat_set_callback(unsigned port, unsigned (*cb)(void)) {
    gn_input_stat_cb[port] = cb;
}

void gn_reset(void) {
    sound_code = 0;
    result_code = 0;

    if (memory.rom.cpu_m68k.size > 0x100000)
        bankaddress = 0x100000;
    else
        bankaddress = 0;

    gn_m68k_reset();
    gn_z80_reset();
    ymfm_shim_reset(); // Reset the YM2610 to make sure everything is defaulted
}

void gn_init(void) {
    gn_m68k_init();
    pd4990a_init();
    gn_z80_init();
    ymfm_shim_init();
    gn_reset();
}

void gn_exec(void) {
    mcycs = mextcycs;
    lcycs = lextcycs;
    zcycs = zextcycs;

    current_line = 0;
    ymsamps = 0;

    draw_bg();

    // Reload counter at the beginning of each frame
    if (irq2_ctrl & 0x40)
        irq2_counter = irq2_reload;

    while (mcycs < MCYC_PER_FRAME) {
        icycs = gn_m68k_run(1);
        mcycs += icycs * DIV_M68K;
        lcycs += icycs * DIV_M68K;

        // Handle IRQ2 counter
        irq2_counter -= icycs >> 1;
        irq2_frags += icycs & 0x01;
        if (irq2_frags == 2) {
            irq2_frags = 0;
            --irq2_counter;
        }

        if (irq2_counter <= 0) {
            // Reload counter when it reaches 0
            if (irq2_ctrl & 0x80)
                irq2_counter += irq2_reload;

            // Timer Interrupt Enabled, trigger when counter reaches 0
            if (irq2_ctrl & 0x10)
                gn_m68k_int(2);
        }

        while (zcycs < mcycs) {
            size_t scycs = gn_z80_run(1);
            zcycs += scycs * DIV_Z80;

            for (size_t s = 0; s < scycs; ++s) {
                if (++ymcycs == 72) { // 72 for medium fidelity, 8 for high
                    ymcycs = 0;
                    ymsamps += ymfm_shim_exec();
                }
            }
        }

        if (lcycs >= MCYC_PER_LINE) {
            lcycs -= MCYC_PER_LINE;
            if (current_line < 255)
                draw_screen_scanline(current_line, 0);
            else if (current_line == 255)
                draw_screen_scanline(current_line, 1);
            ++current_line;
        }
    }

    mextcycs = mcycs - MCYC_PER_FRAME;
    zextcycs = zcycs - MCYC_PER_FRAME;
    lextcycs = lcycs;

    //// Old code, research/rewrite FIXME
    pd4990a_addretrace();
    if (fc >= neogeo_frame_counter_speed) {
        fc = 0;
        ++neogeo_frame_counter;
    }
    ++fc;
    ////

    gn_mixer_resamp(ymsamps);

    if (++memory.watchdog > 7) {
        gn_log(GN_LOG_WRN, "Watchdog reset\n");
        gn_m68k_reset();
    }

    gn_m68k_int(1);
}
