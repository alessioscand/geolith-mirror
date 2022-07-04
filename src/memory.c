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
