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

#define PIXEL_PITCH 352
#define RASTER_LINES 261

typedef struct VIDEO {
    /* Video Ram&Pal */
    uint16_t ram[0x10000];
    uint8_t pal_neo[2][0x2000];
    uint8_t pal_host[2][0x4000];
    uint8_t currentpal;
    uint8_t currentfix; /* 0=bios fix */

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
}VIDEO;

extern unsigned int neogeo_frame_counter;
extern unsigned int neogeo_frame_counter_speed;

extern uint8_t *current_pal;
extern uint32_t *current_pc_pal;
extern uint8_t *current_fix;
extern uint8_t *fix_usage;
extern unsigned current_line;

void video_set_buffer(uint32_t *ptr);

void init_video(void);
void debug_draw_tile(unsigned int tileno,int sx,int sy,int zx,int zy,
             int color,int xflip,int yflip,unsigned char *bmp);
void draw_bg(void);
void draw_screen_scanline(int start_line, int refresh);
void draw_screen(void);

void mem68k_store_pal_word(uint32_t addr, uint16_t data);
void palette_set(int palnum);

#endif
