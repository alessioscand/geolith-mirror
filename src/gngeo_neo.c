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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emu.h"
#include "memory.h"
#include "roms.h"

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

static inline void swapb16_range(void *ptr, size_t len) {
    uint16_t *x = (uint16_t*)ptr;
    for (size_t i = 0; i < len >> 1; ++i)
        x[i] = (x[i] << 8) | (x[i] >> 8);
}

int gn_neo_load(void *data, size_t size) {
    uint8_t *romdata = (uint8_t*)data; // Assign internal ROM pointer
    if (size) { }

    // Bytes 0-3 should be 'N' 'E' 'O' 1
    if (romdata[0] != 'N' || romdata[1] != 'E' ||
        romdata[2] != 'O' || romdata[3] != 0x01) {
        gn_log(GN_LOG_ERR, "Not a valid NEO ROM: %c %c %c %c\n",
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

    gn_log(GN_LOG_INF, "P  ROM Size: %d\n", size_prom);
    gn_log(GN_LOG_INF, "S  ROM Size: %d\n", size_srom);
    gn_log(GN_LOG_INF, "M1 ROM Size: %d\n", size_mrom);
    gn_log(GN_LOG_INF, "V1 ROM Size: %d\n", size_v1rom);
    gn_log(GN_LOG_INF, "V2 ROM Size: %d\n", size_v2rom);
    gn_log(GN_LOG_INF, "C  ROM Size: %d\n", size_crom);

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

    gn_log(GN_LOG_INF, "Year: %d\n", year);
    gn_log(GN_LOG_INF, "Genre: %d\n", genre);
    gn_log(GN_LOG_INF, "Screenshot: %d\n", screenshot);
    gn_log(GN_LOG_INF, "NGH: %03d\n", ngh);
    gn_log(GN_LOG_INF, "Name: %s\n", name);
    gn_log(GN_LOG_INF, "Manufacturer: %s\n", manu);

    // Load BIOS and byteswap
    dr_load_bios(&(memory.rom));
    swapb16_range(memory.rom.bios_m68k.p, memory.rom.bios_m68k.size);

    // Assign pointers and sizes
    uint32_t rom_offset = 4096; // ROM data starts at byte 4096
    memory.rom.cpu_m68k.size = size_prom;
    memory.rom.cpu_m68k.p = &romdata[rom_offset];
    rom_offset += size_prom;
    swapb16_range(memory.rom.cpu_m68k.p, memory.rom.cpu_m68k.size);

    memcpy(memory.game_vector, memory.rom.cpu_m68k.p, 0x80);
    memcpy(memory.rom.cpu_m68k.p, memory.rom.bios_m68k.p, 0x80);

    memory.rom.game_sfix.size = size_srom;
    memory.rom.game_sfix.p = &romdata[rom_offset];
    rom_offset += size_srom;
    memory.fix_game_usage = (uint8_t*)malloc(memory.rom.game_sfix.size >> 5);
    convert_all_char(memory.rom.game_sfix.p, memory.rom.game_sfix.size,
        memory.fix_game_usage);

    memory.rom.cpu_z80.size = size_mrom;
    memory.rom.cpu_z80.p = &romdata[rom_offset];
    rom_offset += size_mrom;

    memory.rom.adpcma.size = size_v1rom;
    memory.rom.adpcma.p = &romdata[rom_offset];

    if (!size_v2rom) { // Single V ROM
        memory.rom.adpcmb.size = size_v1rom;
        memory.rom.adpcmb.p = &romdata[rom_offset];
        rom_offset += size_v1rom;
    }
    else { // V1 and V2 ROMs
        rom_offset += size_v1rom;
        memory.rom.adpcmb.size = size_v2rom;
        memory.rom.adpcmb.p = &romdata[rom_offset];
        rom_offset += size_v2rom;
    }

    memory.rom.tiles.size = size_crom;
    memory.rom.tiles.p = &romdata[rom_offset];
    memory.nb_of_tiles = memory.rom.tiles.size >> 7;
    rom_offset += size_crom;
    convert_all_tile(&(memory.rom));

    fix_usage = memory.fix_board_usage;
    current_pal = memory.vid.pal_neo[0];
    current_fix = memory.rom.bios_sfix.p;
    current_pc_pal = (uint32_t*)memory.vid.pal_host[0];
    memory.current_vector = 0;

    gn_set_decrypt(0); // .neo files are already decrypted

    // Handle descrambling if necessary
    switch (ngh) {
        case 253: // Garou - Mark of the Wolves
            init_garou(&(memory.rom));
            break;
        case 257: // KOF 2000
            init_kof2000(&(memory.rom));
            break;
        default:
            break;
    }

    return 1;
}

void gn_neo_unload(const char *name) {
    save_nvram((char*)name);
    save_memcard((char*)name);
}
