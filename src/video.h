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

#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <stdio.h>
#include <stdint.h>

typedef struct gfx_cache {
    uint8_t *data;  /* The cache */
    uint32_t size;  /* Tha allocated size of the cache */
    uint32_t total_bank;  /* total number of rom bank */
    uint8_t **ptr/*[TOTAL_GFX_BANK]*/; /* ptr[i] Contain a pointer to cached data for bank i */
    int max_slot; /* Maximal numer of bank that can be cached (depend on cache size) */
    int slot_size;
    int *usage;   /* contain index to the banks in used order */
    FILE *gno;
    uint32_t *offset;
    uint8_t* in_buf;
}GFX_CACHE;

typedef struct VIDEO {
    /* Video Ram&Pal */
    uint16_t ram[0x10000];
    uint8_t pal_neo[2][0x2000];
    uint8_t pal_host[2][0x4000];
    uint8_t currentpal;
    uint8_t currentfix; /* 0=bios fix */
    uint16_t rbuf;

    /* Auto anim counter */
    uint32_t fc;
    uint32_t fc_speed;

    uint32_t vptr;
    int16_t modulo;

    uint32_t current_line;

    /* IRQ2 related */
    uint32_t irq2control;
    uint32_t irq2taken;
    uint32_t irq2start;
    uint32_t irq2pos;

    GFX_CACHE spr_cache;
}VIDEO;

/* RGBA 32bits per pixel, entire Neo Geo palette (65536 colors) */
typedef uint32_t buffer_pixel_t;
#define PIXEL_PITCH 352

#define RASTER_LINES 261

extern unsigned int neogeo_frame_counter;
extern unsigned int neogeo_frame_counter_speed;

void video_set_buffer(uint32_t *ptr);

void init_video(void);
void debug_draw_tile(unsigned int tileno,int sx,int sy,int zx,int zy,
             int color,int xflip,int yflip,unsigned char *bmp);
void draw_screen_scanline(int start_line, int end_line, int refresh);
void draw_screen(void);
// void show_cache(void);
int init_sprite_cache(uint32_t size,uint32_t bsize);
void free_sprite_cache(void);

#endif
