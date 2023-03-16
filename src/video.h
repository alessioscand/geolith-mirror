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
}VIDEO;

extern VIDEO vid;
extern uint8_t fix_board_usage[4096];
extern uint8_t *fix_game_usage;

extern unsigned int neogeo_frame_counter;

extern uint8_t *current_pal;
extern uint32_t *current_pc_pal;
extern uint8_t *current_fix;
extern uint8_t *fix_usage;
extern unsigned current_line;

void video_set_buffer(uint32_t*);

void video_set_l0rom(uint8_t*);
void video_romdata(uint8_t*, uint32_t, uint8_t*, uint32_t);

void video_fc_increment(void);

void init_video(void);
void deinit_video(void);
void draw_bg(void);
void draw_screen_scanline(int, int);

void convert_all_char(uint8_t*, int, uint8_t*);

void mem68k_store_pal_word(uint32_t, uint16_t);
void palette_set(int);

#endif
