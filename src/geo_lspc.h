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

#ifndef GEO_LSPC_H
#define GEO_LSPC_H

#define LSPC_WIDTH          352
#define LSPC_WIDTH_VISIBLE  304
#define LSPC_HEIGHT         256
#define LSPC_HEIGHT_VISIBLE 224
#define LSPC_SCANLINES      264

#define FIX_BANKSW_NONE 0
#define FIX_BANKSW_LINE 1
#define FIX_BANKSW_TILE 2

typedef struct _lspc_t {
    // 64K + 4K = 68K, broken into Lower and Upper segments of 16-bit values
    uint16_t vram[(SIZE_64K + SIZE_4K) >> 1];

    // 2 banks of 256 16-entry palettes, 16-bit colours
    uint16_t palram[SIZE_16K >> 1];

    // Palette bank offset currently in use
    uint8_t palbank;

    // Signed 16-bit "modulo" value applied to VRAM address after writes
    int16_t vrammod;
} lspc_t;

void geo_lspc_set_buffer(uint32_t*);
void geo_lspc_set_fix_banksw(unsigned);

void geo_lspc_romdata(uint8_t*, uint32_t, uint8_t*, uint32_t);

uint16_t geo_lspc_palram_rd(uint32_t);
void geo_lspc_palram_wr(uint32_t, uint16_t);
void geo_lspc_palram_bank(unsigned);

uint16_t geo_lspc_vrammod_rd(void);
void geo_lspc_vrammod_wr(int16_t);

void geo_lspc_init(void);

#endif
