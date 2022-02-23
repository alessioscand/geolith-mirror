/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 * 
 *  This program is free software; you can redistribute it and/or modify  
 *  it under the terms of the GNU General Public License as published by   
 *  the Free Software Foundation; either version 2 of the License, or    
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "emu.h"
#include "memory.h"
#include "pd4990a.h"
#include "debug.h"

#include "timer.h"

#include "ym2610/2610intf.h"
#include "neocrypt.h"
#include "conf.h"

#ifdef HAVE_EMUDBG_H
#include <emudbg.h>
#include "generator68k/generator.h"
#include "generator68k/mem68k.h"
#include "generator68k/cpu68k.h"
#endif

struct _conf_t conf;

uint8_t *joy_button[2];
int32_t *joy_axe[2];
uint32_t joy_numaxes[2];

int frame;
int nb_interlace = 256;
int current_line;

static int fc;
static int last_line;

static uint32_t cpu_68k_timeslice = 200000;
static uint32_t cpu_68k_timeslice_scanline = 200000 / 264.0;
static uint32_t cpu_z80_timeslice = 73333;
static uint32_t tm_cycle = 0;
static uint32_t cpu_z80_timeslice_interlace = 200000 / 256.0;

void setup_misc_patch(char *name) {
	if (!strcmp(name, "ssideki")) {
		WRITE_WORD_ROM(&memory.rom.cpu_m68k.p[0x2240], 0x4e71);
	}

	if (!strcmp(name, "mslugx")) {
		/* patch out protection checks */
		int i;
		uint8_t *RAM = memory.rom.cpu_m68k.p;
		for (i = 0; i < memory.rom.cpu_m68k.size; i += 2) {
			if ((READ_WORD_ROM(&RAM[i + 0]) == 0x0243)
					&& (READ_WORD_ROM(&RAM[i + 2]) == 0x0001) && /* andi.w  #$1, D3 */
			(READ_WORD_ROM(&RAM[i + 4]) == 0x6600)) { /* bne xxxx */

				WRITE_WORD_ROM(&RAM[i + 4], 0x4e71);
				WRITE_WORD_ROM(&RAM[i + 6], 0x4e71);
			}
		}

		WRITE_WORD_ROM(&RAM[0x3bdc], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3bde], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3be0], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3c0c], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3c0e], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3c10], 0x4e71);

		WRITE_WORD_ROM(&RAM[0x3c36], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3c38], 0x4e71);
	}

}

void neogeo_reset(void) {
    //	memory.vid.modulo = 1; /* TODO: Move to init_video */
	sram_lock = 0;
	sound_code = 0;
	pending_command = 0;
	result_code = 0;

	if (memory.rom.cpu_m68k.size > 0x100000)
		cpu_68k_bankswitch(0x100000);
	else
		cpu_68k_bankswitch(0);
	cpu_68k_reset();

}

void init_sound(void) {
	cpu_z80_init();
	YM2610_sh_start();
}

void init_neo(void) {
	neogeo_init_save_state();
	cpu_68k_init();
	pd4990a_init();
	init_sound();
	neogeo_reset();
}

static inline int neo_interrupt(void) {
    static int frames;

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
		memory.vid.irq2start = (memory.vid.irq2pos + 3) / 0x180; /* ridhero gives 0x17d */
	else
		memory.vid.irq2start = 1000;

	if (last_line < 21) { /* there was no IRQ2 while the beam was in the
						 * visible area -> no need for scanline rendering */
		draw_screen();
	} else {
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
		if ((last_line >= 21) && (current_line >= 20))
			draw_screen_scanline(last_line - 21, current_line - 20, 0);
		last_line = current_line;
	}
	++current_line;
	return memory.vid.irq2taken;
}

void main_loop(void) {
	if (conf.test_switch == 1)
		conf.test_switch = 0;

	/*if (handle_event()) {
		reset_event();
    }*/

	for (int i = 0; i < nb_interlace; ++i) {
		cpu_z80_run(cpu_z80_timeslice_interlace);
		my_timer();
	}

	if (conf.raster) {
		current_line = 0;
		for (int i = 0; i < 264; ++i) {
			tm_cycle = cpu_68k_run(cpu_68k_timeslice_scanline - tm_cycle);
			if (update_scanline())
				cpu_68k_interrupt(2);
		}
		tm_cycle = cpu_68k_run(cpu_68k_timeslice_scanline - tm_cycle);

		update_screen();
		++memory.watchdog;
		if (memory.watchdog > 7) {
            printf("WATCHDOG RESET\n");
			cpu_68k_reset();
        }
		cpu_68k_interrupt(1);
	}
	else {
		tm_cycle = cpu_68k_run(cpu_68k_timeslice - tm_cycle);
		int a = neo_interrupt();
		/* state handling (we save/load before interrupt) */
		//state_handling(pending_save_state, pending_load_state);

		++memory.watchdog;

		if (memory.watchdog > 7) { // Watchdog reset after ~0.13 == ~7.8 frames
            printf("WATCHDOG RESET %d\n",memory.watchdog);
			cpu_68k_reset();
        }

		if (a) {
			cpu_68k_interrupt(a);
		}
	}
}

void cpu_68k_dpg_step(void) {
	static uint32_t nb_cycle;
	static uint32_t line_cycle;
	uint32_t cpu_68k_timeslice = 200000;
	uint32_t cpu_68k_timeslice_scanline = 200000 / (float) 262;
	uint32_t cycle;
	if (nb_cycle == 0) {
		main_loop(); /* update event etc. */
	}
	cycle = cpu_68k_run_step();
	add_bt(cpu_68k_getpc());
	line_cycle += cycle;
	nb_cycle += cycle;
	if (nb_cycle >= cpu_68k_timeslice) {
		nb_cycle = line_cycle = 0;
		if (conf.raster) {
			update_screen();
		} else {
			neo_interrupt();
		}
		//state_handling(pending_save_state, pending_load_state);
		cpu_68k_interrupt(1);
	} else {
		if (line_cycle >= cpu_68k_timeslice_scanline) {
			line_cycle = 0;
			if (conf.raster) {
				if (update_scanline())
					cpu_68k_interrupt(2);
			}
		}
	}
}
