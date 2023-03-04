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

#define _POSIX_C_SOURCE 200809L

#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <miniz.h>

#include "roms.h"
#include "emu.h"
#include "memory.h"
#include "video.h"

static const char *savedir;

int neogeo_fix_bank_type = 0;

int bankoffset_kof99[64] = {
    0x000000, 0x100000, 0x200000, 0x300000, 0x3cc000,
    0x4cc000, 0x3f2000, 0x4f2000, 0x407800, 0x507800, 0x40d000, 0x50d000,
    0x417800, 0x517800, 0x420800, 0x520800, 0x424800, 0x524800, 0x429000,
    0x529000, 0x42e800, 0x52e800, 0x431800, 0x531800, 0x54d000, 0x551000,
    0x567000, 0x592800, 0x588800, 0x581800, 0x599800, 0x594800, 0x598000,
    /* rest not used? */
};
uint8_t scramblecode_kof99[6] = {14, 6, 8, 10, 12, 5,};

int bankoffset_garou[64] = {
    0x000000, 0x100000, 0x200000, 0x300000, // 00
    0x280000, 0x380000, 0x2d0000, 0x3d0000, // 04
    0x2f0000, 0x3f0000, 0x400000, 0x500000, // 08
    0x420000, 0x520000, 0x440000, 0x540000, // 12
    0x498000, 0x598000, 0x4a0000, 0x5a0000, // 16
    0x4a8000, 0x5a8000, 0x4b0000, 0x5b0000, // 20
    0x4b8000, 0x5b8000, 0x4c0000, 0x5c0000, // 24
    0x4c8000, 0x5c8000, 0x4d0000, 0x5d0000, // 28
    0x458000, 0x558000, 0x460000, 0x560000, // 32
    0x468000, 0x568000, 0x470000, 0x570000, // 36
    0x478000, 0x578000, 0x480000, 0x580000, // 40
    0x488000, 0x588000, 0x490000, 0x590000, // 44
    0x5d0000, 0x5d8000, 0x5e0000, 0x5e8000, // 48
    0x5f0000, 0x5f8000, 0x600000, /* rest not used? */
};
uint8_t scramblecode_garou[6] = {5, 9, 7, 6, 14, 12,};

int bankoffset_garouh[64] = {
    0x000000, 0x100000, 0x200000, 0x300000, // 00
    0x280000, 0x380000, 0x2d0000, 0x3d0000, // 04
    0x2c8000, 0x3c8000, 0x400000, 0x500000, // 08
    0x420000, 0x520000, 0x440000, 0x540000, // 12
    0x598000, 0x698000, 0x5a0000, 0x6a0000, // 16
    0x5a8000, 0x6a8000, 0x5b0000, 0x6b0000, // 20
    0x5b8000, 0x6b8000, 0x5c0000, 0x6c0000, // 24
    0x5c8000, 0x6c8000, 0x5d0000, 0x6d0000, // 28
    0x458000, 0x558000, 0x460000, 0x560000, // 32
    0x468000, 0x568000, 0x470000, 0x570000, // 36
    0x478000, 0x578000, 0x480000, 0x580000, // 40
    0x488000, 0x588000, 0x490000, 0x590000, // 44
    0x5d8000, 0x6d8000, 0x5e0000, 0x6e0000, // 48
    0x5e8000, 0x6e8000, 0x6e8000, 0x000000, // 52
    0x000000, 0x000000, 0x000000, 0x000000, // 56
    0x000000, 0x000000, 0x000000, 0x000000, // 60
};
uint8_t scramblecode_garouh[6] = {4, 8, 14, 2, 11, 13,};

int bankoffset_mslug3[64] = {
    0x000000, 0x020000, 0x040000, 0x060000, // 00
    0x070000, 0x090000, 0x0b0000, 0x0d0000, // 04
    0x0e0000, 0x0f0000, 0x120000, 0x130000, // 08
    0x140000, 0x150000, 0x180000, 0x190000, // 12
    0x1a0000, 0x1b0000, 0x1e0000, 0x1f0000, // 16
    0x200000, 0x210000, 0x240000, 0x250000, // 20
    0x260000, 0x270000, 0x2a0000, 0x2b0000, // 24
    0x2c0000, 0x2d0000, 0x300000, 0x310000, // 28
    0x320000, 0x330000, 0x360000, 0x370000, // 32
    0x380000, 0x390000, 0x3c0000, 0x3d0000, // 36
    0x400000, 0x410000, 0x440000, 0x450000, // 40
    0x460000, 0x470000, 0x4a0000, 0x4b0000, // 44
    0x4c0000, /* rest not used? */
};
uint8_t scramblecode_mslug3[6] = {14, 12, 15, 6, 3, 9,};

int bankoffset_kof2000[64] = {
    0x000000, 0x100000, 0x200000, 0x300000, // 00
    0x3f7800, 0x4f7800, 0x3ff800, 0x4ff800, // 04
    0x407800, 0x507800, 0x40f800, 0x50f800, // 08
    0x416800, 0x516800, 0x41d800, 0x51d800, // 12
    0x424000, 0x524000, 0x523800, 0x623800, // 16
    0x526000, 0x626000, 0x528000, 0x628000, // 20
    0x52a000, 0x62a000, 0x52b800, 0x62b800, // 24
    0x52d000, 0x62d000, 0x52e800, 0x62e800, // 28
    0x618000, 0x619000, 0x61a000, 0x61a800, // 32
};
uint8_t scramblecode_kof2000[6] = {15, 14, 7, 3, 10, 5,};

static inline void swapb16_range(void *ptr, size_t len) {
    uint16_t *x = (uint16_t*)ptr;
    for (size_t i = 0; i < len >> 1; ++i)
        x[i] = (x[i] << 8) | (x[i] >> 8);
}

void gn_set_savedir(const char *dir) {
    savedir = dir;
}

int init_kof99(void) {
    neogeo_fix_bank_type = 0;
    memory.bksw_offset = bankoffset_kof99;
    memory.bksw_unscramble = scramblecode_kof99;
    gn_m68k_prn_addrs(0x2ffff8, 0x2ffffa);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2ffff0);
    return 0;
}

int init_garou(void) {
    neogeo_fix_bank_type = 1;
    memory.bksw_offset = bankoffset_garou;
    memory.bksw_unscramble = scramblecode_garou;
    gn_m68k_prn_addrs(0x2fffcc, 0x2ffff0);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2fffc0);
    //garou_install_protection(machine);
    return 0;
}

int init_garouh(void) {
    neogeo_fix_bank_type = 1;
    memory.bksw_offset = bankoffset_garouh;
    memory.bksw_unscramble = scramblecode_garouh;
    gn_m68k_prn_addrs(0x2fffcc, 0x2ffff0);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2fffc0);
    //garouo_install_protection(machine);
    return 0;
}

int init_mslug3(void) {
    neogeo_fix_bank_type = 1;
    memory.bksw_offset = bankoffset_mslug3;
    memory.bksw_unscramble = scramblecode_mslug3;
    gn_m68k_prn_addrs(0x2ffff8, 0x2ffffa);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2fffe4);

    return 0;
}

int init_kof2000(void) {
    neogeo_fix_bank_type = 2;
    memory.bksw_offset = bankoffset_kof2000;
    memory.bksw_unscramble = scramblecode_kof2000;
    gn_m68k_prn_addrs(0x2fffd8, 0x2fffda);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2fffec);
    return 0;
}

static int allocate_region(ROM_REGION *r, uint32_t size, int region) {
    gn_log(GN_LOG_DBG, "Allocating 0x%08x byte for Region %d\n", size, region);
    if (size != 0) {
        r->p = malloc(size);

        if (r->p == 0) {
            r->size = 0;
            printf("Error allocating\n");
            /* TODO: Be more permissive, allow at least a dump */
            printf("Not enough memory :( exiting\n");
            exit(1);
            return 1;
        }
        memset(r->p, 0, size);
    } else
        r->p = NULL;
    r->size = size;
    return 0;
}

static int convert_roms_tile(uint8_t *g, int tileno) {
    unsigned char swap[128];
    unsigned int *gfxdata;
    int x, y;
    unsigned int pen, usage = 0;
    gfxdata = (uint32_t*) & g[tileno << 7];

    memcpy(swap, gfxdata, 128);

    //filed=1;
    for (y = 0; y < 16; y++) {
        unsigned int dw;

        dw = 0;
        for (x = 0; x < 8; x++) {
            pen = ((swap[64 + (y << 2) + 3] >> x) & 1) << 3;
            pen |= ((swap[64 + (y << 2) + 1] >> x) & 1) << 2;
            pen |= ((swap[64 + (y << 2) + 2] >> x) & 1) << 1;
            pen |= (swap[64 + (y << 2)] >> x) & 1;
            //if (!pen) filed=0;
            dw |= pen << ((7 - x) << 2);
            //memory.pen_usage[tileno]  |= (1 << pen);
            usage |= (1 << pen);
        }
        *(gfxdata++) = dw;

        dw = 0;
        for (x = 0; x < 8; x++) {
            pen = ((swap[(y << 2) + 3] >> x) & 1) << 3;
            pen |= ((swap[(y << 2) + 1] >> x) & 1) << 2;
            pen |= ((swap[(y << 2) + 2] >> x) & 1) << 1;
            pen |= (swap[(y << 2)] >> x) & 1;
            //if (!pen) filed=0;
            dw |= pen << ((7 - x) << 2);
            //memory.pen_usage[tileno]  |= (1 << pen);
            usage |= (1 << pen);
        }
        *(gfxdata++) = dw;
    }

    return 0;

}

void convert_all_tile(GAME_ROMS *r) {
    uint32_t i;
    allocate_region(&r->spr_usage, (r->tiles.size >> 11) * sizeof (uint32_t), REGION_SPR_USAGE);
    memset(r->spr_usage.p, 0, r->spr_usage.size);
    for (i = 0; i < r->tiles.size >> 7; i++) {
        ((uint32_t*) r->spr_usage.p)[i >> 4] |= convert_roms_tile(r->tiles.p, i);
    }
}

void convert_all_char(uint8_t *Ptr, int Taille,
        uint8_t *usage_ptr) {
    int i, j;
    unsigned char usage;

    uint8_t *Src;
    uint8_t *sav_src;

    Src = (uint8_t*) malloc(Taille);
    if (!Src) {
        printf("Not enought memory!!\n");
        return;
    }
    sav_src = Src;
    memcpy(Src, Ptr, Taille);
#ifdef WORDS_BIGENDIAN
#define CONVERT_TILE *Ptr++ = *(Src+8);\
                 usage |= *(Src+8);\
                     *Ptr++ = *(Src);\
             usage |= *(Src);\
             *Ptr++ = *(Src+24);\
             usage |= *(Src+24);\
             *Ptr++ = *(Src+16);\
             usage |= *(Src+16);\
             Src++;
#else
#define CONVERT_TILE *Ptr++ = *(Src+16);\
                 usage |= *(Src+16);\
                     *Ptr++ = *(Src+24);\
             usage |= *(Src+24);\
             *Ptr++ = *(Src);\
             usage |= *(Src);\
             *Ptr++ = *(Src+8);\
             usage |= *(Src+8);\
             Src++;
#endif
    for (i = Taille; i > 0; i -= 32) {
        usage = 0;
        for (j = 0; j < 8; j++) {
            CONVERT_TILE
        }
        Src += 24;
        *usage_ptr++ = usage;
    }
    free(sav_src);
#undef CONVERT_TILE
}

int gn_bios_load(const char *biospath) {
    GAME_ROMS *r = &(memory.rom);
    const char *romfile;

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    // Make sure it's actually a zip file
    if (!mz_zip_reader_init_file(&zip_archive, biospath, 0))
        return 0;

    if (conf.system == SYS_UNIBIOS) {
        romfile = "uni-bios_4_0.rom";
    }
    else if (conf.system == SYS_HOME) {
        romfile = "neo-epo.bin";
    }
    else {
        switch (conf.country) {
            default: case CTY_USA: // Winners don't use drugs
                romfile = "sp-u2.sp1";
                break;
            case CTY_JAPAN:
                romfile = "vs-bios.rom";
                break;
            case CTY_ASIA:
                romfile = "sp-s3.sp1";
                break;
            case CTY_EUROPE:
                romfile = "sp-s2.sp1";
                break;
        }
    }

    gn_log(GN_LOG_DBG, "Loading %s\n", romfile);
    r->bios_m68k.p = mz_zip_reader_extract_file_to_heap(&zip_archive,
        romfile, &(r->bios_m68k.size), 0);

    if (r->bios_m68k.p == NULL) {
        mz_zip_reader_end(&zip_archive);
        gn_log(GN_LOG_ERR, "Failed to load %s from BIOS archive!\n",
            romfile);
        return 0;
    }

    memory.ng_lo = mz_zip_reader_extract_file_to_heap(&zip_archive,
        "000-lo.lo", NULL, 0);
    if (memory.ng_lo == NULL) {
        mz_zip_reader_end(&zip_archive);
        gn_log(GN_LOG_ERR, "Failed to load 000-lo.lo from BIOS archive!\n");
        return 0;
    }

    if (conf.system != SYS_HOME) {
        r->bios_sfix.p = mz_zip_reader_extract_file_to_heap(&zip_archive,
            "sfix.sfix", &(r->bios_sfix.size), 0);
        if (r->bios_sfix.p == NULL) {
            mz_zip_reader_end(&zip_archive);
            gn_log(GN_LOG_ERR, "Failed to load sfix.sfix from BIOS archive!\n");
            return 0;
        }

        // convert bios fix char
        convert_all_char(memory.rom.bios_sfix.p, 0x20000,
            memory.fix_board_usage);
    }

    mz_zip_reader_end(&zip_archive);
    return 1;
}

void open_nvram(char *name) {
    char filename[256];
    FILE *f;

    sprintf(filename, "%s/%s.nv", savedir, name);

    if ((f = fopen(filename, "rb")) == 0)
        return;
    fread(memory.sram, 1, 0x10000, f);
    fclose(f);

}

/* TODO: multiple memcard */
void open_memcard(char *name) {
    char filename[256];
    FILE *f;

    sprintf(filename, "%s/%s.mcr", savedir, name);

    if ((f = fopen(filename, "rb")) == 0)
        return;
    fread(memory.memcard, 1, 0x800, f);
    fclose(f);
}

void save_nvram(char *name) {
    char filename[256];
    FILE *f;

    for (int i = 0xffff; i >= 0; i--) {
        if (memory.sram[i] != 0)
            break;
    }

    sprintf(filename, "%s/%s.nv", savedir, name);

    if ((f = fopen(filename, "wb")) != NULL) {
        fwrite(memory.sram, 1, 0x10000, f);
        fclose(f);
    }
}

void save_memcard(char *name) {
    char filename[256];
    FILE *f;

    sprintf(filename, "%s/%s.mcr", savedir, name);

    if ((f = fopen(filename, "wb")) != NULL) {
        fwrite(memory.memcard, 1, 0x800, f);
        fclose(f);
    }
}
