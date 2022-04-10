/*
MIT License

Copyright (c) 2022 Rupert Carmichael

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdint.h>

#include "m68k.h"

#include "memory.h"
#include "emu.h"

// Helpers for reading 8, 16, and 32-bit values in the 68K address space
static inline uint8_t read08(uint8_t *ptr, uint32_t addr) {
    return ptr[addr];
}

static inline uint16_t read16(uint8_t *ptr, uint32_t addr) {
    return (ptr[addr] << 8) | ptr[addr + 1];
}

static inline uint32_t read32(uint8_t *ptr, uint32_t addr) {
    return (ptr[addr] << 24) | (ptr[addr + 1] << 16) |
        (ptr[addr + 2] << 8) | ptr[addr + 3];
}

/* 68K Memory Map
 * =====================================================================
 * |    Address Range    | Size |             Description              |
 * =====================================================================
 * | 0x000000 - 0x0fffff |   1M | Fixed Bank of 68k program ROM        |
 * ---------------------------------------------------------------------
 * | 0x100000 - 0x10f2ff |      | User RAM                             |
 * |---------------------|  64K |---------------------------------------
 * | 0x10f300 - 0x10ffff |      | System ROM-reserved RAM              |
 * ---------------------------------------------------------------------
 * | 0x110000 - 0x1fffff |  64K | User/System RAM mirror               |
 * ---------------------------------------------------------------------
 * | 0x200000 - 0x2fffff |   1M | Switchable Bank of 68K program ROM   |
 * ---------------------------------------------------------------------
 * | 0x300000 - 0x3fffff |      | Memory Mapped Registers              |
 * ---------------------------------------------------------------------
 * | 0x400000 - 0x401fff |   8K | Banked Palette RAM                   |
 * ---------------------------------------------------------------------
 * | 0x402000 - 0x7fffff |      | Palette RAM Mirror                   |
 * ---------------------------------------------------------------------
 * | 0x800000 - 0xbfffff |   8K | Memory Card                          |
 * ---------------------------------------------------------------------
 * | 0xc00000 - 0xc1ffff | 128K | BIOS ROM                             |
 * ---------------------------------------------------------------------
 * | 0xc20000 - 0xcfffff |      | BIOS ROM Mirror                      |
 * ---------------------------------------------------------------------
 * | 0xd00000 - 0xd0ffff |  64K | Backup RAM (MVS Only)                |
 * ---------------------------------------------------------------------
 * | 0xd10000 - 0xdfffff |      | Backup RAM Mirror                    |
 * ---------------------------------------------------------------------
 */

unsigned int m68k_read_memory_8(unsigned int address) {
    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        return read08(memory.rom.cpu_m68k.p, address);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        return read08(memory.ram, address);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        address &= 0xfffff;
        return read08(memory.rom.cpu_m68k.p, address + bankaddress);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch ((address >> 16) & 0x0f) {
            case 0x00: return mem68k_fetch_ctl1_byte(address);
            case 0x02: return mem68k_fetch_coin_byte(address);
            case 0x04: return mem68k_fetch_ctl2_byte(address);
            case 0x08: return mem68k_fetch_ctl3_byte(address);
            case 0x0c: return mem68k_fetch_video_byte(address);
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        return mem68k_fetch_pal_byte(address);
    }
    else if (address < 0xc00000) { // Memory Card - Mirrored every 8K (?)
        return mem68k_fetch_memcrd_byte(address);
    }
    else if (address < 0xd00000) { // BIOS ROM
        address &= 0x1ffff;
        return read08(memory.rom.bios_m68k.p, address);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        address &= 0xffff;
        return read08(memory.sram, address);
    }

    return 0xff;
}

unsigned int m68k_read_memory_16(unsigned int address) {
    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        return read16(memory.rom.cpu_m68k.p, address);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        return read16(memory.ram, address);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        address &= 0xfffff;
        return read16(memory.rom.cpu_m68k.p, address + bankaddress);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch ((address >> 16) & 0x0f) {
            case 0x00: return mem68k_fetch_ctl1_word(address);
            case 0x02: return mem68k_fetch_coin_word(address);
            case 0x04: return mem68k_fetch_ctl2_word(address);
            case 0x08: return mem68k_fetch_ctl3_word(address);
            case 0x0c: return mem68k_fetch_video_word(address);
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        return mem68k_fetch_pal_word(address);
    }
    else if (address < 0xc00000) { // Memory Card - Mirrored every 8K (?)
        return mem68k_fetch_memcrd_word(address);
    }
    else if (address < 0xd00000) { // BIOS ROM
        address &= 0x1ffff;
        return read16(memory.rom.bios_m68k.p, address);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        address &= 0xffff;
        return read16(memory.sram, address);
    }

    return 0xf0f0;
}

unsigned int m68k_read_memory_32(unsigned int address) {
    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        return read32(memory.rom.cpu_m68k.p, address);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        return read32(memory.ram, address);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        address &= 0xfffff;
        return read32(memory.rom.cpu_m68k.p, address + bankaddress);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch ((address >> 16) & 0x0f) {
            case 0x00: return mem68k_fetch_ctl1_long(address);
            case 0x02: return mem68k_fetch_coin_long(address);
            case 0x04: return mem68k_fetch_ctl2_long(address);
            case 0x08: return mem68k_fetch_ctl3_long(address);
            case 0x0c: return mem68k_fetch_video_long(address);
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        return mem68k_fetch_pal_long(address);
    }
    else if (address < 0xc00000) { // Memory Card - Mirrored every 8K (?)
        return mem68k_fetch_memcrd_long(address);
    }
    else if (address < 0xd00000) { // BIOS ROM
        address &= 0x1ffff;
        return read32(memory.rom.bios_m68k.p, address);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        address &= 0xffff;
        return read32(memory.sram, address);
    }

    return 0xff00ff00;
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
    address &= 0xffffff;

    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        gn_log(GN_LOG_WRN, "68K write to Program ROM: %06x %02x\n",
            address, value);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        mem68k_store_ram_byte(address, value);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        mem68k_store_bk_normal_byte(address, value);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        // Any writes to 0x300001 reset the watchdog via the DOGE pin
        if (address == 0x300001)
            memory.watchdog = 0;

        switch ((address >> 16) & 0x0f) {
            case 0x02: mem68k_store_z80_byte(address, value); break;
            case 0x08: mem68k_store_pd4990_byte(address, value); break;
            case 0x0c: mem68k_store_video_byte(address, value); break;
            case 0x0a: mem68k_store_setting_byte(address, value); break;
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        mem68k_store_pal_byte(address, value);
    }
    else if (address < 0xc00000) { // Memory Card - Mirrored every 8K (?)
        mem68k_store_memcrd_byte(address, value);
    }
    else if (address < 0xd00000) { // BIOS ROM
        gn_log(GN_LOG_WRN, "68K write to BIOS ROM: %06x %02x\n",
            address, value);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        mem68k_store_sram_byte(address, value);
    }
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
    address &= 0xffffff;

    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        gn_log(GN_LOG_WRN, "68K write to Program ROM: %06x %04x\n",
            address, value);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        mem68k_store_ram_word(address, value);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        mem68k_store_bk_normal_word(address, value);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        // Any writes to 0x300001 reset the watchdog via the DOGE pin
        if (address == 0x300001)
            memory.watchdog = 0;

        switch ((address >> 16) & 0x0f) {
            case 0x02: mem68k_store_z80_word(address, value); break;
            case 0x08: mem68k_store_pd4990_word(address, value); break;
            case 0x0c: mem68k_store_video_word(address, value); break;
            case 0x0a: mem68k_store_setting_word(address, value); break;
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        mem68k_store_pal_word(address, value);
    }
    else if (address < 0xc00000) { // Memory Card - Mirrored every 8K (?)
        mem68k_store_memcrd_word(address, value);
    }
    else if (address < 0xd00000) { // BIOS ROM
        gn_log(GN_LOG_WRN, "68K write to BIOS ROM: %06x %04x\n",
            address, value);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        mem68k_store_sram_word(address, value);
    }
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
    address &= 0xffffff;

    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        gn_log(GN_LOG_WRN, "68K write to Program ROM: %06x %08x\n",
            address, value);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        mem68k_store_ram_long(address, value);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        mem68k_store_bk_normal_long(address, value);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        // Any writes to 0x300001 reset the watchdog via the DOGE pin
        if (address == 0x300001)
            memory.watchdog = 0;

        switch ((address >> 16) & 0x0f) {
            case 0x02: mem68k_store_z80_long(address, value); break;
            case 0x08: mem68k_store_pd4990_long(address, value); break;
            case 0x0c: mem68k_store_video_long(address, value); break;
            case 0x0a: mem68k_store_setting_long(address, value); break;
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        mem68k_store_pal_long(address, value);
    }
    else if (address < 0xc00000) { // Memory Card - Mirrored every 8K (?)
        mem68k_store_memcrd_long(address, value);
    }
    else if (address < 0xd00000) { // BIOS ROM
        gn_log(GN_LOG_WRN, "68K write to BIOS ROM: %06x %08x\n",
            address, value);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        mem68k_store_sram_long(address, value);
    }
}

static inline void swapb16_range(void *ptr, size_t len) {
    uint16_t *x = (uint16_t*)ptr;
    for (size_t i = 0; i < len >> 1; ++i)
        x[i] = (x[i] << 8) | (x[i] >> 8);
}

void cpu_68k_bankswitch(uint32_t address) {
    bankaddress = address;
}

void cpu_68k_reset(void) {
    m68k_pulse_reset();
}

void cpu_68k_init(void) {
    // Initialize the 68K CPU
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);

    // Byteswap the ROM data to avoid having to do so on every read
    swapb16_range(memory.rom.bios_m68k.p, memory.rom.bios_m68k.size);
    swapb16_range(memory.rom.cpu_m68k.p, memory.rom.cpu_m68k.size);
    swapb16_range(memory.game_vector, 0x80);
}

int cpu_68k_run(uint32_t nb_cycle) {
    return m68k_execute(nb_cycle);
}

uint32_t cpu_68k_getpc(void) {
    return 0;
}

int cpu_68k_run_step(void) {
    return m68k_execute(1);
}

void cpu_68k_interrupt(int a) {
    m68k_set_irq(a);
}

int cpu_68k_getcycle(void) {
    return 0;
}
