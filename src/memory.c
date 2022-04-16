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

// FIXME: Take a look at many functions with unused parameters

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
uint8_t sound_code;
uint8_t result_code;

uint32_t bankaddress = 0;

static inline void write_neo_control(uint16_t data) {
    neogeo_frame_counter_speed = (((data >> 8) & 0xff) + 1);
    memory.vid.irq2control = data & 0xff;
    return;
}

static inline void write_irq2pos(uint32_t data) {
    memory.vid.irq2pos = data;
    if (memory.vid.irq2control & 0x20) {
        int line = (memory.vid.irq2pos + 0x3b) / 0x180; /* turfmast goes as low as 0x145 */
        memory.vid.irq2start = line + current_line;
    }
}

/**** RAM ****/
void mem68k_store_ram_byte(uint32_t addr, uint8_t data) {
    addr &= 0xffff;
    WRITE_BYTE_ROM(memory.ram + addr,data);
    return;
}

void mem68k_store_ram_word(uint32_t addr, uint16_t data) {
    //printf("Store rom word %08x %04x\n",addr,data);
    addr &= 0xffff;
    WRITE_WORD_ROM(memory.ram + addr,data);
    return;
}

/**** SRAM ****/
void mem68k_store_sram_byte(uint32_t addr, uint8_t data) {
    if (sram_lock)
        return;
    /*
      if (addr == 0xd00000 + sram_protection_hack && ((data & 0xff) == 0x01))
      return;
    */
    memory.sram[addr - 0xd00000] = data;
}

void mem68k_store_sram_word(uint32_t addr, uint16_t data) {
    if (sram_lock)
        return;
    /*
      if (addr == 0xd00000 + sram_protection_hack
      && ((data & 0xffff) == 0x01))
      return;
    */
    addr -= 0xd00000;
    memory.sram[addr] = data >> 8;
    memory.sram[addr + 1] = data & 0xff;
}

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

void update_all_pal(void) {
    int i;
    uint32_t *pc_pal1 = (uint32_t*) memory.vid.pal_host[0];
    uint32_t *pc_pal2 = (uint32_t*) memory.vid.pal_host[1];
    for (i = 0; i < 0x1000; i++) {
        //pc_pal1[i] = convert_pal(READ_WORD_ROM(&memory.pal1[i<<1]));
        //pc_pal2[i] = convert_pal(READ_WORD_ROM(&memory.pal2[i<<1]));
        pc_pal1[i] = convert_pal(READ_WORD(&memory.vid.pal_neo[0][i<<1]));
        pc_pal2[i] = convert_pal(READ_WORD(&memory.vid.pal_neo[1][i<<1]));
    }
}

void mem68k_store_pal_byte(uint32_t addr, uint8_t data) {
    /* TODO: verify this */
    addr &= 0xffff;
    if (addr <= 0x1fff) {
        uint16_t a = READ_WORD(&current_pal[addr & 0xfffe]);
        if (addr & 0x1)
            a = data | (a & 0xff00);
        else
            a = (a & 0xff) | (data << 8);
        WRITE_WORD(&current_pal[addr & 0xfffe], a);
        if ((addr >> 1) & 0xF)
            current_pc_pal[(addr) >> 1] = convert_pal(a);
        else
            current_pc_pal[(addr) >> 1] = 0xF81F;
    }
}

void mem68k_store_pal_word(uint32_t addr, uint16_t data) {
    //printf("Store pal word @ %08x %08x %04x\n",cpu_68k_getpc(),addr,data);
    addr &= 0xffff;
    if (addr <= 0x1fff) {
        WRITE_WORD(&current_pal[addr], data);
        if ((addr >> 1) & 0xF)
            current_pc_pal[(addr) >> 1] = convert_pal(data);
        else
            current_pc_pal[(addr) >> 1] = 0xF81F;
    }
}

/**** VIDEO ****/
void mem68k_store_video_byte(uint32_t addr, uint8_t data) {
    /* garou write at 3c001f, 3c000f, 3c0015 */
    /* wjammers write, and fetch at 3c0000 .... */
    //printf("mem68k_store_video_byte %08x %02x @pc=%08x\n",addr,data,cpu_68k_getpc());
    if (!(addr&0x1)) {
        mem68k_store_video_word(addr,(data<<8)|data);
    }
}

void mem68k_store_video_word(uint32_t addr, uint16_t data) {
    //data&=0xFFFF;
    //printf("mem68k_store_video_word %08x %04x @pc=%08x\n",addr,data,cpu_68k_getpc());
    addr &= 0xF;
    switch (addr) {
    case 0x0:
        memory.vid.vptr = data & 0xffff;
        memory.vid.rbuf = READ_WORD(&memory.vid.ram[memory.vid.vptr << 1]);
        break;
    case 0x2:
        //printf("Store %04x to video %08x @pc=%08x\n",data,vptr<<1,cpu_68k_getpc());
        WRITE_WORD(&memory.vid.ram[memory.vid.vptr << 1], data);
        memory.vid.vptr = (memory.vid.vptr & 0x8000) + ((memory.vid.vptr
                                                         + memory.vid.modulo) & 0x7fff);
        memory.vid.rbuf = READ_WORD(&memory.vid.ram[memory.vid.vptr << 1]);
        break;
    case 0x4:
        if (data&0x4000)
            data|=0x8000;
        else
            data&=0x7FFF;

        memory.vid.modulo = (int) data;
        break;
    case 0x6:
        write_neo_control(data);
        break;
    case 0x8:
        write_irq2pos((memory.vid.irq2pos & 0xffff) | ((uint32_t) data << 16));
        break;
    case 0xa:
        write_irq2pos((memory.vid.irq2pos & 0xffff0000) | (uint32_t) data);
        break;
    case 0xc:
        /* games write 7 or 4 at 0x3c000c at every frame */
        /* IRQ acknowledge */
        break;
    }

}

/**** PD4990 ****/
void mem68k_store_pd4990_byte(uint32_t addr, uint8_t data) {
    write_4990_control_w(addr, data);
}

void mem68k_store_pd4990_word(uint32_t addr, uint16_t data) {
    write_4990_control_w(addr, data);
}

void mem68k_store_pd4990_long(uint32_t addr, uint32_t data) {
    write_4990_control_w(addr, data);
}

/**** Z80 ****/
void mem68k_store_z80_byte(uint32_t addr, uint8_t data) {
    if (addr == 0x320000) {
        sound_code = data & 0xff;
        //printf("B Pending command. Sound_code=%02x\n",sound_code);
        cpu_z80_nmi();
    }
}
void mem68k_store_z80_word(uint32_t addr, uint16_t data) {
    /* tpgolf use word store for sound */
    if (addr == 0x320000) {
        sound_code = data >> 8;
        //printf("W Pending command. Sound_code=%02x\n",sound_code);
        cpu_z80_nmi();
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

void mem68k_store_setting_word(uint32_t addr, uint16_t data) {
    /* TODO: Some game use it */
    // printf("mem68k_store_setting_word USED????\n");
    mem68k_store_setting_byte(addr,data);
    return;
    addr &= 0xFFFFFe;
    if (addr == 0x3a0002) {
        memcpy(memory.rom.cpu_m68k.p, memory.rom.bios_m68k.p, 0x80);
    }

    if (addr == 0x3a0012) {
        memcpy(memory.rom.cpu_m68k.p, memory.game_vector, 0x80);
    }
    if (addr == 0x3a000a) {
        current_fix = memory.rom.bios_sfix.p;
        fix_usage = memory.fix_board_usage;
        return;
    }
    if (addr == 0x3a001a) {
        current_fix = memory.rom.game_sfix.p;
        fix_usage = memory.fix_game_usage;
        return;
    }
    if (addr == 0x3a000c) {
        sram_lock = 1;
        return;
    }
    if (addr == 0x3a001c) {
        sram_lock = 0;
        return;
    }
    if (addr == 0x3a000e) {
        current_pal = memory.vid.pal_neo[1];
        current_pc_pal = (uint32_t *) memory.vid.pal_host[1];
        return;
    }
    if (addr == 0x3a001e) {
        current_pal = memory.vid.pal_neo[0];
        current_pc_pal = (uint32_t *) memory.vid.pal_host[0];
        return;
    }
}

/**** MEMCARD ****/
void mem68k_store_memcrd_byte(uint32_t addr, uint8_t data) {
    addr &= 0xFFF;
    memory.memcard[addr >> 1] = data;
}
void mem68k_store_memcrd_word(uint32_t addr, uint16_t data) {
    //printf("memstore16\n");
    addr &= 0xFFF;
    memory.memcard[addr >> 1] = data & 0xff;
}

/**** bankswitchers ****/
static void bankswitch(uint32_t address, uint8_t data) {

    if (memory.rom.cpu_m68k.size <= 0x100000)
        return;

    if (address >= 0x2FFFF0) {
        data = data & 0x7;
        bankaddress = (data + 1) * 0x100000;
    } else
        return;

    if (bankaddress >= memory.rom.cpu_m68k.size)
        bankaddress = 0x100000;
    cpu_68k_bankswitch(bankaddress);
}

void mem68k_store_bk_normal_byte(uint32_t addr, uint8_t data) {
    //if (addr<0x2FFFF0)
    //printf("bankswitch_b %x %x\n", addr, data);
    bankswitch(addr, data);
}

void mem68k_store_bk_normal_word(uint32_t addr, uint16_t data) {
    //if (addr<0x2FFFF0)
    //printf("bankswitch_w %x %x\n",addr,data);
    if (memory.bksw_unscramble && (addr & 0xFF) == memory.bksw_unscramble[0]) {
        /* unscramble bank number */
        data =
            (((data >> memory.bksw_unscramble[1]) & 1) << 0) +
            (((data >> memory.bksw_unscramble[2]) & 1) << 1) +
            (((data >> memory.bksw_unscramble[3]) & 1) << 2) +
            (((data >> memory.bksw_unscramble[4]) & 1) << 3) +
            (((data >> memory.bksw_unscramble[5]) & 1) << 4) +
            (((data >> memory.bksw_unscramble[6]) & 1) << 5);

        bankaddress = 0x100000 + memory.bksw_offset[data];
        cpu_68k_bankswitch(bankaddress);
    } else
        bankswitch(addr, data);
}
