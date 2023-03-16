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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gngeo.h"
#include "gngeo_z80.h"
#include "ymfm/ymfm_opn.h"
#include "z80/z80.h"

static z80 z80ctx;

static uint8_t *mromdata = NULL;
static size_t mromsize = 0;
static uint8_t zram[SIZE_2K];
static uint32_t zbank[4];
static unsigned nmi_enabled = 0;

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
        return mromdata[addr];
    else if (addr < 0xc000) // Switchable Bank 0
        return mromdata[zbank[0] + (addr & 0x3fff)];
    else if (addr < 0xe000) // Switchable Bank 1
        return mromdata[zbank[1] + (addr & 0x1fff)];
    else if (addr < 0xf000) // Switchable Bank 2
        return mromdata[zbank[2] + (addr & 0x0fff)];
    else if (addr < 0xf800) // Switchable Bank 3
        return mromdata[zbank[3] + (addr & 0x07ff)];

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
static inline void gn_z80_bankswap(uint8_t bank, uint16_t port) {
    /* The NEO-ZMC switches ROM banks through Z80 IO Port reads. The 8 most
       significant bits of the port address are used to determine the ROM bank
       to swap in.
    */
    switch (bank) {
        case 0:
            zbank[0] = ((port >> 8) & 0x0f) * SIZE_16K;
            break;
        case 1:
            zbank[1] = ((port >> 8) & 0x1f) * SIZE_8K;
            break;
        case 2:
            zbank[2] = ((port >> 8) & 0x3f) * SIZE_4K;
            break;
        case 3:
            zbank[3] = ((port >> 8) & 0x7f) * SIZE_2K;
            break;
    }
}

static uint8_t gn_z80_port_rd(z80 *userdata, uint16_t port) {
    if (userdata) { } // Unused

    switch (port & 0xff) {
        case 0x00: {
            // Acknowledge NMI - to implement
            return sound_code;
        }
        case 0x04: case 0x05: case 0x06: case 0x07: {
            return ym2610_read(port);
        }
        case 0x08: {
            gn_z80_bankswap(3, port);
            break;
        }
        case 0x09: {
            gn_z80_bankswap(2, port);
            break;
        }
        case 0x0a: {
            gn_z80_bankswap(1, port);
            break;
        }
        case 0x0b: {
            gn_z80_bankswap(0, port);
            break;
        }
        case 0x0c: {
            gn_log(GN_LOG_WRN, "Z80 port read 0x0c\n");
            break;
        }
        case 0x0e: {
            // Unknown, many games read from this port (and write to 0x0d)
            break;
        }
        case 0x18: {
            gn_log(GN_LOG_WRN, "Z80 port read 0x18\n");
            break;
        }
        default: {
            gn_log(GN_LOG_WRN, "Z80 port read 0x%04x\n", port);
            break;
        }
    }
    return 0;
}

static void gn_z80_port_wr(z80 *userdata, uint16_t port, uint8_t value) {
    if (userdata) { } // Unused

    switch (port & 0xff) {
        case 0x00: case 0xc0: { // FIXME: Port decoding can be improved
            sound_code = 0;
            break;
        }
        case 0x04: case 0x05: case 0x06: case 0x07: {
            ym2610_write(port, value);
            break;
        }
        case 0x08: case 0x09: case 0x0a: case 0x0b: {
            nmi_enabled = 1;
            break;
        }
        case 0x0c: {
            result_code = value;
            break;
        }
        case 0x0d: {
            // Unknown, many games write to this port (and read from 0x0e)
            break;
        }
        case 0x18: {
            nmi_enabled = 0;
            break;
        }
        default: {
            gn_log(GN_LOG_WRN, "Z80 port write: %04x, %02x\n", port, value);
            break;
        }
    }
}

// Reset the Z80
void gn_z80_reset(void) {
    z80_reset(&z80ctx);
    nmi_enabled = 0;
}

// Initialize the Z80
void gn_z80_init(void) {
    /* The NEO-ZMC initializes all banks to 0 (verified on hardware):
         https://wiki.neogeodev.org/index.php?title=Z80_bankswitching
       Some games did not do any bankswitching at all, so initial values are
       set to handle such cases.
    */
    //zbank[0] = zbank[1] = zbank[2] = zbank[3] = 0x0000;
    zbank[0] = 0x8000;
    zbank[1] = 0xc000;
    zbank[2] = 0xe000;
    zbank[3] = 0xf000;

    z80_init(&z80ctx);
    z80ctx.read_byte = &gn_z80_mem_rd;
    z80ctx.write_byte = &gn_z80_mem_wr;
    z80ctx.port_in = &gn_z80_port_rd;
    z80ctx.port_out = &gn_z80_port_wr;
}

// Run at least N Z80 cycles
int gn_z80_run(unsigned cycs) {
    return z80_step_n(&z80ctx, cycs);
}

// Pulse the Z80 NMI line
void gn_z80_nmi(void) {
    if (nmi_enabled)
        z80_pulse_nmi(&z80ctx);
}

// Assert the Z80 IRQ line
void gn_z80_assert_irq(unsigned l) {
    z80_assert_irq(&z80ctx, l & 0xff);
}

// Clear the Z80 IRQ line
void gn_z80_clear_irq(void) {
    z80_clr_irq(&z80ctx);
}

// Assign M ROM pointer and size
void gn_z80_romdata(uint8_t *m, size_t msz) {
    mromdata = m;
    mromsize = msz;
}
