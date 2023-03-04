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

#ifndef _EMU_H_
#define _EMU_H_

#include "video.h"
#include "roms.h"

#define VERSION "0.9.0-pre1"

#define SIZE_1K     0x000400
#define SIZE_2K     0x000800
#define SIZE_4K     0x001000
#define SIZE_8K     0x002000
#define SIZE_16K    0x004000
#define SIZE_32K    0x008000
#define SIZE_64K    0x010000

#define NUMINPUTS_NG 2

#define BANKSW_DEF 0x00 // Default
#define BANKSW_SMA 0x01 // NEO-SMA

typedef enum SYSTEM {
    SYS_UNIBIOS,
    SYS_ARCADE,
    SYS_HOME,
    SYS_MAX
} SYSTEM;

typedef enum COUNTRY {
    CTY_USA,
    CTY_JAPAN,
    CTY_ASIA,
    CTY_EUROPE,
    CTY_MAX
} COUNTRY;

struct _conf_t {
    char *game;
    uint16_t sample_rate;
    uint8_t extra_xor;
    uint8_t pal;
    SYSTEM system;
    COUNTRY country;
};
extern struct _conf_t conf;

enum gn_loglevel {
    GN_LOG_DBG,
    GN_LOG_INF,
    GN_LOG_WRN,
    GN_LOG_ERR,
    GN_LOG_SCR
};

typedef struct neo_mem {
    GAME_ROMS rom;
    uint8_t ram[0x10000];
    VIDEO vid;
    uint8_t *ng_lo; /* Put it in memory.vid? use zoom table in rom */

    uint32_t nb_of_tiles;

    uint8_t nvram[0x10000];

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

void gn_exec(void);
void gn_init(void);
void gn_reset(void);

void gn_log_set_callback(void (*)(int, const char *, ...));
void gn_input_set_callback(unsigned, unsigned (*)(unsigned));
void gn_input_stat_set_callback(unsigned, unsigned (*)(void));

extern void (*gn_log)(int, const char *, ...);

extern unsigned (*gn_input_cb[NUMINPUTS_NG])(unsigned);
extern unsigned (*gn_input_stat_cb[NUMINPUTS_NG])(void);

extern uint8_t reg_dipsw;

void gn_m68k_prn_addrs(uint32_t a1, uint32_t a2);
void gn_m68k_banksw_set(unsigned swtype, uint32_t addr);



/* cpu 68k interface */
void cpu_68k_init(void);
void cpu_68k_reset(void);
int cpu_68k_run(uint32_t nb_cycle);
void cpu_68k_interrupt(int a);

extern neo_mem memory;

/* Sound control */
extern uint8_t sound_code;
extern uint8_t result_code;

/* 68k cpu Banking control */
extern uint32_t bankaddress;            /* current bank */

#endif
