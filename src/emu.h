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

#ifndef _EMU_H_
#define _EMU_H_

#define SIZE_1K     0x000400
#define SIZE_2K     0x000800
#define SIZE_4K     0x001000
#define SIZE_8K     0x002000
#define SIZE_16K    0x004000
#define SIZE_32K    0x008000
#define SIZE_64K    0x010000

#define NUMINPUTS_NG 2

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

#endif
