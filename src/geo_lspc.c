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

#include <stddef.h>
#include <stdint.h>

#include "geo.h"
#include "geo_lspc.h"

static lspc_t lspc;

static uint32_t *vbuf = NULL;
static unsigned fix_banksw = 0;

static uint8_t *sromdata = NULL;
static size_t sromsize = 0;
static uint8_t *cromdata = NULL;
static size_t cromsize = 0;

// Set the pointer to the video buffer
void geo_lspc_set_buffer(uint32_t *ptr) {
    vbuf = ptr;
}

// Set the Fix Layer bank switching type
void geo_lspc_set_fix_banksw(unsigned f) {
    /* Type 0: None
       Type 1: Per-line banking
       Type 2: Per-tile banking
    */
    if (f <= FIX_BANKSW_TILE)
        fix_banksw = f;
}

void geo_lspc_romdata(uint8_t *s, uint32_t ssz, uint8_t *c, uint32_t csz) {
    sromdata = s;
    sromsize = ssz;
    cromdata = c;
    cromsize = csz;
}

/* VRAM Memory Map
   ==================================================================
   | Range           | Words | Zone  | Description                  |
   ==================================================================
   | 0x0000 - 0x6fff |   28K |       | SBC1                         |
   |-------------------------|       |------------------------------|
   | 0x7000 - 0x74ff |       | Lower | Fix map                      |
   |-----------------|    4K |       |------------------------------|
   | 0x7500 - 0x7fff |       |       | Extension                    |
   ------------------------------------------------------------------
   | 0x8000 - 0x81ff |   512 |       | SBC2                         |
   |-----------------|-------|       |------------------------------|
   | 0x8200 - 0x83ff |   512 |       | SBC3                         |
   |-----------------|-------|       |------------------------------|
   | 0x8400 - 0x85ff |   512 | Upper | SBC4                         |
   |-----------------|-------|       |------------------------------|
   | 0x8600 - 0x867f |   128 |       | Sprite list (even scanlines) |
   |-----------------|-------|       |------------------------------|
   | 0x8680 - 0x86ff |   128 |       | Sprite list (odd scanlines)  |
   ------------------------------------------------------------------
   | 0x8700 - 0x87ff |   256 | Unused                               |
   ------------------------------------------------------------------
*/

// Read a palette RAM entry from the active bank
uint16_t geo_lspc_palram_rd(uint32_t addr) {
    return lspc.palram[(addr & 0x1fff) + lspc.palbank ? SIZE_4K : 0];
}

// Write a value to the active bank of palette RAM
void geo_lspc_palram_wr(uint32_t addr, uint16_t data) {
    lspc.palram[(addr & 0x1fff) + lspc.palbank ? SIZE_4K : 0] = data;
}

// Set the active palette bank
void geo_lspc_palram_bank(unsigned bank) {
    lspc.palbank = bank;
}

// Return REG_VRAMMOD as a 16-bit unsigned integer
uint16_t geo_lspc_vrammod_rd(void) {
    return lspc.vrammod;
}

// Write a value to REG_VRAMMOD as a 16-bit signed integer
void geo_lspc_vrammod_wr(int16_t mod) {
    lspc.vrammod = mod;
}

void geo_lspc_init(void) {
    lspc.palbank = 0;
    lspc.vrammod = 0;
}
