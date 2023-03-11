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

#ifndef GNGEO_H
#define GNGEO_H

#define VERSION "1.0.0-pre1"

#define SIZE_1K     0x000400
#define SIZE_2K     0x000800
#define SIZE_4K     0x001000
#define SIZE_8K     0x002000
#define SIZE_16K    0x004000
#define SIZE_32K    0x008000
#define SIZE_64K    0x010000

#define SYSTEM_AES  0x00 // Console
#define SYSTEM_MVS  0x01 // Arcade
#define SYSTEM_UNI  0x02 // Universe BIOS

#define REGION_US   0x00 // USA
#define REGION_JP   0x01 // Japan
#define REGION_AS   0x02 // Asia
#define REGION_EU   0x03 // Europe

#define NUMINPUTS_NG 2

enum gn_loglevel {
    GN_LOG_DBG,
    GN_LOG_INF,
    GN_LOG_WRN,
    GN_LOG_ERR,
    GN_LOG_SCR
};

typedef struct ROM_REGION {
    uint8_t* p;
    size_t size;
}ROM_REGION;

typedef struct GAME_ROMS {
    ROM_REGION cpu_m68k;
    ROM_REGION cpu_z80;
    ROM_REGION bios_sfix;
    ROM_REGION bios_audio;
    ROM_REGION bios_m68k;
}GAME_ROMS;

#include "video.h"

typedef struct neo_mem {
    GAME_ROMS rom;
    uint8_t ram[0x10000];
    VIDEO vid;

    uint8_t nvram[0x10000];

    uint8_t fix_board_usage[4096];
    uint8_t *fix_game_usage;

    uint8_t game_vector[0x80];
    int   current_vector;

    /* crypted rom bankswitch system */
    uint32_t bksw_handler;
    uint8_t *bksw_unscramble;
    uint32_t *bksw_offset;

    uint8_t memcard[0x800];
} neo_mem;

int gn_bios_load(const char*);
void gn_bios_unload(void);

int gn_memcard_load(const char*);
int gn_memcard_save(const char*);

int gn_nvram_load(const char*);
int gn_nvram_save(const char*);

void gn_exec(void);
void gn_init(void);
void gn_reset(void);

void gn_log_set_callback(void (*)(int, const char *, ...));
void gn_input_set_callback(unsigned, unsigned (*)(unsigned));
void gn_input_stat_set_callback(unsigned, unsigned (*)(void));

int gn_get_region(void);
void gn_set_region(int);

int gn_get_system(void);
void gn_set_system(int);

extern void (*gn_log)(int, const char *, ...);

extern unsigned (*gn_input_cb[NUMINPUTS_NG])(unsigned);
extern unsigned (*gn_input_stat_cb[NUMINPUTS_NG])(void);

extern uint8_t reg_dipsw;

void gn_watchdog_reset(void);

// Timer Interrupt (IRQ2)
extern uint8_t irq2_ctrl;
extern uint32_t irq2_reload;
extern int64_t irq2_counter;

extern neo_mem memory;

/* Sound control */
extern uint8_t sound_code;
extern uint8_t result_code;

/* 68k cpu Banking control */
extern uint32_t bankaddress;            /* current bank */

#endif
