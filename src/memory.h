/*
 * Gngeo JG
 *
 * Copyright (c) 2001-2015 Mathieu Peponas
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

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "video.h"
#include "roms.h"

typedef struct neo_mem {
        GAME_ROMS rom;
        uint8_t ram[0x10000];
        VIDEO vid;
        uint8_t *ng_lo;                          /* Put it in memory.vid? use zoom table in rom */

        uint32_t nb_of_tiles;

        uint8_t sram[0x10000];

    //  uint32_t *pen_usage;                      /* TODO: it's also in rom  */
        uint8_t fix_board_usage[4096];
        uint8_t *fix_game_usage;

        uint8_t game_vector[0x80];
        int   current_vector;

        /* crypted rom bankswitch system */

        uint32_t bksw_handler;

        uint8_t *bksw_unscramble;
        int *bksw_offset;

        uint8_t memcard[0x800];

        uint32_t watchdog;
} neo_mem;

extern neo_mem memory;

/* video related */
extern uint8_t *current_pal;
extern uint32_t *current_pc_pal;
extern uint8_t *current_fix;
extern uint8_t *fix_usage;
extern unsigned current_line;

/* sram */
extern uint8_t sram_lock;

/* Sound control */
extern uint8_t sound_code;
extern uint8_t result_code;

/* 68k cpu Banking control */
extern uint32_t bankaddress;            /* current bank */

/* cpu 68k interface */
int cpu_68k_getcycle(void);
void cpu_68k_init(void);
void cpu_68k_reset(void);
int cpu_68k_run(uint32_t nb_cycle);
void cpu_68k_interrupt(int a);
void cpu_68k_disassemble(int pc, int nb_instr);
void cpu_68k_dumpreg(void);
int cpu_68k_run_step(void);
uint32_t cpu_68k_getpc(void);
int cpu_68k_debuger(void (*execstep)(void),void (*dump)(void));

/* cpu z80 interface */
int cpu_z80_run(int nbcycle);
void cpu_z80_nmi(void);
void cpu_z80_raise_irq(int l);
void cpu_z80_lower_irq(void);
void cpu_z80_init(void);
void cpu_z80_reset(void);

/* 68k storring function */
void mem68k_store_bk_normal_byte(uint32_t addr, uint8_t data);
void mem68k_store_bk_normal_word(uint32_t addr, uint16_t data);
void mem68k_store_pal_word(uint32_t addr, uint16_t data);
void mem68k_store_setting_byte(uint32_t addr, uint8_t data);

void gn_m68k_prn_addrs(uint32_t a1, uint32_t a2);
#endif
