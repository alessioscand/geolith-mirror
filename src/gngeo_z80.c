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

#include "emu.h"
#include "memory.h"
#include "z80.h"

static z80 z80ctx;

static uint8_t zram[SIZE_2K];
static uint32_t zbank[4];

/* Z80 Memory Map
 * =====================================================================
 * |  Address Range  | Size | Description                              |
 * =====================================================================
 * | 0x0000 - 0x7fff |  32K | Static main code bank (start of M1 ROM)  |
 * ---------------------------------------------------------------------
 * | 0x8000 - 0xbfff |  16K | Switchable Bank 0                        |
 * ---------------------------------------------------------------------
 * | 0xc000 - 0xdfff |   8K | Switchable Bank 1                        |
 * ---------------------------------------------------------------------
 * | 0xe000 - 0xefff |   4K | Switchable Bank 2                        |
 * ---------------------------------------------------------------------
 * | 0xf000 - 0xf7ff |   2K | Switchable Bank 3                        |
 * ---------------------------------------------------------------------
 * | 0xf800 - 0xffff |   2K | Work RAM                                 |
 * ---------------------------------------------------------------------
 *
 * The NEO-ZMC (Z80 Memory Controller) is a chip on the cartridge which
 * handles memory mapping.
 */
static uint8_t gn_z80_mem_rd(void *userdata, uint16_t addr) {
    if (userdata) { } // Unused

    if (addr < 0x8000) // Static main code bank
        return memory.rom.cpu_z80.p[addr];
    else if (addr < 0xc000) // Switchable Bank 0
        return memory.rom.cpu_z80.p[zbank[0] + (addr & 0x3fff)];
    else if (addr < 0xe000) // Switchable Bank 1
        return memory.rom.cpu_z80.p[zbank[1] + (addr & 0x1fff)];
    else if (addr < 0xf000) // Switchable Bank 2
        return memory.rom.cpu_z80.p[zbank[2] + (addr & 0x0fff)];
    else if (addr < 0xf800) // Switchable Bank 3
        return memory.rom.cpu_z80.p[zbank[3] + (addr & 0x07ff)];

    // Work RAM
    return zram[addr & 0x07ff];
}

static void gn_z80_mem_wr(void *userdata, uint16_t addr, uint8_t data) {
    if (userdata) { } // Unused

    if (addr > 0xf7ff)
        zram[addr & 0x07ff] = data;
    else
        gn_log(GN_LOG_WRN, "Z80 write outside RAM: %04x %02x\n", addr, data);
}

/* Z80 Port Map
 * =====================================================================
 * | Address   | Read                          | Write          | Mask |
 * =====================================================================
 * |      0x00 | Read 68K code/NMI Acknowledge | Clear 68K code | 0x0c |
 * ---------------------------------------------------------------------
 * | 0x04-0x07 | YM2610 Read                   | YM2610 Write   | 0x0c |
 * ---------------------------------------------------------------------
 * |      0x08 | Set Switchable Bank 3         | Enable NMIs    | 0x1c |
 * ---------------------------------------------------------------------
 * |      0x09 | Set Switchable Bank 2         | Enable NMIs    | 0x1c |
 * ---------------------------------------------------------------------
 * |      0x0a | Set Switchable Bank 1         | Enable NMIs    | 0x1c |
 * ---------------------------------------------------------------------
 * |      0x0b | Set Switchable Bank 0         | Enable NMIs    | 0x1c |
 * ---------------------------------------------------------------------
 * |      0x0c | SDRD1 (?)                     | Reply to 68K   | 0x0c |
 * ---------------------------------------------------------------------
 * |      0x18 | Address 0x08 (?)              | Disable NMIs   | 0x1c |
 * ---------------------------------------------------------------------
 */
static uint8_t gn_z80_port_rd(z80 *userdata, uint16_t port) {
    if (userdata) { } // Unused
    return z80_port_read(port);
}

static void gn_z80_port_wr(z80 *userdata, uint16_t port, uint8_t value) {
    if (userdata) { } // Unused
    z80_port_write(port, value);
}

void cpu_z80_switchbank(uint8_t bank, uint16_t port) {
    /* The NEO-ZMC switches ROM banks through Z80 IO Port reads. The 8 most
       significant bits of the port address are used to determine the ROM bank
       to swap in.
    */
    switch (bank) {
        case 0:
            zbank[bank] = ((port >> 8) & 0x0f) * SIZE_16K;
            break;
        case 1:
            zbank[bank] = ((port >> 8) & 0x1f) * SIZE_8K;
            break;
        case 2:
            zbank[bank] = ((port >> 8) & 0x3f) * SIZE_4K;
            break;
        case 3:
            zbank[bank] = ((port >> 8) & 0x7f) * SIZE_2K;
            break;
    }
}

void cpu_z80_init(void) {
    z80_init(&z80ctx);

    // The NEO-ZMC initializes all banks to 0 (verified on hardware)
    zbank[0] = zbank[1] = zbank[2] = zbank[3] = 0x0000;

    z80ctx.read_byte = &gn_z80_mem_rd;
    z80ctx.write_byte = &gn_z80_mem_wr;
    z80ctx.port_in = &gn_z80_port_rd;
    z80ctx.port_out = &gn_z80_port_wr;
}

int cpu_z80_run(int nbcycle) {
    return z80_step_n(&z80ctx, nbcycle);
}

void cpu_z80_nmi(void) {
    z80_gen_nmi(&z80ctx);
}

void cpu_z80_raise_irq(int l) {
    z80_gen_int(&z80ctx, l & 0xff);
}

void cpu_z80_lower_irq(void) {
    z80_clr_int(&z80ctx);
}

uint16_t cpu_z80_get_pc(void) {
    return 0;
}
