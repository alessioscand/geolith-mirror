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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geo.h"
#include "geo_lspc.h"
#include "geo_m68k.h"
#include "geo_z80.h"
#include "geo_neo.h"
#include "geo_sma.h"

#include "ymfm/ymfm_shim.h"

extern int neogeo_fix_bank_type;

static inline uint32_t bcd_decimal(uint32_t hex) {
    // NGH is only 3 digits, so only convert 3 digits
    assert(((hex & 0xf00) >> 8) < 10);
    assert(((hex & 0xf0) >> 4) < 10);
    assert((hex & 0x0f) < 10);
    return (((hex & 0xf00) >> 8) * 100) +
        (((hex & 0xf0) >> 4) * 10) +
        (hex & 0x0f);
}  

static inline uint32_t read32le(uint8_t *ptr, uint32_t addr) {
    return ptr[addr] | (ptr[addr + 1] << 8) |
        (ptr[addr + 2] << 16) | (ptr[addr + 3] << 24);
}

int geo_neo_load(void *data, size_t size) {
    uint8_t *romdata = (uint8_t*)data; // Assign internal ROM pointer
    if (size) { }

    // Bytes 0-3 should be 'N' 'E' 'O' 1
    if (romdata[0] != 'N' || romdata[1] != 'E' ||
        romdata[2] != 'O' || romdata[3] != 0x01) {
        geo_log(GEO_LOG_ERR, "Not a valid NEO ROM: %c %c %c %c\n",
            romdata[0], romdata[1], romdata[2], romdata[3]);
        return 0;
    }

    // Parse ROM info
    uint32_t size_prom = read32le(romdata, 4);
    uint32_t size_srom = read32le(romdata, 8);
    uint32_t size_mrom = read32le(romdata, 12);
    uint32_t size_v1rom = read32le(romdata, 16);
    uint32_t size_v2rom = read32le(romdata, 20);
    uint32_t size_crom = read32le(romdata, 24);

    geo_log(GEO_LOG_INF, "P  ROM Size: %d\n", size_prom);
    geo_log(GEO_LOG_INF, "S  ROM Size: %d\n", size_srom);
    geo_log(GEO_LOG_INF, "M1 ROM Size: %d\n", size_mrom);
    geo_log(GEO_LOG_INF, "V1 ROM Size: %d\n", size_v1rom);
    geo_log(GEO_LOG_INF, "V2 ROM Size: %d\n", size_v2rom);
    geo_log(GEO_LOG_INF, "C  ROM Size: %d\n", size_crom);

    // Parse Year, Genre, Screenshot, and NGH info
    uint32_t year = read32le(romdata, 28);
    uint32_t genre = read32le(romdata, 32);
    uint32_t screenshot = read32le(romdata, 36);
    uint32_t ngh = bcd_decimal(read32le(romdata, 40));

    // Parse Name
    char name[34];
    memcpy((char*)name, (uint8_t*)romdata + 44, 33);
    name[33] = '\0';

    // Parse Manufacturer
    char manu[18];
    memcpy((char*)manu, (uint8_t*)romdata + 77, 17);
    manu[17] = '\0';

    geo_log(GEO_LOG_INF, "Year: %d\n", year);
    geo_log(GEO_LOG_INF, "Genre: %d\n", genre);
    geo_log(GEO_LOG_INF, "Screenshot: %d\n", screenshot);
    geo_log(GEO_LOG_INF, "NGH: %03d\n", ngh);
    geo_log(GEO_LOG_INF, "Name: %s\n", name);
    geo_log(GEO_LOG_INF, "Manufacturer: %s\n", manu);

    // Assign pointers and sizes
    uint32_t rom_offset = 4096; // ROM data starts at byte 4096
    uint8_t *data_prom = &romdata[rom_offset];
    rom_offset += size_prom;

    uint8_t *data_srom = &romdata[rom_offset];
    rom_offset += size_srom;

    geo_m68k_romdata_game(data_prom, size_prom, data_srom, size_srom);

    uint8_t *data_mrom = &romdata[rom_offset];
    geo_z80_romdata(data_mrom, size_mrom);
    rom_offset += size_mrom;

    if (!size_v2rom) { // Single V ROM
        ymfm_shim_romdata(&romdata[rom_offset], size_v1rom,
            &romdata[rom_offset], size_v1rom);
        rom_offset += size_v1rom;
    }
    else { // V1 and V2 ROMs
        ymfm_shim_romdata(&romdata[rom_offset], size_v1rom,
            &romdata[rom_offset + size_v1rom], size_v2rom);
        rom_offset += size_v1rom + size_v2rom;
    }

    uint8_t *data_crom = &romdata[rom_offset];
    rom_offset += size_crom;

    // Pass S ROM and C ROM data to LSPC
    geo_lspc_romdata(data_srom, size_srom, data_crom, size_crom);

    // Temporary FIXME
    geo_m68k_postload();

    // Set default bankswitching
    geo_m68k_board_set(BOARD_DEFAULT);

    // Set default Fix Layer bank switching type
    geo_lspc_set_fix_banksw(FIX_BANKSW_NONE);

    // Handle special cases if necessary
    switch (ngh) {
        case 3: { // Top Player's Golf, Crouching Tiger Hidden Dragon bootlegs
            /* NGH 003 is assigned to Top Player's Golf, but the Crouching
               Tiger Hidden Dragon bootlegs also use it. Simple heuristics are
               used to determine which one it is. Top Player's Golf requires no
               special handling.
            */
            if (size_prom > 0x100000) // Crouching Tiger Hidden Dragon 2003
                geo_m68k_board_set(BOARD_CTHD2003);
            break;
        }
        case 6: case 19: case 38: {
            // Riding Hero, League Bowling, Thrash Rally
            geo_m68k_board_set(BOARD_LINKABLE);
            break;
        }
        case 8: { // Jockey Grand Prix
            geo_m68k_board_set(BOARD_JOCKEYGP);
            break;
        }
        case 47: case 52: { // Fatal Fury 2, Super Sidekicks
            geo_m68k_board_set(BOARD_CT0);
            break;
        }
        case 242: { // KOF 98
            geo_m68k_board_set(BOARD_KOF98);
            break;
        }
        case 250: { // Metal Slug X
            geo_m68k_board_set(BOARD_MSLUGX);
            break;
        }
        case 151: case 251: { // KOF 99
            if (size_prom > 0x500000) { // Encrypted
                geo_m68k_sma_init(sma_addr_kof99, sma_bank_kof99,
                    sma_scramble_kof99);
                geo_m68k_board_set(BOARD_SMA);
            }
            break;
        }
        case 253: { // Garou - Mark of the Wolves
            geo_lspc_set_fix_banksw(FIX_BANKSW_LINE);

            // Use heuristics to handle descrambling differences
            if (romdata[0xc1000 + 0x3e481] == 0x9f) { // NEO-SMA KE (AES)
                geo_m68k_sma_init(sma_addr_garouh, sma_bank_garouh,
                    sma_scramble_garouh);
                geo_m68k_board_set(BOARD_SMA);
            }
            else if (romdata[0xc1000 + 0x3e481] == 0x41) { // NEO-SMA KF (MVS)
                geo_m68k_sma_init(sma_addr_garou, sma_bank_garou,
                    sma_scramble_garou);
                geo_m68k_board_set(BOARD_SMA);
            }
            // Bootleg and Prototype don't have protection
            break;
        }
        case 256: { // Metal Slug 3
            geo_lspc_set_fix_banksw(FIX_BANKSW_LINE);

            if (size_prom > 0x500000) { // Encrypted
                geo_m68k_sma_init(sma_addr_mslug3, sma_bank_mslug3,
                    sma_scramble_mslug3);
                geo_m68k_board_set(BOARD_SMA);
            }
            // FIXME: Support mslug3a
            /*else if (mslug3a) {
                geo_m68k_sma_init(sma_addr_mslug3a, sma_bank_mslug3a,
                    sma_scramble_mslug3a);
                geo_m68k_board_set(BOARD_SMA);
            }*/
            break;
        }
        case 257: { // KOF 2000
            geo_lspc_set_fix_banksw(FIX_BANKSW_TILE);

            if (size_prom > 0x500000) { // Encrypted
                geo_m68k_sma_init(sma_addr_kof2000, sma_bank_kof2000,
                    sma_scramble_kof2000);
                geo_m68k_board_set(BOARD_SMA);
            }
            break;
        }
        case 263: { // Metal Slug 4
            // FIXME - ms4plus does not use bankswitching
            geo_lspc_set_fix_banksw(FIX_BANKSW_LINE);
            break;
        }
        case 266: { // Matrimelee
            geo_lspc_set_fix_banksw(FIX_BANKSW_TILE);
            break;
        }
        case 268: { // Metal Slug 5
            // Use heuristics to detect Metal Slug 5 Plus (bootleg)
            if (romdata[0x1000 + 0x26b] == 0xb9)
                geo_m68k_board_set(BOARD_MS5PLUS);
            else
                geo_m68k_board_set(BOARD_PVC);
            break;
        }
        case 269: { // SNK vs. Capcom - SVC Chaos
            /* The official release, SNK vs. Capcom (bootleg), and
               SNK vs. Capcom Super Plus use the NEO-PVC. The official release
               also uses Fix Bankswitching Type 2 (per-tile banking). The other
               known bootlegs use neither.
            */
            if (romdata[0x1000 + 0x3d25] == 0xc4) // Official Release Only
                geo_lspc_set_fix_banksw(FIX_BANKSW_TILE);
            if (romdata[0x1000 + 0x2f8f] == 0xc0)
                geo_m68k_board_set(BOARD_PVC);
            break;
        }
        case 271: { // KOF 2003
            if (romdata[0x1000 + 0xc1] == 0x38) { // Official Release
                geo_lspc_set_fix_banksw(FIX_BANKSW_TILE);
                geo_m68k_board_set(BOARD_PVC);
            }
            else if (romdata[0x1000 + 0x689] == 0x10) { // kf2k3bla/kf2k3pl
                geo_m68k_board_set(BOARD_KF2K3BLA);
            }
            else { // kf2k3bl and kf2k3upl
                geo_m68k_board_set(BOARD_KF2K3BL);
            }
            break;
        }
        default: {
            break;
        }
    }

    return 1;
}
