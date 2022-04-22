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

#include "emu.h"
#include "memory.h"
#include "pd4990a.h"

#define SMATAP 0x98ec // NEO-SMA Tapped bits - 2, 3, 5, 6, 7, 11, 12, and 15

// Z80 command
uint8_t sound_code;

// Pseudo-Random Number addresses
static uint32_t addr_prn[2] = { 0, 0 };
static uint16_t prn = 0x2345; // Pseudo-random number initial value

// Interrupt status
static uint32_t irqlines = 0;
static uint32_t irqack = 0;

static inline uint16_t parity(uint16_t v) {
    /* This technique is used, adapted for 16-bit values:
       https://graphics.stanford.edu/~seander/bithacks.html#ParityParallel
    */
    v ^= v >> 8;
    v ^= v >> 4;
    v &= 0xf;
    return (0x6996 >> v) & 1;
}

static inline uint16_t gn_m68k_prn_read(void) {
    prn = (prn << 1) | parity(prn & SMATAP);
    return prn;
}

// Helpers for reading 8, 16, and 32-bit values in the 68K address space
static inline uint8_t read08(uint8_t *ptr, uint32_t addr) {
    return ptr[addr];
}

static inline uint16_t read16(uint8_t *ptr, uint32_t addr) {
    return (ptr[addr] << 8) | ptr[addr + 1];
}

static inline uint16_t read16be(uint8_t *ptr, uint32_t addr) {
    return (ptr[addr + 1] << 8) | ptr[addr];
}

static inline void write08(uint8_t *ptr, uint32_t addr, uint8_t data) {
    ptr[addr] = data;
}

static inline void write16(uint8_t *ptr, uint32_t addr, uint16_t data) {
    ptr[addr + 1] = data & 0xff;
    ptr[addr] = data >> 8;
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
 * | 0x800000 - 0xbfffff |   4M | Memory Card                          |
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
        if (memory.bksw_unscramble) {
            // Read SMA Chip Presence
            if (address == 0x2fe446)
                return 0x37; // Always return this value for SMA presence
            else if (address == 0x2fe447)
                return 0x9a; // Always return this value for SMA presence
            else if (address == addr_prn[0] || address == addr_prn[1])
                return gn_m68k_prn_read() & 0xff;
        }
        address &= 0xfffff;
        return read08(memory.rom.cpu_m68k.p, address + bankaddress);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch (address) {
            case 0x300000: { // REG_P1CNT
                return gn_input_cb[0](0);
            }
            case 0x300001: { // REG_DIPSW
                /* Hardware DIP Switches (Active Low)
                   Bit 7:    Stop Mode
                   Bit 6:    Free Play
                   Bit 5:    Enable Multiplayer
                   Bits 3-4: Comm. ID Code
                   Bit 2:    0 = Normal Controller, 1 = Mahjong Keyboard
                   Bit 1:    0 = 1 Chute, 1 = 2 Chutes
                   Bit 0:    Settings Mode
                */
                return ~reg_dipsw; // Complemented because it is active low
            }
            case 0x300081: { // REG_SYSTYPE
                /* Bit 7:    Test button - FIXME more documentation...
                   Bit 6:    0 = 2 slots, 1 = 4 or 6 slots
                   Bits 0-5: Unknown
                */
                return 0xc0; // Active Low
            }
            case 0x320000: { // REG_SOUND
                return result_code; // Z80 Reply Code
            }
            case 0x320001: { // REG_STATUS_A
                /* Bit 7:    RTC Data Bit
                   Bit 6:    RTC Time Pulse
                   Bit 5:    0 = 4 Slot, 1 = 6 Slot
                   Bit 4:    Coin-in 4
                   Bit 3:    Coin-in 3
                   Bit 2:    Service Button
                   Bit 1:    Coin-in 2
                   Bit 0:    Coin-in 1
                */
                return gn_input_stat_cb[0]() |
                    (read_4990_testbit() << 6) |
                    (read_4990_databit() << 7);
            }
            case 0x340000: { // REG_P2CNT
                return gn_input_cb[1](1);
            }
            case 0x380000: { // REG_STATUS_B
                /* Aux Inputs (Lower bits Active Low)
                   Bit 7:    0 = AES, 1 = MVS
                   Bit 6:    Memory card write protect
                   Bit 4-5:  Memory card inserted if 00
                   Bit 3:    Player 2 Select
                   Bit 2:    Player 2 Start
                   Bit 1:    Player 1 Select
                   Bit 0:    Player 1 Start
                */
                return gn_input_stat_cb[1]();
            }
            case 0x3c0000: case 0x3c0002: case 0x3c0008: case 0x3c000a: {
                // REG_VRAMADDR, REG_VRAMRW, REG_TIMERHIGH, REG_TIMERLOW
                return memory.vid.ram[memory.vid.vptr] & 0xff;
            }
            case 0x3c0004: case 0x3c000c: {
                // REG_VRAMMOD, REG_IRQACK
                return memory.vid.modulo & 0xff;
            }
            case 0x3c0006: case 0x3c000e: {
                /* REG_LSPCMODE, REG_TIMERSTOP
                   Bits 7-15: Raster line counter (with offset of 0xf8)
                   Bits 4-6:  000
                   Bit 3:     0 = 60Hz, 1 = 50Hz
                   Bits 0-2:  Auto animation counter
                */
                return (((current_line + 0xf8) << 7) |
                    (conf.pal << 3) |
                    (neogeo_frame_counter & 0x07)) & 0xff;
            }
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        return read08(current_pal, address & 0x1fff);
    }
    else if (address < 0xc00000) { // Memory Card
        //gn_log(GN_LOG_DBG, "8-bit Memory Card Read: %06x\n", address);
        /* 8-bit Memory Card reads return 0xff for odd addresses, and memcard
           data for even addresses. This is effectively half of a 16-bit read.
        */
        if (address & 0x01)
            return 0xff;
        address = (address >> 1) & 0x7ff;
        return memory.memcard[address];
    }
    else if (address < 0xd00000) { // BIOS ROM
        address &= 0x1ffff;
        return read08(memory.rom.bios_m68k.p, address);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        address &= 0xffff;
        return read08(memory.sram, address);
    }

    gn_log(GN_LOG_WRN, "Unknown 8-bit 68K Read at %06x\n", address);
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
        if (memory.bksw_unscramble) {
            // Read SMA Chip Presence at 0x2fe447 - mask bit 0 for 16-bit reads
            if (address == 0x2fe446)
                return 0x9a37; // Always return this value for SMA presence
            else if (address == addr_prn[0] || address == addr_prn[1])
                return gn_m68k_prn_read();
        }
        address &= 0xfffff;
        return read16(memory.rom.cpu_m68k.p, address + bankaddress);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch (address) {
            case 0x3c0000: case 0x3c0002: case 0x3c0008: case 0x3c000a: {
                // REG_VRAMADDR, REG_VRAMRW, REG_TIMERHIGH, REG_TIMERLOW
                return memory.vid.ram[memory.vid.vptr];
            }
            case 0x3c0004: case 0x3c000c: {
                // REG_VRAMMOD, REG_IRQACK
                return (uint16_t)memory.vid.modulo;
            }
            case 0x3c0006: case 0x3c000e: {
                /* REG_LSPCMODE, REG_TIMERSTOP
                   Bits 7-15: Raster line counter (with offset of 0xf8)
                   Bits 4-6:  000
                   Bit 3:     0 = 60Hz, 1 = 50Hz
                   Bits 0-2:  Auto animation counter
                */
                return ((current_line + 0xf8) << 7) |
                    (conf.pal << 3) |
                    (neogeo_frame_counter & 0x07);
            }
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        return read16be(current_pal, address & 0x1fff);
    }
    else if (address < 0xc00000) { // Memory Card
        /* Emulate an 8-bit, 2K memory card - since only the low byte is used,
           the address must be divided by two to get the correct byte. The
           upper byte is always 0xff.
        */
        address = (address >> 1) & 0x7ff;
        return memory.memcard[address] | 0xff00;
    }
    else if (address < 0xd00000) { // BIOS ROM
        address &= 0x1ffff;
        return read16(memory.rom.bios_m68k.p, address);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        address &= 0xffff;
        return read16(memory.sram, address);
    }

    gn_log(GN_LOG_WRN, "Unknown 16-bit 68K Read at %06x\n", address);
    return 0xf0f0;
}

unsigned int m68k_read_memory_32(unsigned int address) {
    return (m68k_read_memory_16(address) << 16) |
        m68k_read_memory_16(address + 2);
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
    address &= 0xffffff;

    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        gn_log(GN_LOG_WRN, "68K write to Program ROM: %06x %02x\n",
            address, value);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        write08(memory.ram, address, value & 0xff);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        mem68k_store_bk_normal_byte(address, value);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch (address) {
            // Writes to 0x300001 reset watchdog timer via DOGE pin
            case 0x300001: {
                memory.watchdog = 0;
                break;
            }
            case 0x320000: { // REG_SOUND
                // FIXME: Sync or run extra Z80 cycles to fix wjammers
                sound_code = value & 0xff;
                cpu_z80_nmi();
                break;
            }
            case 0x380051: { // REG_RTCCTRL
                pd4990a_serial_control(value & 0x07);
                break;
            }
            case 0x3c0000: case 0x3c0002: case 0x3c0004: case 0x3c0006:
            case 0x3c0008: case 0x3c000a: case 0x3c000c: case 0x3c000e: {
                /* Byte writes are only effective on even addresses, and they
                   store the same data in both bytes.
                */
                m68k_write_memory_16(address, (value << 8) | (value & 0xff));
                break;
            }
        }

        switch ((address >> 16) & 0x0f) {
            case 0x0a: mem68k_store_setting_byte(address, value); break;
        }
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        gn_log(GN_LOG_WRN, "Palette RAM 8-bit write: %08x, %02x\n",
            address, value);
    }
    else if (address < 0xc00000) { // Memory Card - Mirrored every 8K (?)
        mem68k_store_memcrd_byte(address, value);
    }
    else if (address < 0xd00000) { // BIOS ROM
        gn_log(GN_LOG_WRN, "68K write to BIOS ROM: %06x %02x\n",
            address, value);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        if (!sram_lock)
            memory.sram[address & 0xffff] = value;
    }
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
    address &= 0xffffff;

    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        gn_log(GN_LOG_WRN, "68K write to Program ROM: %06x %04x\n",
            address, value);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        write16(memory.ram, address, value & 0xffff);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        mem68k_store_bk_normal_word(address, value);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch (address) {
            // Writes to 0x300001 reset watchdog timer via DOGE pin
            case 0x300001: {
                memory.watchdog = 0;
                return;
            }
            case 0x320000: { // REG_SOUND
                sound_code = (value >> 8) & 0xff; // Use the upper byte
                cpu_z80_nmi();
                return;
            }
            case 0x380051: { // REG_RTCCTRL
                pd4990a_serial_control(value & 0x07);
                return;
            }
            case 0x3c0000: { // REG_VRAMADDR
                memory.vid.vptr = value & 0xffff;
                return;
            }
            case 0x3c0002: { // REG_VRAMRW
                // Write to VRAM (modulo is applied after)
                memory.vid.ram[memory.vid.vptr] = value;
                memory.vid.vptr += memory.vid.modulo;
                return;
            }
            case 0x3c0004: { // REG_VRAMMOD
                /* Set the signed value automatically added to the VRAM address
                   after a write
                */
                memory.vid.modulo = ((int16_t)value);
                return;
            }
            case 0x3c0006: { // REG_LSPCMODE
                /* Bits 8-15: Auto animation speed - tick every N+1 frames
                   Bits 7-5:  Timer interrupt mode
                   Bit 4:     Timer interrupt enable
                   Bit 3:     Disable auto animation
                   Bits 0-2:  Unused
                */
                neogeo_frame_counter_speed = (((value >> 8) & 0xff) + 1);
                memory.vid.irq2control = value & 0xff;
                return;
            }
            case 0x3c0008: { // REG_TIMERHIGH
                memory.vid.irq2pos =
                    (memory.vid.irq2pos & 0xffff) | ((uint32_t)value << 16);
                return;
            }
            case 0x3c000a: { // REG_TIMERLOW
                memory.vid.irq2pos =
                    (memory.vid.irq2pos & 0xffff0000) | (value & 0xffff);
                return;
            }
            case 0x3c000c: { // REG_IRQACK
                /* Bit 2: Ack VBlank
                   Bit 1: Ack HBlank
                   Bit 0: Ack IRQ3
                */
                irqack = value;
                return;
            }
            case 0x3c000e: { // REG_TIMERSTOP
                /* Bit 0=1: Stop timer counter during first and last 16 lines
                   (32 total) when in PAL mode -- FIXME
                */
                return;
            }
        }
        gn_log(GN_LOG_WRN, "Unknown 16-bit 68K Write: %06x %04x\n",
            address, value);
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
        if (!sram_lock)
            write16(memory.sram, address & 0xffff, value);
    }
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
    m68k_write_memory_16(address, value >> 16);
    m68k_write_memory_16(address + 2, value & 0xffff);
}

void cpu_68k_bankswitch(uint32_t address) {
    bankaddress = address;
}

void cpu_68k_reset(void) {
    m68k_pulse_reset();
    prn = 0x2345;
}

void cpu_68k_init(void) {
    // Initialize the 68K CPU
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
}

int cpu_68k_run(uint32_t nb_cycle) {
    return m68k_execute(nb_cycle);
}

int cpu_68k_run_step(void) {
    return m68k_execute(1);
}

void cpu_68k_interrupt(int a) {
    irqlines |= a;
    m68k_set_irq(irqlines);
}

int cpu_68k_getcycle(void) {
    return 0;
}

// Acknowledge interrupts
int gn_m68k_int_ack(int level) {
    if (level) { } // Not used, but part of Musashi's function prototype

    if (irqack & 0x04) // VBlank
        irqlines &= ~0x01;
    if (irqack & 0x02) // HBlank/Timer
        irqlines &= ~0x02;
    if (irqack & 0x01) // IRQ3 - Pending after reset
        irqlines &= ~0x04;

    m68k_set_irq(irqlines);
    return M68K_INT_ACK_AUTOVECTOR;
}

// Set the pseudo-random number generator read addresses
void gn_m68k_prn_addrs(uint32_t a1, uint32_t a2) {
    addr_prn[0] = a1;
    addr_prn[1] = a2;
}
