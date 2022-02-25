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

#ifndef _EMU_H_
#define _EMU_H_

#include <string.h>
#include <stdint.h>

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
    uint16_t test_switch;
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

void main_loop(void);
void init_neo(void);
void cpu_68k_dpg_step(void);
void setup_misc_patch(char *name);
void neogeo_reset(void);
void gn_log_set_callback(void (*)(int, const char *, ...));

extern void (*gn_log)(int, const char *, ...);

#endif
