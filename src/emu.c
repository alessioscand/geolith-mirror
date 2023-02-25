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
#include "gngeo_mixer.h"
#include "memory.h"
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

static int last_line;
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
    //  memory.vid.modulo = 1; /* TODO: Move to init_video */
    sound_code = 0;
    result_code = 0;

    if (memory.rom.cpu_m68k.size > 0x100000)
        bankaddress = 0x100000;
    else
        bankaddress = 0;

    cpu_68k_reset();
    cpu_z80_reset();
    ymfm_shim_reset(); // Reset the YM2610 to make sure everything is defaulted
}

void gn_init(void) {
    cpu_68k_init();
    pd4990a_init();
    cpu_z80_init();
    ymfm_shim_init();
    gn_reset();
}

static inline int neo_interrupt(void) {
    pd4990a_addretrace();
    // printf("neogeo_frame_counter_speed %d\n",neogeo_frame_counter_speed);
    if (!(memory.vid.irq2control & 0x8)) {
        if (fc >= neogeo_frame_counter_speed) {
            fc = 0;
            ++neogeo_frame_counter;
        }
        ++fc;
    }

    draw_screen();

    return 1;
}

static inline void update_screen(void) {
    if (memory.vid.irq2control & 0x40)
        // ridhero gives 0x17d
        memory.vid.irq2start = (memory.vid.irq2pos + 3) / 0x180;
    else
        memory.vid.irq2start = 1000;

    /* there was no IRQ2 while the beam was in the visible area -> no need for
       scanline rendering
    */
    if (last_line < 21) {
        draw_screen();
    }
    else {
        draw_screen_scanline(last_line - 21, 262, 1);
    }

    last_line = 0;

    pd4990a_addretrace();
    if (fc >= neogeo_frame_counter_speed) {
        fc = 0;
        ++neogeo_frame_counter;
    }
    ++fc;
}

static inline int update_scanline(void) {
    memory.vid.irq2taken = 0;

    if (memory.vid.irq2control & 0x10) {

        if (current_line == memory.vid.irq2start) {
            if (memory.vid.irq2control & 0x80)
                memory.vid.irq2start += (memory.vid.irq2pos + 3) / 0x180;
            memory.vid.irq2taken = 1;
        }
    }

    if (memory.vid.irq2taken) {
        if (last_line < 21)
            last_line = 21;
        if (current_line < 20)
            current_line = 20;
        draw_screen_scanline(last_line - 21, current_line - 20, 0);
        last_line = current_line;
    }

    ++current_line;
    return memory.vid.irq2taken;
}

void gn_exec(void) {
    mcycs = mextcycs;
    lcycs = lextcycs;
    zcycs = zextcycs;

    while (mcycs < MCYC_PER_FRAME) {
        icycs = cpu_68k_run(1);
        mcycs += icycs * DIV_M68K;
        lcycs += icycs * DIV_M68K;

        while (zcycs < mcycs) {
            size_t scycs = cpu_z80_run(1);
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
            if (update_scanline())
                cpu_68k_interrupt(2);
        }
    }

    mextcycs = mcycs - MCYC_PER_FRAME;
    zextcycs = zcycs - MCYC_PER_FRAME;
    lextcycs = lcycs - MCYC_PER_LINE;

    update_screen();

    gn_mixer_resamp(ymsamps);
    ymsamps = 0;

    if (++memory.watchdog > 7) {
        gn_log(GN_LOG_DBG, "Watchdog reset\n");
        cpu_68k_reset();
    }

    cpu_68k_interrupt(1);
}
