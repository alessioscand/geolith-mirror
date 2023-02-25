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

#include "roms.h"
#include "emu.h"
#include "memory.h"

#include "unzip.h"

#include "video.h"

#include "gngeo_romsets.h"

/* Prototype */
void kof98_decrypt_68k(GAME_ROMS *r);
void kof99_decrypt_68k(GAME_ROMS *r);
void garou_decrypt_68k(GAME_ROMS *r);
void garouo_decrypt_68k(GAME_ROMS *r);
void mslug3_decrypt_68k(GAME_ROMS *r);
void kof2000_decrypt_68k(GAME_ROMS *r);
void kof2002_decrypt_68k(GAME_ROMS *r);
void matrim_decrypt_68k(GAME_ROMS *r);
void samsho5_decrypt_68k(GAME_ROMS *r);
void samsh5p_decrypt_68k(GAME_ROMS *r);
void mslug5_decrypt_68k(GAME_ROMS *r);
void kf2k3pcb_decrypt_s1data(GAME_ROMS *r);
void kf2k3pcb_decrypt_68k(GAME_ROMS *r);
void kof2003_decrypt_68k(GAME_ROMS *r);
void kof99_neogeo_gfx_decrypt(GAME_ROMS *r, int extra_xor);
void kof2000_neogeo_gfx_decrypt(GAME_ROMS *r, int extra_xor);
void cmc50_neogeo_gfx_decrypt(GAME_ROMS *r, int extra_xor);
void cmc42_neogeo_gfx_decrypt(GAME_ROMS *r, int extra_xor);
void neogeo_bootleg_cx_decrypt(GAME_ROMS *r);
void neogeo_bootleg_sx_decrypt(GAME_ROMS *r, int extra_xor);
void svcpcb_gfx_decrypt(GAME_ROMS *r);
void svcpcb_s1data_decrypt(GAME_ROMS *r);
void neo_pcm2_swap(GAME_ROMS *r, int value);
void neo_pcm2_snk_1999(GAME_ROMS *r, int value);
void neogeo_cmc50_m1_decrypt(GAME_ROMS *r);

static int need_decrypt = 1;

static const char *gngeo_dir;
static const char *rpath;

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

#define LOAD_BUF_SIZE (128*1024)
static uint8_t* iloadbuf = NULL;

static inline void swapb16_range(void *ptr, size_t len) {
    uint16_t *x = (uint16_t*)ptr;
    for (size_t i = 0; i < len >> 1; ++i)
        x[i] = (x[i] << 8) | (x[i] >> 8);
}

void gn_set_savedir(const char *dir) {
    gngeo_dir = dir;
}

void gn_set_romdir(const char *dir) {
    rpath = dir;
}

void gn_set_decrypt(int d) {
    need_decrypt = d;
}

int init_kof99(GAME_ROMS *r) {
    if (need_decrypt) {
        kof99_decrypt_68k(r);
        kof99_neogeo_gfx_decrypt(r, 0x00);
    }
    neogeo_fix_bank_type = 0;
    memory.bksw_offset = bankoffset_kof99;
    memory.bksw_unscramble = scramblecode_kof99;
    gn_m68k_prn_addrs(0x2ffff8, 0x2ffffa);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2ffff0);
    //kof99_install_protection(machine);
    return 0;
}

int init_kof99n(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) kof99_neogeo_gfx_decrypt(r, 0x00);
    return 0;
}

int init_garou(GAME_ROMS *r) {
    if (need_decrypt) {
        garou_decrypt_68k(r);
        kof99_neogeo_gfx_decrypt(r, 0x06);
    }
    neogeo_fix_bank_type = 1;
    memory.bksw_offset = bankoffset_garou;
    memory.bksw_unscramble = scramblecode_garou;
    gn_m68k_prn_addrs(0x2fffcc, 0x2ffff0);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2fffc0);
    //garou_install_protection(machine);
    return 0;
}

int init_garouh(GAME_ROMS *r) {
    if (need_decrypt) {
        garouo_decrypt_68k(r);
        kof99_neogeo_gfx_decrypt(r, 0x06);
    }
    neogeo_fix_bank_type = 1;
    memory.bksw_offset = bankoffset_garouh;
    memory.bksw_unscramble = scramblecode_garouh;
    gn_m68k_prn_addrs(0x2fffcc, 0x2ffff0);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2fffc0);
    //garouo_install_protection(machine);
    return 0;
}

int init_garoubl(GAME_ROMS *r) {
    /* TODO: Bootleg support */
    if (need_decrypt) {
        neogeo_bootleg_sx_decrypt(r, 2);
        neogeo_bootleg_cx_decrypt(r);
    }
    return 0;
}

int init_mslug3(GAME_ROMS *r) {
    if (need_decrypt) {
        mslug3_decrypt_68k(r);
        kof99_neogeo_gfx_decrypt(r, 0xad);
    }
    neogeo_fix_bank_type = 1;
    memory.bksw_offset = bankoffset_mslug3;
    memory.bksw_unscramble = scramblecode_mslug3;
    gn_m68k_prn_addrs(0x2ffff8, 0x2ffffa);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2fffe4);

    return 0;
}

int init_mslug3b6(GAME_ROMS *r) {
    /* TODO: Bootleg support */
    if (need_decrypt) {
        neogeo_bootleg_sx_decrypt(r, 2);
        cmc42_neogeo_gfx_decrypt(r, 0xad);
    }
    return 0;
}

int init_kof2000(GAME_ROMS *r) {
    if (need_decrypt) {
        kof2000_decrypt_68k(r);
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0x00);
    }
    neogeo_fix_bank_type = 2;
    memory.bksw_offset = bankoffset_kof2000;
    memory.bksw_unscramble = scramblecode_kof2000;
    gn_m68k_prn_addrs(0x2fffd8, 0x2fffda);
    gn_m68k_banksw_set(BANKSW_SMA, 0x2fffec);
    return 0;

}

int init_kof2000n(GAME_ROMS *r) {
    neogeo_fix_bank_type = 2;
    if (need_decrypt) {
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0x00);
    }
    return 0;
}

int init_kof2001(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) {
        kof2000_neogeo_gfx_decrypt(r, 0x1e);
        neogeo_cmc50_m1_decrypt(r);
    }
    return 0;

}

int init_mslug4(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1; /* USA violent content screen is wrong --
                             * not a bug, confirmed on real hardware! */
    if (need_decrypt) {
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0x31);

        neo_pcm2_snk_1999(r, 8);
    }
    return 0;

}

int init_ms4plus(GAME_ROMS *r) {
    if (need_decrypt) {
        cmc50_neogeo_gfx_decrypt(r, 0x31);
        neo_pcm2_snk_1999(r, 8);
        neogeo_cmc50_m1_decrypt(r);
    }
    return 0;
}

int init_ganryu(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) kof99_neogeo_gfx_decrypt(r, 0x07);
    return 0;
}

int init_s1945p(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) kof99_neogeo_gfx_decrypt(r, 0x05);
    return 0;
}

int init_preisle2(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) kof99_neogeo_gfx_decrypt(r, 0x9f);
    return 0;
}

int init_bangbead(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) kof99_neogeo_gfx_decrypt(r, 0xf8);
    return 0;
}

int init_nitd(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) kof99_neogeo_gfx_decrypt(r, 0xff);
    return 0;
}

int init_zupapa(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) kof99_neogeo_gfx_decrypt(r, 0xbd);
    return 0;
}

int init_sengoku3(GAME_ROMS *r) {
    neogeo_fix_bank_type = 1;
    if (need_decrypt) kof99_neogeo_gfx_decrypt(r, 0xfe);
    return 0;
}

int init_kof98(GAME_ROMS *r) {
    if (need_decrypt) kof98_decrypt_68k(r);

    //install_kof98_protection(r);
    return 0;
}

int init_rotd(GAME_ROMS *r) {
    if (need_decrypt) {
        neo_pcm2_snk_1999(r, 16);
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0x3f);
    }
    neogeo_fix_bank_type = 1;
    return 0;
}

int init_kof2002(GAME_ROMS *r) {
    if (need_decrypt) {
        kof2002_decrypt_68k(r);
        neo_pcm2_swap(r, 0);
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0xec);
    }
    return 0;
}

int init_kof2002b(GAME_ROMS *r) {
    /* TODO: Bootleg */
    if (need_decrypt) {
        kof2002_decrypt_68k(r);
        neo_pcm2_swap(r, 0);
        neogeo_cmc50_m1_decrypt(r);
        //kof2002b_gfx_decrypt(r, r->tiles.p,0x4000000);
        //kof2002b_gfx_decrypt(r, r->game_sfix.p,0x20000);
    }
    return 0;
}

int init_kf2k2pls(GAME_ROMS *r) {
    if (need_decrypt) {
        kof2002_decrypt_68k(r);
        neo_pcm2_swap(r, 0);
        neogeo_cmc50_m1_decrypt(r);
        cmc50_neogeo_gfx_decrypt(r, 0xec);
    }
    return 0;
}

int init_kf2k2mp(GAME_ROMS *r) {
    /* TODO: Bootleg */
    if (need_decrypt) {
        //kf2k2mp_decrypt(r);
        neo_pcm2_swap(r, 0);
        //neogeo_bootleg_sx_decrypt(r, 2);
        cmc50_neogeo_gfx_decrypt(r, 0xec);
    }
    return 0;
}

int init_kof2km2(GAME_ROMS *r) {
    /* TODO: Bootleg */
    if (need_decrypt) {
        //kof2km2_px_decrypt(r);
        neo_pcm2_swap(r, 0);
        //neogeo_bootleg_sx_decrypt(r, 1);
        cmc50_neogeo_gfx_decrypt(r, 0xec);
    }
    return 0;
}

/*

 TODO
 static DRIVER_INIT( kof10th )
 {
 decrypt_kof10th(machine);
 DRIVER_INIT_CALL(neogeo);
 install_kof10th_protection(machine);
 }

 static DRIVER_INIT( kf10thep )
 {
 decrypt_kf10thep(machine);
 DRIVER_INIT_CALL(neogeo);
 }

 static DRIVER_INIT( kf2k5uni )
 {
 decrypt_kf2k5uni(machine);
 DRIVER_INIT_CALL(neogeo);
 }

 static DRIVER_INIT( kof2k4se )
 {
 decrypt_kof2k4se_68k(machine);
 DRIVER_INIT_CALL(neogeo);
 }

 static DRIVER_INIT( matrimbl )
 {
 matrim_decrypt_68k(machine);
 neogeo_fixed_layer_bank_type = 2;
 matrimbl_decrypt(machine);
 neogeo_sfix_decrypt(machine);
 DRIVER_INIT_CALL(neogeo);
 }

 */

int init_matrim(GAME_ROMS *r) {
    if (need_decrypt) {
        matrim_decrypt_68k(r);
        neo_pcm2_swap(r, 1);
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0x6a);
    }
    neogeo_fix_bank_type = 2;
    return 0;
}

int init_pnyaa(GAME_ROMS *r) {
    if (need_decrypt) {
        neo_pcm2_snk_1999(r, 4);
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0x2e);
    }
    neogeo_fix_bank_type = 1;
    return 0;
}

int init_mslug5(GAME_ROMS *r) {
    if (need_decrypt) {
        mslug5_decrypt_68k(r);
        neo_pcm2_swap(r, 2);
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0x19);
    }
    neogeo_fix_bank_type = 1;
    //install_pvc_protection(r);
    return 0;
}

/*
 TODO:
 static TIMER_CALLBACK( ms5pcb_bios_timer_callback )
 {
 int harddip3 = input_port_read(machine, "HARDDIP") & 1;
 memory_set_bankptr(machine, NEOGEO_BANK_BIOS, memory_region(machine, "mainbios")
 +0x20000+harddip3*0x20000);
 }

 */
int init_ms5pcb(GAME_ROMS *r) {

    /* TODO: start a timer that will check the BIOS select DIP every second */
    //timer_set(machine, attotime_zero, NULL, 0, ms5pcb_bios_timer_callback);
    //timer_pulse(machine, ATTOTIME_IN_MSEC(1000), NULL, 0, ms5pcb_bios_timer_callback);
    if (need_decrypt) {
        mslug5_decrypt_68k(r);
        svcpcb_gfx_decrypt(r);
        neogeo_cmc50_m1_decrypt(r);
        kof2000_neogeo_gfx_decrypt(r, 0x19);
        svcpcb_s1data_decrypt(r);
        neo_pcm2_swap(r, 2);
    }
    neogeo_fix_bank_type = 2;
    //install_pvc_protection(r);
    return 0;
}

int init_ms5plus(GAME_ROMS *r) {
    /* TODO: Bootleg */
    if (need_decrypt) {
        cmc50_neogeo_gfx_decrypt(r, 0x19);
        neo_pcm2_swap(r, 2);
        //neogeo_bootleg_sx_decrypt(r, 1);
    }
    neogeo_fix_bank_type = 1;

    //install_ms5plus_protection(r);
    return 0;
}
#if 0
// TODO:

static TIMER_CALLBACK(svcpcb_bios_timer_callback) {
    int harddip3 = input_port_read(machine, "HARDDIP") & 1;
    memory_set_bankptr(machine, NEOGEO_BANK_BIOS, memory_region(machine, "mainbios")
            + 0x20000 + harddip3 * 0x20000);
}

static DRIVER_INIT(svcpcb) {
    /* start a timer that will check the BIOS select DIP every second */
    timer_set(machine, attotime_zero, NULL, 0, svcpcb_bios_timer_callback);
    timer_pulse(machine, ATTOTIME_IN_MSEC(1000), NULL, 0, svcpcb_bios_timer_callback);

    svc_px_decrypt(machine);
    svcpcb_gfx_decrypt(machine);
    neogeo_cmc50_m1_decrypt(machine);
    kof2000_neogeo_gfx_decrypt(machine, 0x57);
    svcpcb_s1data_decrypt(machine);
    neo_pcm2_swap(machine, 3);
    neogeo_fixed_layer_bank_type = 2;
    DRIVER_INIT_CALL(neogeo);
    install_pvc_protection(machine);
}

static DRIVER_INIT(svc) {
    svc_px_decrypt(machine);
    neo_pcm2_swap(machine, 3);
    neogeo_fixed_layer_bank_type = 2;
    neogeo_cmc50_m1_decrypt(machine);
    kof2000_neogeo_gfx_decrypt(machine, 0x57);
    DRIVER_INIT_CALL(neogeo);
    install_pvc_protection(machine);
}

static DRIVER_INIT(svcboot) {
    svcboot_px_decrypt(machine);
    svcboot_cx_decrypt(machine);
    DRIVER_INIT_CALL(neogeo);
    install_pvc_protection(machine);
}

static DRIVER_INIT(svcplus) {
    svcplus_px_decrypt(machine);
    svcboot_cx_decrypt(machine);
    neogeo_bootleg_sx_decrypt(machine, 1);
    svcplus_px_hack(machine);
    DRIVER_INIT_CALL(neogeo);
}

static DRIVER_INIT(svcplusa) {
    svcplusa_px_decrypt(machine);
    svcboot_cx_decrypt(machine);
    svcplus_px_hack(machine);
    DRIVER_INIT_CALL(neogeo);
}

static DRIVER_INIT(svcsplus) {
    svcsplus_px_decrypt(machine);
    neogeo_bootleg_sx_decrypt(machine, 2);
    svcboot_cx_decrypt(machine);
    svcsplus_px_hack(machine);
    DRIVER_INIT_CALL(neogeo);
    install_pvc_protection(machine);
}

static DRIVER_INIT(samsho5) {
    samsho5_decrypt_68k(machine);
    neo_pcm2_swap(machine, 4);
    neogeo_fixed_layer_bank_type = 1;
    neogeo_cmc50_m1_decrypt(machine);
    kof2000_neogeo_gfx_decrypt(machine, 0x0f);
    DRIVER_INIT_CALL(neogeo);
}

static DRIVER_INIT(samsho5b) {
    samsho5b_px_decrypt(machine);
    samsho5b_vx_decrypt(machine);
    neogeo_bootleg_sx_decrypt(machine, 1);
    neogeo_bootleg_cx_decrypt(machine);
    DRIVER_INIT_CALL(neogeo);
}

static DRIVER_INIT(kf2k3pcb) {
    kf2k3pcb_decrypt_68k(machine);
    kf2k3pcb_gfx_decrypt(machine);
    kof2003biosdecode(machine);
    neogeo_cmc50_m1_decrypt(machine);

    /* extra little swap on the m1 - this must be performed AFTER the m1 decrypt
     or the m1 checksum (used to generate the key) for decrypting the m1 is
     incorrect */
    {
        int i;
        uint8_t* rom = memory_region(machine, "audiocpu");
        for (i = 0; i < 0x90000; i++) {
            rom[i] = BITSWAP8(rom[i], 5, 6, 1, 4, 3, 0, 7, 2);
        }

    }

    kof2000_neogeo_gfx_decrypt(machine, 0x9d);
    kf2k3pcb_decrypt_s1data(machine);
    neo_pcm2_swap(machine, 5);
    neogeo_fixed_layer_bank_type = 2;
    DRIVER_INIT_CALL(neogeo);
    install_pvc_protection(machine);
    memory_install_read16_handler(cputag_get_address_space(machine, "maincpu",
            ADDRESS_SPACE_PROGRAM), 0xc00000, 0xc7ffff, 0, 0,
            (read16_space_func) SMH_BANK(6)); // 512k bios
}

static DRIVER_INIT(kof2003) {
    kof2003_decrypt_68k(machine);
    neo_pcm2_swap(machine, 5);
    neogeo_fixed_layer_bank_type = 2;
    neogeo_cmc50_m1_decrypt(machine);
    kof2000_neogeo_gfx_decrypt(machine, 0x9d);
    DRIVER_INIT_CALL(neogeo);
    install_pvc_protection(machine);
}

static DRIVER_INIT(kof2003h) {
    kof2003h_decrypt_68k(machine);
    neo_pcm2_swap(machine, 5);
    neogeo_fixed_layer_bank_type = 2;
    neogeo_cmc50_m1_decrypt(machine);
    kof2000_neogeo_gfx_decrypt(machine, 0x9d);
    DRIVER_INIT_CALL(neogeo);
    install_pvc_protection(machine);
}

static DRIVER_INIT(kf2k3bl) {
    cmc50_neogeo_gfx_decrypt(machine, 0x9d);
    neo_pcm2_swap(machine, 5);
    neogeo_bootleg_sx_decrypt(machine, 1);
    DRIVER_INIT_CALL(neogeo);
    kf2k3bl_install_protection(machine);
}

static DRIVER_INIT(kf2k3pl) {
    cmc50_neogeo_gfx_decrypt(machine, 0x9d);
    neo_pcm2_swap(machine, 5);
    kf2k3pl_px_decrypt(machine);
    neogeo_bootleg_sx_decrypt(machine, 1);
    DRIVER_INIT_CALL(neogeo);
    kf2k3pl_install_protection(machine);
}

static DRIVER_INIT(kf2k3upl) {
    cmc50_neogeo_gfx_decrypt(machine, 0x9d);
    neo_pcm2_swap(machine, 5);
    kf2k3upl_px_decrypt(machine);
    neogeo_bootleg_sx_decrypt(machine, 2);
    DRIVER_INIT_CALL(neogeo);
    kf2k3upl_install_protection(machine);
}

static DRIVER_INIT(samsh5sp) {
    samsh5sp_decrypt_68k(machine);
    neo_pcm2_swap(machine, 6);
    neogeo_fixed_layer_bank_type = 1;
    neogeo_cmc50_m1_decrypt(machine);
    kof2000_neogeo_gfx_decrypt(machine, 0x0d);
    DRIVER_INIT_CALL(neogeo);
}

static DRIVER_INIT(jockeygp) {
    uint16_t* extra_ram;

    neogeo_fixed_layer_bank_type = 1;
    neogeo_cmc50_m1_decrypt(machine);
    kof2000_neogeo_gfx_decrypt(machine, 0xac);

    /* install some extra RAM */
    extra_ram = auto_alloc_array(machine, uint16_t, 0x2000 / 2);
    state_save_register_global_pointer(machine, extra_ram, 0x2000 / 2);

    memory_install_readwrite16_handler(cputag_get_address_space(machine, "maincpu",
            ADDRESS_SPACE_PROGRAM), 0x200000, 0x201fff, 0, 0,
            (read16_space_func) SMH_BANK(8), (write16_space_func) SMH_BANK(8));
    memory_set_bankptr(machine, NEOGEO_BANK_EXTRA_RAM, extra_ram);

    //  memory_install_read_port_handler(cputag_get_address_space(machine,
    //"maincpu", ADDRESS_SPACE_PROGRAM), 0x280000, 0x280001, 0, 0, "IN5");
    //  memory_install_read_port_handler(cputag_get_address_space(machine,
    //"maincpu", ADDRESS_SPACE_PROGRAM), 0x2c0000, 0x2c0001, 0, 0, "IN6");

    DRIVER_INIT_CALL(neogeo);
}

static DRIVER_INIT(vliner) {
    uint16_t* extra_ram;

    /* install some extra RAM */
    extra_ram = auto_alloc_array(machine, uint16_t, 0x2000 / 2);
    state_save_register_global_pointer(machine, extra_ram, 0x2000 / 2);

    memory_install_readwrite16_handler(cputag_get_address_space(machine, "maincpu",
            ADDRESS_SPACE_PROGRAM), 0x200000, 0x201fff, 0, 0, (read16_space_func)
            SMH_BANK(8), (write16_space_func) SMH_BANK(8));
    memory_set_bankptr(machine, NEOGEO_BANK_EXTRA_RAM, extra_ram);

    memory_install_read_port_handler(cputag_get_address_space(machine, "maincpu",
            ADDRESS_SPACE_PROGRAM), 0x280000, 0x280001, 0, 0, "IN5");
    memory_install_read_port_handler(cputag_get_address_space(machine, "maincpu",
            ADDRESS_SPACE_PROGRAM), 0x2c0000, 0x2c0001, 0, 0, "IN6");

    DRIVER_INIT_CALL(neogeo);
}

static DRIVER_INIT(kog) {
    /* overlay cartridge ROM */
    memory_install_read_port_handler(cputag_get_address_space(machine, "maincpu",
            ADDRESS_SPACE_PROGRAM), 0x0ffffe, 0x0fffff, 0, 0, "JUMPER");

    kog_px_decrypt(machine);
    neogeo_bootleg_sx_decrypt(machine, 1);
    neogeo_bootleg_cx_decrypt(machine);
    DRIVER_INIT_CALL(neogeo);
}

static DRIVER_INIT(lans2004) {
    lans2004_decrypt_68k(machine);
    lans2004_vx_decrypt(machine);
    neogeo_bootleg_sx_decrypt(machine, 1);
    neogeo_bootleg_cx_decrypt(machine);
    DRIVER_INIT_CALL(neogeo);
}

#endif

struct roms_init_func {
    char *name;
    int (*init)(GAME_ROMS * r);
} init_func_table[] = {
    { "kof99", init_kof99},
    { "kof99n", init_kof99n},
    { "garou", init_garou},
    { "garouh", init_garouh},
    { "garoubl", init_garoubl},
    { "mslug3", init_mslug3},
    { "mslug3b6", init_mslug3b6},
    { "kof2000", init_kof2000},
    { "kof2000n", init_kof2000n},
    { "kof2001", init_kof2001},
    { "mslug4", init_mslug4},
    { "ms4plus", init_ms4plus},
    { "ganryu", init_ganryu},
    { "s1945p", init_s1945p},
    { "preisle2", init_preisle2},
    { "bangbead", init_bangbead},
    { "nitd", init_nitd},
    { "zupapa", init_zupapa},
    { "sengoku3", init_sengoku3},
    { "kof98", init_kof98},
    { "rotd", init_rotd},
    { "kof2002", init_kof2002},
    { "kof2002b", init_kof2002b},
    { "kf2k2pls", init_kf2k2pls},
    { "kf2k2mp", init_kf2k2mp},
    { "kof2km2", init_kof2km2},
    { "matrim", init_matrim},
    { "pnyaa", init_pnyaa},
    { "mslug5", init_mslug5},
    { "ms5pcb", init_ms5pcb},
    { "ms5plus", init_ms5plus},
    { NULL, NULL}
};

/*
 * Load a rom definition
 * return ROM_DEF*, NULL on error
 */
static ROM_DEF *res_load_drv(char *name) {
    for (size_t i = 0; i < sizeof(romsets) / sizeof(ROM_DEF); ++i)
        if (!strcmp(name, romsets[i].name))
            return &romsets[i];
    return NULL;
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

static void free_region(ROM_REGION *r) {
    gn_log(GN_LOG_DBG, "Free Region %p %p %d\n", r, r->p, r->size);
    if (r->p)
        free(r->p);
    r->size = 0;
    r->p = NULL;
}

static int zip_seek_current_file(ZFILE *gz, uint32_t offset) {
    uint8_t *buf;
    uint32_t s = 4096, c;
    buf = malloc(s);
    if (!buf)
        return -1;
    while (offset) {
        c = offset;
        if (c > s)
            c = s;

        c = gn_unzip_fread(gz, buf, c);
        if (c == 0) {
            break;
        }
        offset -= c;
    }
    free(buf);
    return 0;

}

static int read_counter;

static int read_data_i(ZFILE *gz, ROM_REGION *r, uint32_t dest, uint32_t size) {
    //uint8_t *buf;
    uint8_t *p = r->p + dest;
    uint32_t s = LOAD_BUF_SIZE, c, i;
    if (r->p == NULL || r->size < (dest & ~0x1) + (size * 2)) {
        printf("Region not allocated or not big enough %08x %08x\n", r->size,
                dest + (size * 2));
        return -1;
    }
    //buf=malloc(s);
    if (!iloadbuf)
        return -1;

    while (size) {
        c = size;
        if (c > s)
            c = s;

        c = gn_unzip_fread(gz, iloadbuf, c);
        if (c == 0) {
            //free(buf);
            return 0;
        }
        for (i = 0; i < c; i++) {
            //printf("%d %d\n",i,c);
            *p = iloadbuf[i];
            p += 2;
        }
        size -= c;
        read_counter += c;
    }
    //free(buf);
    return 0;
}

static int read_data_p(ZFILE *gz, ROM_REGION *r, uint32_t dest, uint32_t size) {
    uint32_t s = LOAD_BUF_SIZE, c, i = 0;
    if (r->p == NULL || r->size < dest + size) {
        printf("Region not allocated or not big enough\n");
        return -1;
    }
    while (size) {
        c = size;
        if (c > s)
            c = s;
        c = gn_unzip_fread(gz, r->p + dest + i, c);
        if (c == 0) {
            //free(buf);
            return 0;
        }
        i += c;
        size -= c;
        read_counter += c;
    }

    return 0;
}

static int load_region(PKZIP *pz, GAME_ROMS *r, int region, uint32_t src,
        uint32_t dest, uint32_t size, uint32_t crc, char *filename) {
    int rc;
    int badcrc = 0;
    ZFILE *gz;

    gz = gn_unzip_fopen(pz, filename, crc);
    if (gz == NULL) {
        gn_log(GN_LOG_DBG, "KO\n");
        gn_log(GN_LOG_DBG, "Load file %-17s in region %d: KO\n", filename, region);
        return 1;
    }

    if (src != 0) { /* TODO: Reuse an allready opened zfile */

        if (region == REGION_SPRITES)
            rc = zip_seek_current_file(gz, src / 2);
        else
            rc = zip_seek_current_file(gz, src);
        gn_log(GN_LOG_DBG, "setoffset: %d %08x %08x %08x\n", rc, src, dest, size);
    }

    gn_log(GN_LOG_DBG, "Trying to load file %-17s in region %d\n", filename, region);

    switch (region) {
        case REGION_SPRITES: /* Special interleaved loading  */
            read_data_i(gz, &r->tiles, dest, size);
            break;
        case REGION_AUDIO_CPU_CARTRIDGE:
            read_data_p(gz, &r->cpu_z80, dest, size);
            break;
        case REGION_AUDIO_CPU_ENCRYPTED:
            read_data_p(gz, &r->cpu_z80c, dest, size);
            break;
        case REGION_MAIN_CPU_CARTRIDGE:
            read_data_p(gz, &r->cpu_m68k, dest, size);
            break;
        case REGION_FIXED_LAYER_CARTRIDGE:
            read_data_p(gz, &r->game_sfix, dest, size);
            break;
        case REGION_AUDIO_DATA_1:
            read_data_p(gz, &r->adpcma, dest, size);
            break;
        case REGION_AUDIO_DATA_2:
            read_data_p(gz, &r->adpcmb, dest, size);
            break;
        case REGION_MAIN_CPU_BIOS:
            read_data_p(gz, &r->bios_m68k, dest, size);
            break;
        case REGION_AUDIO_CPU_BIOS:
            read_data_p(gz, &r->bios_m68k, dest, size);
            break;
        case REGION_FIXED_LAYER_BIOS:
            read_data_p(gz, &r->bios_sfix, dest, size);
            break;

        default:
            gn_log(GN_LOG_DBG, "Unhandled region %d\n", region);
            break;

    }
    gn_log(GN_LOG_DBG, "Load file %-17s in region %d: OK %s\n", filename, region,
            (badcrc ? "(Bad CRC)" : ""));
    //unzCloseCurrentFile(gz);
    gn_unzip_fclose(gz);
    return 0;
}

static PKZIP *open_rom_zip(const char *rom_path, char *name) {
    char *buf;
    int size = strlen(rom_path) + strlen(name) + 6;
    PKZIP *gz;
    buf = malloc(size);
    snprintf(buf, size, "%s/%s.zip", rom_path, name);
    gz = gn_open_zip(buf);
    free(buf);
    return gz;
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

static int init_roms(GAME_ROMS *r) {
    int i = 0;
    //printf("INIT ROM %s\n",r->info.name);
    neogeo_fix_bank_type = 0;
    memory.bksw_handler = 0;
    memory.bksw_unscramble = NULL;
    memory.bksw_offset = NULL;

    while (init_func_table[i].name) {
        //printf("INIT ROM ? %s %s\n",init_func_table[i].name,r->info.name);
        if (strcmp(init_func_table[i].name, r->info.name) == 0
                && init_func_table[i].init != NULL) {
            gn_log(GN_LOG_DBG, "Special init func\n");
            return init_func_table[i].init(r);
        }
        i++;
    }
    gn_log(GN_LOG_DBG, "Default roms init\n");
    return 0;
}

int dr_load_bios(GAME_ROMS *r) {
    PKZIP *pz;
    unsigned int size;
    char fpath[256];
    const char *romfile;

    snprintf(fpath, sizeof(fpath), "%s/neogeo.zip", rpath);

    pz = gn_open_zip(fpath);
    if (pz == NULL) {
        gn_log(GN_LOG_ERR, "Failed to open BIOS archive: %s\n", fpath);
        return 0;
    }

    memory.ng_lo = gn_unzip_file_malloc(pz, "000-lo.lo", 0x0, &size);
    if (memory.ng_lo == NULL) {
        gn_log(GN_LOG_ERR, "Failed to load 000-lo.lo from BIOS archive!\n");
        return 0;
    }

    if (!(r->info.flags & HAS_CUSTOM_SFIX_BIOS)) {
        r->bios_sfix.p = gn_unzip_file_malloc(pz, "sfix.sfix", 0x0, &r->bios_sfix.size);
        if (r->bios_sfix.p == NULL) {
            gn_log(GN_LOG_ERR, "Failed to load sfix.sfix from BIOS archive!\n");
            return 0;
        }
    }

    /* convert bios fix char */
    convert_all_char(memory.rom.bios_sfix.p, 0x20000, memory.fix_board_usage);

    if (!(r->info.flags & HAS_CUSTOM_CPU_BIOS)) {
        if (conf.system == SYS_UNIBIOS) {
            r->bios_m68k.p = gn_unzip_file_malloc(pz, "uni-bios_4_0.rom", 0x0, &r->bios_m68k.size);
            if (r->bios_m68k.p == NULL) {
                gn_log(GN_LOG_ERR, "Failed to load %s from BIOS archive!\n", "uni-bios_4_0.rom");
                gn_close_zip(pz);
                return 0;
            }
        }
        else {
            /*if (conf.system == SYS_HOME) { // Currently broken
                romfile = "neo-epo.bin";
            }*/
            //else {
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
            //}

            gn_log(GN_LOG_DBG, "Loading %s\n", romfile);
            r->bios_m68k.p = gn_unzip_file_malloc(pz, romfile, 0x0, &r->bios_m68k.size);

            if (r->bios_m68k.p == NULL) {
                gn_log(GN_LOG_ERR, "Failed to load %s from BIOS archive!\n", romfile);
                gn_close_zip(pz);
                return 0;
            }
        }
    }

    gn_close_zip(pz);
    return 1;
}

ROM_DEF *dr_check_zip(char *filename) {

    char *z;
    ROM_DEF *drv;
    char *game = strdup(basename(filename));

    if (game == NULL)
        return NULL;

    z = strstr(game, ".zip");

    if (z == NULL)
    {
        free(game);
        return NULL;
    }
    z[0] = 0;
    drv = res_load_drv(game);
    free(game);
    return drv;
}

int dr_load_roms(GAME_ROMS *r, const char *rom_path, char *name) {
    //unzFile *gz,*gzp=NULL,*rdefz;
    PKZIP *gz, *gzp = NULL;
    ROM_DEF *drv;
    int romsize;

    memset(r, 0, sizeof (GAME_ROMS));

    drv = res_load_drv(name);
    if (!drv) {
        gn_log(GN_LOG_ERR, "Failed to find ROM driver for: %s\n", name);

        return 0;
    }

    gz = open_rom_zip(rom_path, name);
    if (gz == NULL) {
        gn_log(GN_LOG_ERR, "Failed to find ROM: %s/%s.zip\n", rom_path, name);
        return 0;
    }

    /* Open Parent.
     For now, only one parent is supported, no recursion
     */
    gzp = open_rom_zip(rom_path, drv->parent);
    if (gzp == NULL) {
        gn_log(GN_LOG_ERR, "Failed to find parent: %s/%s.zip\n", rom_path, name);
        return 0;
    }

    //printf("year %d\n",drv->year);
    //return;

    r->info.name = strdup(drv->name);
    r->info.longname = strdup(drv->longname);
    r->info.year = drv->year;
    r->info.flags = 0;
    allocate_region(&r->cpu_m68k, drv->romsize[REGION_MAIN_CPU_CARTRIDGE],
            REGION_MAIN_CPU_CARTRIDGE);
    if (drv->romsize[REGION_AUDIO_CPU_CARTRIDGE] == 0
            && drv->romsize[REGION_AUDIO_CPU_ENCRYPTED] != 0) {
        //allocate_region(&r->cpu_z80,drv->romsize[REGION_AUDIO_CPU_ENCRYPTED]);
        //allocate_region(&r->cpu_z80c,drv->romsize[REGION_AUDIO_CPU_ENCRYPTED]);
        allocate_region(&r->cpu_z80c, 0x80000, REGION_AUDIO_CPU_ENCRYPTED);
        allocate_region(&r->cpu_z80, 0x90000, REGION_AUDIO_CPU_CARTRIDGE);
    } else {
        allocate_region(&r->cpu_z80, drv->romsize[REGION_AUDIO_CPU_CARTRIDGE],
                REGION_AUDIO_CPU_CARTRIDGE);
    }
    allocate_region(&r->tiles, drv->romsize[REGION_SPRITES], REGION_SPRITES);
    allocate_region(&r->game_sfix, drv->romsize[REGION_FIXED_LAYER_CARTRIDGE],
            REGION_FIXED_LAYER_CARTRIDGE);
    allocate_region(&r->gfix_usage, r->game_sfix.size >> 5,
            REGION_GAME_FIX_USAGE);

    allocate_region(&r->adpcma, drv->romsize[REGION_AUDIO_DATA_1],
            REGION_AUDIO_DATA_1);
    allocate_region(&r->adpcmb, drv->romsize[REGION_AUDIO_DATA_2],
            REGION_AUDIO_DATA_2);

    /* Allocate bios if necessary */
    gn_log(GN_LOG_DBG, "BIOS SIZE %08x %08x %08x\n", drv->romsize[REGION_MAIN_CPU_BIOS],
            drv->romsize[REGION_AUDIO_CPU_BIOS],
            drv->romsize[REGION_FIXED_LAYER_BIOS]);
    if (drv->romsize[REGION_MAIN_CPU_BIOS] != 0) {
        r->info.flags |= HAS_CUSTOM_CPU_BIOS;
        allocate_region(&r->bios_m68k, drv->romsize[REGION_MAIN_CPU_BIOS],
                REGION_MAIN_CPU_BIOS);
    }
    if (drv->romsize[REGION_AUDIO_CPU_BIOS] != 0) {
        r->info.flags |= HAS_CUSTOM_AUDIO_BIOS;
        allocate_region(&r->bios_audio, drv->romsize[REGION_AUDIO_CPU_BIOS],
                REGION_AUDIO_CPU_BIOS);
    }
    if (drv->romsize[REGION_FIXED_LAYER_BIOS] != 0) {
        r->info.flags |= HAS_CUSTOM_SFIX_BIOS;
        allocate_region(&r->bios_sfix, drv->romsize[REGION_FIXED_LAYER_BIOS],
                REGION_FIXED_LAYER_BIOS);
    }

    iloadbuf = malloc(LOAD_BUF_SIZE);

    /* Now, load the roms */
    read_counter = 0;
    romsize = 0;
    for (unsigned i = 0; i < drv->nb_romfile; i++)
        romsize += drv->rom[i].size;

    for (unsigned i = 0; i < drv->nb_romfile; i++) {
        //      gn_update_pbar(i, drv->nb_romfile);
        if (load_region(gz, r, drv->rom[i].region, drv->rom[i].src,
                drv->rom[i].dest, drv->rom[i].size, drv->rom[i].crc,
                drv->rom[i].filename) != 0) {
            /* File not found in the roms, try the parent */
            if (gzp) {
                int region = drv->rom[i].region;
                int pi;
                pi = load_region(gzp, r, drv->rom[i].region, drv->rom[i].src,
                        drv->rom[i].dest, drv->rom[i].size, drv->rom[i].crc,
                        drv->rom[i].filename);
                gn_log(GN_LOG_DBG, "From parent %d\n", pi);
                if (pi && (region != 5 && region != 0 && region != 7)) {
                    gn_log(GN_LOG_ERR, "File not found: %s\n", drv->rom[i].filename);
                    goto error1;
                }
            } else {
                int region = drv->rom[i].region;
                if (region != 5 && region != 0 && region != 7) {
                    gn_log(GN_LOG_ERR, "File not found: %s\n", drv->rom[i].filename);
                    goto error1;
                }

            }
        }

    }

    /* Close/clean up */
    gn_close_zip(gz);
    if (gzp) gn_close_zip(gzp);

    if (r->adpcmb.size == 0) {
        r->adpcmb.p = r->adpcma.p;
        r->adpcmb.size = r->adpcma.size;
    }

    memory.fix_game_usage = r->gfix_usage.p; //malloc(r->game_sfix.size >> 5);
    memory.nb_of_tiles = r->tiles.size >> 7;

    free(iloadbuf);

    /* Init rom and bios */
    init_roms(r);
    convert_all_tile(r);
    return dr_load_bios(r);

error1:
    //unzClose(gz);
    //if (gzp) unzClose(gzp);
    gn_close_zip(gz);
    if (gzp)
        gn_close_zip(gzp);

    free(drv);
    return 0;
}

int dr_load_game(char *name) {
    //GAME_ROMS rom;
    int rc;

    memory.bksw_handler = 0;
    memory.bksw_unscramble = NULL;
    memory.bksw_offset = NULL;

    rc = dr_load_roms(&memory.rom, rpath, name);
    if (rc == 0) {
        return 0;
    }
    conf.game = memory.rom.info.name;
    /* TODO *///neogeo_fix_bank_type =0;
    /* TODO */
    //  set_bankswitchers(0);

    memcpy(memory.game_vector, memory.rom.cpu_m68k.p, 0x80);
    memcpy(memory.rom.cpu_m68k.p, memory.rom.bios_m68k.p, 0x80);

    convert_all_char(memory.rom.game_sfix.p, memory.rom.game_sfix.size,
            memory.fix_game_usage);

    /* TODO: Move this somewhere else. */
    init_video();

    return 1;

}

void dr_free_roms(GAME_ROMS *r) {
    free_region(&r->cpu_m68k);
    free_region(&r->cpu_z80c);

    gn_log(GN_LOG_DBG, "Free tiles\n");
    free_region(&r->tiles);
    free_region(&r->game_sfix);

    free_region(&r->cpu_z80);
    free_region(&r->bios_audio);
    if (r->adpcmb.p != r->adpcma.p)
        free_region(&r->adpcmb);
    else {
        r->adpcmb.p = NULL;
        r->adpcmb.size = 0;
    }

    free_region(&r->adpcma);

    free_region(&r->bios_m68k);
    free_region(&r->bios_sfix);

    free(memory.ng_lo);
    free(memory.fix_game_usage);
    free_region(&r->spr_usage);

    free(r->info.name);
    free(r->info.longname);

    conf.game = NULL;
}

void open_nvram(char *name) {
    char filename[256];
    FILE *f;

    sprintf(filename, "%s/%s.nv", gngeo_dir, name);

    if ((f = fopen(filename, "rb")) == 0)
        return;
    fread(memory.sram, 1, 0x10000, f);
    fclose(f);

}

/* TODO: multiple memcard */
void open_memcard(char *name) {
    char filename[256];
    FILE *f;

    sprintf(filename, "%s/%s.mcr", gngeo_dir, name);

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

    sprintf(filename, "%s/%s.nv", gngeo_dir, name);

    if ((f = fopen(filename, "wb")) != NULL) {
        fwrite(memory.sram, 1, 0x10000, f);
        fclose(f);
    }
}

void save_memcard(char *name) {
    char filename[256];
    FILE *f;

    sprintf(filename, "%s/%s.mcr", gngeo_dir, name);

    if ((f = fopen(filename, "wb")) != NULL) {
        fwrite(memory.memcard, 1, 0x800, f);
        fclose(f);
    }
}

int close_game(void) {
    if (conf.game == NULL) return 0;
    save_nvram(conf.game);
    save_memcard(conf.game);

    dr_free_roms(&memory.rom);

    return 1;
}

int init_game(char *rom_name) {
    gn_m68k_banksw_set(0, 0); // Set default bankswitch function

    if (dr_load_game(rom_name) == 0) {
        printf("Can't load %s\n", rom_name);
        return 0;
    }

    // Byteswap the ROM data to avoid having to do so on every read
    swapb16_range(memory.rom.bios_m68k.p, memory.rom.bios_m68k.size);
    swapb16_range(memory.rom.cpu_m68k.p, memory.rom.cpu_m68k.size);
    swapb16_range(memory.game_vector, 0x80);

    open_nvram(conf.game);
    open_memcard(conf.game);

    gn_init();

    fix_usage = memory.fix_board_usage;
    current_pal = memory.vid.pal_neo[0];
    current_fix = memory.rom.bios_sfix.p;
    current_pc_pal = (uint32_t *) memory.vid.pal_host[0];

    memory.vid.currentpal=0;
    memory.vid.currentfix=0;

    return 1;
}
