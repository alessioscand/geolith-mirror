/*
Copyright (c) 2022-2023 Rupert Carmichael
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stddef.h>
#include <stdint.h>

#include "m68k/m68k.h"

#include "gngeo_m68k.h"
#include "gngeo_z80.h"
#include "gngeo.h"

#include "video.h"

#define SMATAP 0x98ec // NEO-SMA Tapped bits - 2, 3, 5, 6, 7, 11, 12, and 15

// BIOS ROM data
static uint8_t *biosdata = NULL;
static size_t biossize = 0;
static uint8_t *sfixdata = NULL;
static size_t sfixsize = 0;

// Game ROM data
static uint8_t *promdata = NULL;
static size_t promsize = 0;
static uint8_t *sromdata = NULL;
static size_t sromsize = 0;

// Main RAM
static uint8_t ram[SIZE_64K];

// Cartridge RAM
static uint8_t cartram[SIZE_8K];

// Switchable bank offset address
static uint32_t banksw_addr = 0;

// Vector Table currently in use
static uint8_t vectable = VECTOR_TABLE_BIOS;

// Registers
static uint8_t reg_sramlock = 0; // SRAM Lock
static uint8_t reg_dipsw = 0; // DIP Switches

// NEO-SMA Pseudo-Random Number addresses
static uint32_t sma_prn_addr[2] = { 0, 0 };
static uint16_t sma_prn = 0x2345; // Pseudo-random number initial value

// NEO-SMA bankswitching
static uint32_t sma_addr = 0x00000000;
static uint32_t *sma_offset;
static uint8_t *sma_scramble;

// Metal Slug X Protection
static uint16_t mslugx_command = 0;
static uint16_t mslugx_counter = 0;

// Z80 command
uint8_t sound_code;
uint8_t result_code;

// Function pointers for handling switchable bank area
static uint8_t (*gn_m68k_read_banksw_8)(uint32_t);
static uint16_t (*gn_m68k_read_banksw_16)(uint32_t);
static void (*gn_m68k_write_banksw_8)(uint32_t, uint8_t);
static void (*gn_m68k_write_banksw_16)(uint32_t, uint16_t);

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
    sma_prn = (sma_prn << 1) | parity(sma_prn & SMATAP);
    return sma_prn;
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

static inline void write16be(uint8_t *ptr, uint32_t addr, uint16_t data) {
    ptr[addr] = data & 0xff;
    ptr[addr + 1] = data >> 8;
}

static inline void swapb16_range(void *ptr, size_t len) {
    uint16_t *x = (uint16_t*)ptr;
    for (size_t i = 0; i < len >> 1; ++i)
        x[i] = (x[i] << 8) | (x[i] >> 8);
}

// Default Switchable Bank Routines
static uint8_t gn_m68k_read_banksw_8_default(uint32_t addr) {
    return read08(promdata, (addr & 0xfffff) + banksw_addr);
}

static uint16_t gn_m68k_read_banksw_16_default(uint32_t addr) {
    return read16(promdata, (addr & 0xfffff) + banksw_addr);
}

static void gn_m68k_write_banksw_8_default(uint32_t addr, uint8_t data) {
    if (addr >= 0x2ffff0)
        banksw_addr = ((data * 0x100000) + 0x100000) & 0xffffff;
    else
        gn_log(GN_LOG_WRN, "8-bit write at %06x\n", addr);
}

static void gn_m68k_write_banksw_16_default(uint32_t addr, uint16_t data) {
    if (addr >= 0x2ffff0)
        banksw_addr = ((data * 0x100000) + 0x100000) & 0xffffff;
    else
        gn_log(GN_LOG_WRN, "16-bit write at %06x\n", addr);
}

// NEO-SMA Switchable Bank Routines
static uint8_t gn_m68k_read_banksw_8_sma(uint32_t addr) {
    // Read SMA Chip Presence
    if (addr == 0x2fe446)
        return 0x37; // Always return this value for SMA presence
    else if (addr == 0x2fe447)
        return 0x9a; // Always return this value for SMA presence
    else if (addr == sma_prn_addr[0] || addr == sma_prn_addr[1])
        return gn_m68k_prn_read() & 0xff;

    return read08(promdata, (addr & 0xfffff) + banksw_addr);
}

static uint16_t gn_m68k_read_banksw_16_sma(uint32_t addr) {
    // Read SMA Chip Presence at 0x2fe447 - mask bit 0 for 16-bit reads
    if (addr == 0x2fe446)
        return 0x9a37; // Always return this value for SMA presence
    else if (addr == sma_prn_addr[0] || addr == sma_prn_addr[1])
        return gn_m68k_prn_read();

    return read16(promdata, (addr & 0xfffff) + banksw_addr);
}

static void gn_m68k_write_banksw_16_sma(uint32_t addr, uint16_t data) {
    /* If the NEO-SMA bankswitch address is written to, the data must be
       unscrambled. A 6-bit bank value is derived by shifting the original
       value by 6 Lookup Table (LUT) entries, and then ORing the least
       significant bits together in their corresponding position. For example,
       the 4th value may be derived by shifting the data being written by the
       4th LUT entry, and the least significant bit will be in the 4th position
       of the final unscrambled value.
    */
    if (addr == sma_addr) {
        uint8_t unscrambled = 0;

        for (size_t i = 0; i < 6; ++i)
            unscrambled |= (((data >> sma_scramble[i]) & 0x01) << i);

        /* A 64 entry LUT is used to determine the offset into P ROM. The
           6-bit unscrambled value derived above serves as the index into the
           Bankswitch LUT, which contains the memory offset value.
        */
        banksw_addr = (0x100000 + sma_offset[unscrambled]) & 0xffffff;
    }
}

// Set the SMA LUTs and pseudo-random number generator read addresses
void gn_m68k_sma_init(uint32_t *a, uint32_t *b, uint8_t *s) {
    sma_prn_addr[0] = a[0];
    sma_prn_addr[1] = a[1];
    sma_addr = a[2];
    sma_offset = b;
    sma_scramble = s;
}

// NEO-PVC Switchable Bank Routines

/* PVC Palette Protection Packing/Unpacking
   Colour data may be packed or unpacked. Packed values are 16-bit while
   unpacked values are 32-bit. No data is lost packing or unpacking, as a byte
   is used to store R, G, B, (5 bits) and D (1 bit) each. Packing simply merges
   the data into a 16-bit value with no space wasted.

   Packed Format:
   DrgbRRRR GGGGBBBB

   Unpacked Format:
   0000000D 000RRRRr 000GGGGg 000BBBBb
*/

// Unpack a value from 0x2fffe0 to 0x2fffe2 and 0x2fffe4
static inline void gn_m68k_pvc_unpack(void) {
    uint8_t d = cartram[0x1fe1] >> 7; // 0000 000D
    uint8_t r = // 000R RRRr
        ((cartram[0x1fe1] & 0x40) >> 6) | ((cartram[0x1fe1] & 0x0f) << 1);
    uint8_t g = // 000G GGGg
        ((cartram[0x1fe1] & 0x20) >> 5) | ((cartram[0x1fe0] & 0xf0) >> 3);
    uint8_t b = // 000B BBBb
        ((cartram[0x1fe1] & 0x10) >> 4) | ((cartram[0x1fe0] & 0x0f) << 1);

    cartram[0x1fe5] = d;
    cartram[0x1fe4] = r;
    cartram[0x1fe3] = g;
    cartram[0x1fe2] = b;
}

// Pack a value from 0x2fffe8 and 0x2fffea to 0x2fffec
static inline void gn_m68k_pvc_pack(void) {
    uint8_t d = cartram[0x1feb] & 0x01;
    uint8_t r = cartram[0x1fea] & 0x1f;
    uint8_t g = cartram[0x1fe9] & 0x1f;
    uint8_t b = cartram[0x1fe8] & 0x1f;

    cartram[0x1fec] = (b >> 1) | ((g & 0x1e) << 3); // GGGG BBBB
    cartram[0x1fed] = (r >> 1) | ((b & 0x01) << 4) | ((g & 0x01) << 5) |
        ((r & 0x01) << 6) | (d << 7); // Drgb RRRR
}

// Swap Banks (PVC)
static inline void gn_m68k_pvc_bankswap(void) {
    /* The offset into ROM for the Switchable Bank Area is a 24-bit value:
       0x2ffff2 is used for the upper 16 bits, and the most significant byte of
       0x2ffff0 is used for the lower 8 bits. This is again offset by 0x100000.
       The magic numbers applied after calculating the bank offset come from
       both FB Neo and MAME - further information is seemingly unavailable.
    */
    uint32_t bankaddress =
        (cartram[0x1ff3] << 16) | (cartram[0x1ff2] << 8) | cartram[0x1ff1];

    cartram[0x1ff0] = 0xa0;
    cartram[0x1ff1] &= 0xfe;
    cartram[0x1ff3] &= 0x7f;

    banksw_addr = (bankaddress + 0x100000) & 0xffffff;
}

static uint8_t gn_m68k_read_banksw_8_pvc(uint32_t addr) {
    if (addr >= 0x2fe000)
        return cartram[(addr & 0x1fff) ^ 1];

    return read08(promdata, (addr & 0xfffff) + banksw_addr);
}

static uint16_t gn_m68k_read_banksw_16_pvc(uint32_t addr) {
    if (addr >= 0x2fe000)
        return read16be(cartram, addr & 0x1fff);

    return read16(promdata, (addr & 0xfffff) + banksw_addr);
}

static void gn_m68k_write_banksw_8_pvc(uint32_t addr, uint8_t data) {
    if (addr >= 0x2fe000)
        write08(cartram, (addr & 0x1fff) ^ 1, data & 0xff);

    if (addr >= 0x2fffe0 && addr <= 0x2fffe3) // Unpack
        gn_m68k_pvc_unpack();
    else if (addr >= 0x2fffe8 && addr <= 0x2fffeb) // Pack
        gn_m68k_pvc_pack();
    else if (addr >= 0x2ffff0 && addr <= 0x2ffff3) // Bankswap
        gn_m68k_pvc_bankswap();
}

static void gn_m68k_write_banksw_16_pvc(uint32_t addr, uint16_t data) {
    if (addr >= 0x2fe000)
        write16be(cartram, addr & 0x1fff, data & 0xffff);

    if (addr >= 0x2fffe0 && addr <= 0x2fffe3) // Unpack
        gn_m68k_pvc_unpack();
    else if (addr >= 0x2fffe8 && addr <= 0x2fffeb) // Pack
        gn_m68k_pvc_pack();
    else if (addr >= 0x2ffff0 && addr <= 0x2ffff3) // Bankswap
        gn_m68k_pvc_bankswap();
}

// Set up NEO-PVC
void gn_m68k_pvc_init(void) {
    // Zero the memory - should this be randomized instead?
    for (size_t i = 0; i < 0x2000; ++i)
        cartram[i] = 0x00;
}

// Jockey Grand Prix - Cartridge RAM mapped to 0x200000-0x201fff
static uint8_t gn_m68k_read_banksw_8_jockeygp(uint32_t addr) {
    return read08(cartram, addr & 0x1fff);
}

static uint16_t gn_m68k_read_banksw_16_jockeygp(uint32_t addr) {
    return read16(cartram, addr & 0x1fff);
}

static void gn_m68k_write_banksw_8_jockeygp(uint32_t addr, uint8_t data) {
    write08(cartram, addr & 0x1fff, data);
}

static void gn_m68k_write_banksw_16_jockeygp(uint32_t addr, uint16_t data) {
    write16(cartram, addr & 0x1fff, data);
}

// Metal Slug X - Challenge/Response Protection
/* There is no real documentation available on this beyond the code, but the
   same code is used in any emulators supporting this board type without dirty
   soft patching. It's magic!
*/
static uint16_t gn_m68k_read_banksw_16_mslugx(uint32_t addr) {
    if (addr >= 0x2fffe0 && addr <= 0x2fffef) {
        switch (mslugx_command) {
            case 0x0001: {
                uint16_t ret =
                    (read08(promdata, 0xdedd2 + ((mslugx_counter >> 3) & 0xfff))
                    >> (~mslugx_counter & 0x07)) & 0x0001;
                ++mslugx_counter;
                return ret;
            }
            case 0x0fff: {
                int32_t select = read16(ram, 0xf00a) - 1;
                return (read08(promdata, 0xdedd2 + ((select >> 3) & 0x0fff))
                    >> (~select & 0x07)) & 0x0001;
            }
            default: {
                gn_log(GN_LOG_WRN, "mslugx read: %06x\n", addr);
                break;
            }
        }
    }

    return read16(promdata, (addr & 0xfffff) + banksw_addr);
}

static void gn_m68k_write_banksw_16_mslugx(uint32_t addr, uint16_t data) {
    if (addr >= 0x2fffe0 && addr <= 0x2fffef) {
        switch (addr) {
            case 0x2fffe0: {
                mslugx_command = 0;
                break;
            }
            case 0x2fffe2: case 0x2fffe4: {
                mslugx_command |= data;
                break;
            }
            case 0x2fffe6: {
                break;
            }
            case 0x2fffea: {
                mslugx_command = 0;
                mslugx_counter = 0;
                break;
            }
            default: {
                gn_log(GN_LOG_WRN, "mslugx write: %06x, %04x\n", addr, data);
                break;
            }
        }
    }
    else if (addr >= 0x2ffff0) {
        banksw_addr = ((data * 0x100000) + 0x100000) & 0xffffff;
    }
    else {
        gn_log(GN_LOG_WRN, "16-bit write at %06x\n", addr);
    }
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
    if (address < 0x000080) { // Vector Table
        return vectable ?
            read08(promdata, address) : read08(biosdata, address);
    }
    else if (address < 0x100000) { // Fixed 1M Program ROM Bank
        return read08(promdata, address);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        return read08(ram, address);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        return gn_m68k_read_banksw_8(address);
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
                return gn_input_stat_cb[0](); // FIXME add RTC bits
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
                return gn_input_stat_cb[1]() |
                    (gn_get_system() == SYSTEM_AES ? 0x00 : 0x80);
            }
            case 0x3c0000: case 0x3c0002: case 0x3c0008: case 0x3c000a: {
                // REG_VRAMADDR, REG_VRAMRW, REG_TIMERHIGH, REG_TIMERLOW
                return vid.ram[vid.vptr] & 0xff;
            }
            case 0x3c0004: case 0x3c000c: {
                // REG_VRAMMOD, REG_IRQACK
                return vid.modulo & 0xff;
            }
            case 0x3c0006: case 0x3c000e: {
                /* REG_LSPCMODE, REG_TIMERSTOP
                   Bits 7-15: Raster line counter (with offset of 0xf8)
                   Bits 4-6:  000
                   Bit 3:     0 = 60Hz, 1 = 50Hz
                   Bits 0-2:  Auto animation counter
                */
                return (((current_line + 0xf8) << 7) |
                    //(conf.pal << 3) | // FIXME restore when PAL is supported
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
        return memcard[(address >> 1) & 0x7ff];
    }
    else if (address < 0xd00000) { // BIOS ROM
        address &= 0x1ffff;
        return read08(biosdata, address);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        address &= 0xffff;
        return read08(nvram, address);
    }

    gn_log(GN_LOG_WRN, "Unknown 8-bit 68K Read at %06x\n", address);
    return 0xff;
}

unsigned int m68k_read_memory_16(unsigned int address) {
    if (address < 0x000080) { // Vector Table
        return vectable ?
            read16(promdata, address) : read16(biosdata, address);
    }
    else if (address < 0x100000) { // Fixed 1M Program ROM Bank
        return read16(promdata, address);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        return read16(ram, address);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        return gn_m68k_read_banksw_16(address);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch (address) {
            case 0x300000: { // REG_P1CNT
                uint8_t val = gn_input_cb[0](0);
                return (val << 8) | val; // Is this the correct way? FIXME
            }
            case 0x340000: { // REG_P2CNT
                uint8_t val = gn_input_cb[1](1);
                return (val << 8) | val; // Is this the correct way? FIXME
            }
            case 0x380000: { // REG_STATUS_B
                uint8_t val = gn_input_stat_cb[1]() |
                    (gn_get_system() == SYSTEM_AES ? 0x00 : 0x80);
                return (val << 8) | val; // Is this the correct way? FIXME
            }
            case 0x3c0000: case 0x3c0002: case 0x3c0008: case 0x3c000a: {
                // REG_VRAMADDR, REG_VRAMRW, REG_TIMERHIGH, REG_TIMERLOW
                return vid.ram[vid.vptr];
            }
            case 0x3c0004: case 0x3c000c: {
                // REG_VRAMMOD, REG_IRQACK
                return (uint16_t)vid.modulo;
            }
            case 0x3c0006: case 0x3c000e: {
                /* REG_LSPCMODE, REG_TIMERSTOP
                   Bits 7-15: Raster line counter (with offset of 0xf8)
                   Bits 4-6:  000
                   Bit 3:     0 = 60Hz, 1 = 50Hz
                   Bits 0-2:  Auto animation counter
                */
                return ((current_line + 0xf8) << 7) |
                    //(conf.pal << 3) | // FIXME restore when PAL is supported
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
        return memcard[(address >> 1) & 0x7ff] | 0xff00;
    }
    else if (address < 0xd00000) { // BIOS ROM
        address &= 0x1ffff;
        return read16(biosdata, address);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        address &= 0xffff;
        return read16(nvram, address);
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
        gn_log(GN_LOG_DBG, "68K write to Program ROM: %06x %02x\n",
            address, value);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        write08(ram, address, value & 0xff);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        gn_m68k_write_banksw_8(address, value);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch (address) {
            // Writes to 0x300001 reset watchdog timer via DOGE pin
            case 0x300001: {
                gn_watchdog_reset();
                return;
            }
            case 0x320000: { // REG_SOUND
                sound_code = value & 0xff;
                gn_z80_nmi();
                return;
            }
            case 0x380001: { // REG_POUTPUT
                // Do joypad outputs need to be emulated?
                return;
            }
            case 0x380011: { // REG_CRDBANK
                // TODO - memory card bank selection, lowest 3 bits
                return;
            }
            case 0x380021: { // REG_SLOT
                // TODO?
                return;
            }
            case 0x380031: { // REG_LEDLATCHES
                return;
            }
            case 0x380041: { // REG_LEDDATA
                return;
            }
            case 0x380051: { // REG_RTCCTRL
                return;
            }
            case 0x380061: // REG_RESETCC1, REG_RESETCC1 - Reset Coin Counters
            case 0x380063: {
                return;
            }
            case 0x380065: // REG_RESETCL1, REG_RESETCL2 - Reset Coin Lockouts
            case 0x380067: {
                return;
            }
            case 0x3800e1: // REG_SETCC1, REG_SETCC1 - Set Coin Counters
            case 0x3800e3: {
                return;
            }
            case 0x3a0001: { // REG_NOSHADOW
                gn_log(GN_LOG_DBG, "REG_NOSHADOW write\n");
                return;
            }
            case 0x3a0003: { // REG_SWPBIOS
                vectable = VECTOR_TABLE_BIOS;
                gn_log(GN_LOG_DBG, "Selected BIOS vector table\n");
                return;
            }
            case 0x3a0005: { // REG_CRDUNLOCK1
                gn_log(GN_LOG_DBG, "REG_CRDUNLOCK1 write: %02x\n", value);
                return;
            }
            case 0x3a0007: { // REG_CRDLOCK2
                gn_log(GN_LOG_DBG, "REG_CRDLOCK2 write: %02x\n", value);
                return;
            }
            case 0x3a0009: { // REG_CRDREGSEL
                gn_log(GN_LOG_DBG, "REG_CRDREGSEL write: %02x\n", value);
                return;
            }
            case 0x3a000b: { // REG_BRDFIX
                current_fix = sfixdata;
                fix_usage = fix_board_usage;
                vid.currentfix = 0;
                return;
            }
            case 0x3a000d: { // REG_SRAMLOCK
                reg_sramlock = 1;
                return;
            }
            case 0x3a000f: { // REG_PALBANK1
                palette_set(1);
                return;
            }
            case 0x3a0011: { // REG_SHADOW
                gn_log(GN_LOG_DBG, "REG_SHADOW write: %02x\n", value);
                return;
            }
            case 0x3a0013: { // REG_SWPROM
                vectable = VECTOR_TABLE_CART;
                gn_log(GN_LOG_DBG, "Selected Cartridge vector table\n");
                return;
            }
            case 0x3a0015: { // REG_CRDLOCK1
                gn_log(GN_LOG_DBG, "REG_CRDLOCK1 write: %02x\n", value);
                return;
            }
            case 0x3a0017: { // REG_CRDUNLOCK2
                gn_log(GN_LOG_DBG, "REG_CRDUNLOCK2 write: %02x\n", value);
                return;
            }
            case 0x3a0019: { // REG_CRDREGSEL
                gn_log(GN_LOG_DBG, "REG_CRDREGSEL write: %02x\n", value);
                return;
            }
            case 0x3a001b: { // REG_CRTFIX
                current_fix = sromdata;
                fix_usage = fix_game_usage;
                vid.currentfix = 1;
                return;
            }
            case 0x3a001d: { // REG_SRAMUNLOCK
                reg_sramlock = 0;
                return;
            }
            case 0x3a001f: { // REG_PALBANK0
                palette_set(0);
                return;
            }
            case 0x3c0000: case 0x3c0002: case 0x3c0004: case 0x3c0006:
            case 0x3c0008: case 0x3c000a: case 0x3c000c: case 0x3c000e: {
                /* Byte writes are only effective on even addresses, and they
                   store the same data in both bytes.
                */
                m68k_write_memory_16(address, (value << 8) | (value & 0xff));
                return;
            }
        }
        gn_log(GN_LOG_WRN, "Unknown 8-bit write: %06x, %02x\n",
            address, value);
    }
    else if (address < 0x800000) { // Palette RAM - Mirrored every 8K (?)
        gn_log(GN_LOG_WRN, "Palette RAM 8-bit write: %06x, %02x\n",
            address, value);
    }
    else if (address < 0xc00000) { // Memory Card - Mirrored every 8K (?)
        memcard[(address >> 1) & 0x7ff] = value;
    }
    else if (address < 0xd00000) { // BIOS ROM
        gn_log(GN_LOG_WRN, "68K write to BIOS ROM: %06x %02x\n",
            address, value);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        if (!reg_sramlock)
            nvram[address & 0xffff] = value;
    }
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
    address &= 0xffffff;

    if (address < 0x100000) { // Fixed 1M Program ROM Bank
        gn_log(GN_LOG_DBG, "68K write to Program ROM: %06x %04x\n",
            address, value);
    }
    else if (address < 0x200000) { // RAM - Mirrored every 64K
        address &= 0xffff;
        write16(ram, address, value & 0xffff);
    }
    else if (address < 0x300000) { // Switchable 1M Program ROM Bank
        gn_m68k_write_banksw_16(address, value);
    }
    else if (address < 0x400000) { // Memory Mapped Registers
        switch (address) {
            // Writes to 0x300001 reset watchdog timer via DOGE pin
            case 0x300001: {
                gn_watchdog_reset();
                return;
            }
            case 0x320000: { // REG_SOUND
                sound_code = (value >> 8) & 0xff; // Use the upper byte
                gn_z80_nmi();
                return;
            }
            case 0x380051: { // REG_RTCCTRL
                return;
            }
            case 0x3c0000: { // REG_VRAMADDR
                vid.vptr = value & 0xffff;
                return;
            }
            case 0x3c0002: { // REG_VRAMRW
                // Write to VRAM (modulo is applied after)
                vid.ram[vid.vptr] = value;
                vid.vptr += vid.modulo;
                vid.vptr &= 0xffff;
                return;
            }
            case 0x3c0004: { // REG_VRAMMOD
                /* Set the signed value automatically added to the VRAM address
                   after a write
                */
                vid.modulo = ((int16_t)value);
                return;
            }
            case 0x3c0006: { // REG_LSPCMODE
                /* Bits 8-15: Auto animation speed - tick every N+1 frames
                   Bits 7-5:  Timer interrupt mode
                   Bit 4:     Timer interrupt enable
                   Bit 3:     Disable auto animation
                   Bits 0-2:  Unused
                */
                vid.fc_speed = (((value >> 8) & 0xff) + 1);
                irq2_ctrl = value & 0xff;
                return;
            }
            case 0x3c0008: { // REG_TIMERHIGH
                irq2_reload = (irq2_reload & 0xffff) | (value << 16);
                return;
            }
            case 0x3c000a: { // REG_TIMERLOW
                irq2_reload = (irq2_reload & 0xffff0000) | (value & 0xffff);

                // Reload counter when REG_TIMERLOW is written
                if (irq2_ctrl & IRQ_TIMER_RELOAD_WRITE)
                    irq2_counter = irq2_reload;

                return;
            }
            case 0x3c000c: { // REG_IRQACK
                /* Bit 2: Ack VBlank
                   Bit 1: Ack HBlank
                   Bit 0: Ack IRQ3 (Reset)
                */
                if (value & 0x04) // VBlank
                    m68k_set_virq(IRQ_VBLANK, 0);
                if (value & 0x02) // HBlank/Timer
                    m68k_set_virq(IRQ_TIMER, 0);
                if (value & 0x01) // IRQ3 - Pending after reset
                    m68k_set_virq(IRQ_RESET, 0);

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
        memcard[(address >> 1) & 0x7ff] = value & 0xff;
    }
    else if (address < 0xd00000) { // BIOS ROM
        gn_log(GN_LOG_WRN, "68K write to BIOS ROM: %06x %04x\n",
            address, value);
    }
    else if (address < 0xe00000) { // Backup RAM - Mirrored every 64K (?)
        if (!reg_sramlock)
            write16(nvram, address & 0xffff, value);
    }
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
    m68k_write_memory_16(address, value >> 16);
    m68k_write_memory_16(address + 2, value & 0xffff);
}

void gn_m68k_reset(void) {
    m68k_pulse_reset();
    sma_prn = 0x2345;
    reg_sramlock = 0;

    if (promsize > 0x100000)
        banksw_addr = 0x100000;
    else
        banksw_addr = 0;
}

void gn_m68k_init(void) {
    // Initialize the 68K CPU
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
}

int gn_m68k_run(unsigned cycs) {
    return m68k_execute(cycs);
}

void gn_m68k_interrupt(unsigned level) {
    if ((m68k_get_virq(level)) == 0)
        m68k_set_virq(level, 1);
}

// Acknowledge interrupts
int gn_m68k_int_ack(int level) {
    if (level) { }
    //gn_log(GN_LOG_INF, "IRQ ACK Level: %02x\n", level);
    return M68K_INT_ACK_AUTOVECTOR;
}

void gn_m68k_banksw_set(unsigned swtype) {
    switch (swtype) {
        case BANKSW_SMA: // NEO-SMA Bankswitching
            gn_m68k_read_banksw_8 = &gn_m68k_read_banksw_8_sma;
            gn_m68k_read_banksw_16 = &gn_m68k_read_banksw_16_sma;
            gn_m68k_write_banksw_8 = &gn_m68k_write_banksw_8_default;
            gn_m68k_write_banksw_16 = &gn_m68k_write_banksw_16_sma;
            break;
        case BANKSW_PVC: // NEO-PVC Bankswitching
            gn_m68k_read_banksw_8 = &gn_m68k_read_banksw_8_pvc;
            gn_m68k_read_banksw_16 = &gn_m68k_read_banksw_16_pvc;
            gn_m68k_write_banksw_8 = &gn_m68k_write_banksw_8_pvc;
            gn_m68k_write_banksw_16 = &gn_m68k_write_banksw_16_pvc;
            break;
        case BANKSW_JOCKEYGP: // Jockey Grand Prix
            gn_m68k_read_banksw_8 = &gn_m68k_read_banksw_8_jockeygp;
            gn_m68k_read_banksw_16 = &gn_m68k_read_banksw_16_jockeygp;
            gn_m68k_write_banksw_8 = &gn_m68k_write_banksw_8_jockeygp;
            gn_m68k_write_banksw_16 = &gn_m68k_write_banksw_16_jockeygp;
            break;
        case BANKSW_MSLUGX: // Metal Slug X
            gn_m68k_read_banksw_8 = &gn_m68k_read_banksw_8_default;
            gn_m68k_read_banksw_16 = &gn_m68k_read_banksw_16_mslugx;
            gn_m68k_write_banksw_8 = &gn_m68k_write_banksw_8_default;
            gn_m68k_write_banksw_16 = &gn_m68k_write_banksw_16_mslugx;
            break;
        default: // Default
            gn_m68k_read_banksw_8 = &gn_m68k_read_banksw_8_default;
            gn_m68k_read_banksw_16 = &gn_m68k_read_banksw_16_default;
            gn_m68k_write_banksw_8 = &gn_m68k_write_banksw_8_default;
            gn_m68k_write_banksw_16 = &gn_m68k_write_banksw_16_default;
            break;
    }
}

void gn_m68k_romdata_bios(uint8_t *p, size_t psz, uint8_t *s, size_t ssz) {
    biosdata = p;
    biossize = psz;
    sfixdata = s;
    sfixsize = ssz;
}

void gn_m68k_romdata_game(uint8_t *p, size_t psz, uint8_t *s, size_t ssz) {
    promdata = p;
    promsize = psz;
    sromdata = s;
    sromsize = ssz;
}

void gn_m68k_postload(void) {
    // Byteswap the BIOS and P ROM
    swapb16_range(biosdata, biossize);
    swapb16_range(promdata, promsize);

    if (gn_get_system() == SYSTEM_AES) {
        current_fix = sromdata;
        fix_usage = fix_game_usage;
    }
    else {
        current_fix = sfixdata;
        fix_usage = fix_board_usage;
    }
}
