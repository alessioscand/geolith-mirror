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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "gngeo.h"
#include "video.h"

uint32_t *vbuf;
extern int neogeo_fix_bank_type;
unsigned int neogeo_frame_counter;

static uint8_t *l0rom = NULL;

char *ldda_y_skip;
char *dda_x_skip;
char ddaxskip[16][16] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
    { 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
    { 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0},
    { 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0},
    { 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0},
    { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0},
    { 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0},
    { 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0},
    { 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1},
    { 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
    { 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
    { 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
    { 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};
uint32_t ddaxskip_i[16] = {
    0x0080, 0x0880, 0x0888, 0x2888, 0x288a, 0x2a8a, 0x2aaa, 0xaaaa,
    0xaaea, 0xbaea, 0xbaeb, 0xbbeb, 0xbbef, 0xfbef, 0xfbff, 0xffff
};
uint32_t dda_x_skip_i;

char dda_y_skip[17];
uint32_t dda_y_skip_i;
uint32_t full_y_skip_i = 0xFFFE;
char full_y_skip[16] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

int neogeo_fix_bank_type = 0;
uint8_t *current_pal;
uint32_t *current_pc_pal;
uint8_t *current_fix;
uint8_t *fix_usage;
unsigned current_line;

void video_set_buffer(uint32_t *ptr) {
    vbuf = ptr;
}

#define fix_add(x, y) ((((memory.vid.ram[0x7500 + (((y-1)&31) + 32 * (x/6))] >> (5-(x%6))*2) & 3) ^ 3))

/* Drawing function generation */
#define RENAME(name) name##_tile
#define PUTPIXEL(dst,src) dst=src
#include "video_template.h"

static inline void draw_fix_char(unsigned char *buf, int start, int end) {
    unsigned int *gfxdata, myword;
    int x, y, yy;
    unsigned char col;
    uint32_t *br;
    unsigned int *paldata;
    unsigned int byte1, byte2;
    int banked, garouoffsets[32];
    int ystart = 1, yend = 32;

    //banked = (current_fix == memory.rom.game_sfix.p && memory.rom.game_sfix.size > 0x1000) ? 1 : 0;
    banked = (current_fix == memory.rom.game_sfix.p && neogeo_fix_bank_type && memory.rom.game_sfix.size > 0x1000) ? 1 : 0;
    //if (banked && conf.rom_type==MVS_CMC42)
    if (banked && neogeo_fix_bank_type == 1) {
        int garoubank = 0;
        int k = 0;
        y = 0;

        while (y < 32) {
            if (memory.vid.ram[0x7500 + k] == 0x0200 &&
                    (memory.vid.ram[0x7580 + k] & 0xff00) == 0xff00) {

                garoubank = memory.vid.ram[0x7580 + k] & 3;
                garouoffsets[y++] = garoubank;
            }
            garouoffsets[y++] = garoubank;
            k += 2;
        }
    }

    if (start != 0 && end != 0) {
        ystart = start >> 3;
        yend = (end >> 3) + 1;
        if (ystart < 1) ystart = 1;
    }

    for (y = ystart; y < yend; y++) {
        for (x = 1; x < 39; x++) {
            byte1 = memory.vid.ram[0x7000 + y + (x << 5)];
            byte2 = byte1 >> 12;
            byte1 = byte1 & 0xfff;

            if (banked) {
                switch (neogeo_fix_bank_type) {
                    case 1:
                        /* Garou, MSlug 3 */
                        byte1 += 0x1000 * (garouoffsets[(y - 2)&31] ^ 3);
                        break;
                    case 2:
                        byte1 += 0x1000 * fix_add(x, y);
                        break;
                }
            }

            if ((byte1 >= (memory.rom.game_sfix.size >> 5)) || (fix_usage[byte1] == 0x00)) continue;

            br = (uint32_t*) buf + ((y << 3)) * 352 + (x << 3) + 16;
            paldata = (unsigned int *) &current_pc_pal[16 * byte2];
            gfxdata = (unsigned int *) &current_fix[ byte1 << 5];

            for (yy = 0; yy < 8; yy++) {
                myword = gfxdata[yy];
                col = (myword >> 28)&0xf;
                if (col) br[7] = paldata[col];
                col = (myword >> 24)&0xf;
                if (col) br[6] = paldata[col];
                col = (myword >> 20)&0xf;
                if (col) br[5] = paldata[col];
                col = (myword >> 16)&0xf;
                if (col) br[4] = paldata[col];
                col = (myword >> 12)&0xf;
                if (col) br[3] = paldata[col];
                col = (myword >> 8)&0xf;
                if (col) br[2] = paldata[col];
                col = (myword >> 4)&0xf;
                if (col) br[1] = paldata[col];
                col = (myword >> 0)&0xf;
                if (col) br[0] = paldata[col];
                br += 352;
            }
        }
    }
}

/**** PALETTE ****/
static uint32_t convert_pal(uint16_t npal) {
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

static int allocate_region(ROM_REGION *r, uint32_t size) {
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
    allocate_region(&r->spr_usage, (r->tiles.size >> 11) * sizeof (uint32_t));
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

void mem68k_store_pal_word(uint32_t addr, uint16_t data) {
    addr &= 0xffff;
    if (addr <= 0x1fff) {
        current_pal[addr] = data & 0xff;
        current_pal[addr + 1] = data >> 8;

        if ((addr >> 1) & 0xF)
            current_pc_pal[(addr) >> 1] = convert_pal(data);
        else
            current_pc_pal[(addr) >> 1] = 0xF81F;
    }
}

void palette_set(int palnum) {
    current_pal = memory.vid.pal_neo[palnum];
    current_pc_pal = (uint32_t *) memory.vid.pal_host[palnum];
    memory.vid.currentpal = palnum;
}

void draw_bg(void) {
    uint32_t bgcol = current_pc_pal[4095];
    uint32_t *p = vbuf, *e = vbuf + 256*352;
    while(p < e) *(p++) = bgcol;
}

void draw_screen_scanline(int start_line, int refresh) {
    int sx = 0, sy = 0, my = 0, zx = 1, zy = 1;
    int offs, count, y;
    int tileno, tileatr;
    int tctl1, tctl2, tctl3;
    uint16_t *vidram = memory.vid.ram;

    int yy;
    int tile, yoffs;
    int zoom_line;
    int invert;
    uint8_t *zoomy_rom;

    //if (start_line > 255) start_line = 255;
    //if (end_line > 255) end_line = 255;

    /* Draw sprites */
    for (count = 0; count < 0x180; ++count) {
        tctl3 = vidram[0x8000 + count];
        tctl1 = vidram[0x8200 + count];
        tctl2 = vidram[0x8400 + count];

        /* If this bit is set this new column is placed next to last one */
        if (tctl1 & 0x40) {
            sx += zx + 1; /* new x */

            /* Get new zoom for this column */
            zx = (tctl3 >> 8) & 0x0f;
        } else { /* nope it is a new block */
            /* Sprite scaling */
            zx = (tctl3 >> 8) & 0x0f; /* zomm x */
            zy = tctl3 & 0xff; /* zoom y */

            sx = (tctl2 >> 7); /* x pos 0 - 512  */

            /* Number of tiles in this strip */
            my = tctl1 & 0x3f;

            sy = 512 - (tctl1 >> 7); /* y pos 512 - 0 */

            if (my > 0x20) my = 0x20;
        }

        /* No point doing anything if tile strip is 0 */
        if (my == 0) continue;
        if (sx >= 496) { /* after 496, we consider sx negative */
            //printf("SX=%d\n",sx);
            sx -= 512;
            //continue;
        }

        if (sx > 320) {
            continue;
            //sx-=16;
            //printf("SX=%d\n",sx);
        }

        if (sx<-16) continue;
        //sx&=0x1ff;

        /* Process x zoom */
        if (zx != 16) {
            dda_x_skip = ddaxskip[zx];
        } else zx = 16;

        offs = count << 6;
        zoomy_rom = l0rom + (zy << 8);

        //for (yy = start_line; yy <= end_line; yy++) {
            yy = start_line;
            y = yy - sy; /* y: 0 -> my*16 */

            if (y < 0) y += 512;

            if (y >= (my << 4)) continue;

            invert = 0;

            zoom_line = y & 0xff;

            if (y & 0x100) {
                zoom_line ^= 0xff; /* zoom_line = 255 - zoom_line */
                invert = 1;
            }

            if (my == 0x20) /* fix for joyjoy, trally... */ {
                if (zy) {
                    zoom_line %= (zy << 1);
                    if (zoom_line >= zy) {
                        zoom_line = (zy << 1) - 1 - zoom_line;
                        invert ^= 1;
                    }
                }
            }

            yoffs = zoomy_rom[zoom_line] & 0x0f;
            tile = zoomy_rom[zoom_line] >> 4;

            if (invert) {
                tile ^= 0x1f; // tile=31 - tile;
                yoffs ^= 0x0f; // yoffs= 15 - yoffs;
            }

            tileno = vidram[offs + (tile << 1)];
            tileatr = vidram[offs + (tile << 1) + 1];

            if (memory.nb_of_tiles > 0x10000 && (tileatr & 0x10)) tileno += 0x10000;
            if (memory.nb_of_tiles > 0x20000 && (tileatr & 0x20)) tileno += 0x20000;
            if (memory.nb_of_tiles > 0x40000 && (tileatr & 0x40)) tileno += 0x40000;

            /* animation automatique */
            if (tileatr & 0x8) tileno = (tileno&~7)+((tileno + neogeo_frame_counter)&7);
            else if (tileatr & 0x4) tileno = (tileno&~3)+((tileno + neogeo_frame_counter)&3);
            if (tileatr & 0x02) yoffs ^= 0x0f; /* flip y */

            draw_scanline_tile(tileno, yoffs, sx + 16, yy, zx, tileatr >> 8,
                tileatr & 0x01, (unsigned char*) vbuf);

        }
    //} /* for count */

    if (refresh) {
        draw_fix_char((unsigned char*)vbuf, 0, 0);
    }
}

void video_set_l0rom(uint8_t *l0) {
    l0rom = l0; // Set L0 ROM
}

void video_fc_increment(void) {
    if (memory.vid.fc >= memory.vid.fc_speed) {
        memory.vid.fc = 0;
        ++neogeo_frame_counter;
    }
    ++memory.vid.fc;
}

void init_video(void) {
    memory.vid.modulo = 1;
}
