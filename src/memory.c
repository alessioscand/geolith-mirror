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

#include <stdint.h>
#include <string.h>

#include "emu.h"
#include "video.h"
#include "memory.h"
#include "pd4990a.h"

neo_mem memory;

uint8_t *current_pal;
uint32_t *current_pc_pal;
uint8_t *current_fix;
uint8_t *fix_usage;
uint8_t sram_lock;
uint8_t result_code;

uint32_t bankaddress = 0;

/**** PALETTE ****/
/*static inline */uint32_t convert_pal(uint16_t npal) {
    int r = 0, g = 0, b = 0, dark = 0;
    r = ((npal >> 7) & 0x1e) | ((npal >> 14) & 0x01);
    g = ((npal >> 3) & 0x1e) | ((npal >> 13) & 0x01);
    b = ((npal << 1) & 0x1e) | ((npal >> 12) & 0x01);
    dark = ((npal >> 15) & 0x01) ^ 1;

    /* 2^6 bits components (neogeo) -> 2^8 bits components (RGB24) */
    r = (r << 1 | dark) << 2;
    g = (g << 1 | dark) << 2;
    b = (b << 1 | dark) << 2;
    return (r << 16) + (g << 8) + b;
}

void mem68k_store_pal_word(uint32_t addr, uint16_t data) {
    addr &= 0xffff;
    if (addr <= 0x1fff) {
        current_pal[addr] = data & 0xff;
        current_pal[addr + 1] = data >> 8;

        if ((addr >> 1) & 0xF)
            current_pc_pal[(addr) >> 1] = convert_pal(data);
        else
            current_pc_pal[(addr) >> 1] = 0xF81F;
    }
}

/**** SETTINGS ****/
void mem68k_store_setting_byte(uint32_t addr, uint8_t data) {
    if (data) { } // FIXME: Reduce number of parameters?
    //printf("mem68k_store_setting_byte %08x\n",addr);
    addr &= 0xFFFF;
    if (addr == 0x0003) {
        gn_log(GN_LOG_DBG, "Selected BIOS vector\n");
        memcpy(memory.rom.cpu_m68k.p, memory.rom.bios_m68k.p, 0x80);
        memory.current_vector=0;
    }

    if (addr == 0x0013) {
        gn_log(GN_LOG_DBG, "Selected game vector\n");
        memcpy(memory.rom.cpu_m68k.p, memory.game_vector, 0x80);
        memory.current_vector=1;
    }

    if (addr == 0x000b) { /* select board fix */
        current_fix = memory.rom.bios_sfix.p;
        fix_usage = memory.fix_board_usage;
        memory.vid.currentfix=0;
        return;
    }
    if (addr == 0x001b) { /* select game fix */
        current_fix = memory.rom.game_sfix.p;
        fix_usage = memory.fix_game_usage;
        memory.vid.currentfix=1;
        return;
    }
    if (addr == 0x000d) { /* sram lock */
        sram_lock = 1;
        return;
    }
    if (addr == 0x001d) { /* sram unlock */
        sram_lock = 0;
        return;
    }
    if (addr == 0x000f) { /* set palette 2 */
        current_pal = memory.vid.pal_neo[1];
        current_pc_pal = (uint32_t *) memory.vid.pal_host[1];
        memory.vid.currentpal=1;
        return;
    }
    if (addr == 0x001f) { /* set palette 1 */
        current_pal = memory.vid.pal_neo[0];
        current_pc_pal = (uint32_t *) memory.vid.pal_host[0];
        memory.vid.currentpal = 0;
        return;
    }
    /* garou write 0 to 3a0001 -> enable display, 3a0011 -> disable display */
    //printf("unknow mem68k_store_setting_byte %x %x\n",addr,data);

}
