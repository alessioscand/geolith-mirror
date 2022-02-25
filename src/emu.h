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
    SYS_ARCADE=0,
    SYS_HOME,
    SYS_UNIBIOS,
    SYS_MAX
} SYSTEM;

typedef enum COUNTRY {
    CTY_JAPAN=0,
    CTY_EUROPE,
    CTY_USA,
    CTY_ASIA,
    CTY_MAX
} COUNTRY;

struct _conf_t {
    char *game;
    uint16_t x_start;
    uint16_t y_start;
    uint16_t res_x;
    uint16_t res_y;
    uint16_t sample_rate;
    uint16_t test_switch;
    uint8_t raster;
    uint8_t rom_type;
    uint8_t special_bios;
    uint8_t extra_xor;
    uint8_t pal;
    SYSTEM system;
    COUNTRY country;
};
extern struct _conf_t conf;

enum {
    HOTKEY_MASK_A = 0x1,
    HOTKEY_MASK_B = 0x2,
    HOTKEY_MASK_C = 0x4,
    HOTKEY_MASK_D = 0x8,
};

enum {
    BUT_A = 0,
    BUT_B,
    BUT_C,
    BUT_D,
    BUT_START,
    BUT_COIN,
    KB_UP,
    KB_DOWN,
    KB_LEFT,
    KB_RIGHT,
    BUT_HOTKEY0,
    BUT_HOTKEY1,
    BUT_HOTKEY2,
    BUT_HOTKEY3
};
enum {
    AXE_X = 6,
    AXE_Y,
    AXE_X_DIR,
    AXE_Y_DIR
};

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
