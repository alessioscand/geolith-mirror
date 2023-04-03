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

// Dynamic output palette with values converted from palette RAM
static uint32_t palette[SIZE_8K];

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

// Return the backdrop colour -- the last value in the active palette bank
static inline uint32_t geo_lspc_backdrop(void) {
    return palette[(lspc.palbank ? SIZE_4K : 0) + 4095];
}

// Convert a palette RAM entry to a host-friendly output value
static inline void geo_lspc_palconv(uint16_t addr, uint16_t data) {
    /* Colour Format
       =================================================
       |D0|R1|G1|B1|R5|R4|R3|R2|G5|G4|G3|G2|B5|B4|B3|B2|
       =================================================
       Colour values are 6 bits, made up of 5 colour bits and a global "dark"
       bit which acts as the least significant bit for the R, G, and B values.
       These values can simply be shifted left twice to give 8-bit equivalent
       values which can be ORed together for output.
    */
    unsigned r = (((data >> 6) & 0x3c) | ((data >> 13) & 0x02) |
        ((data >> 15) & 0x01)) << 2;
    unsigned g = (((data >> 2) & 0x3c) | ((data >> 12) & 0x02) |
        ((data >> 15) & 0x01)) << 2;
    unsigned b = (((data << 2) & 0x3c) | ((data >> 11) & 0x02) |
        ((data >> 15) & 0x01)) << 2;
    palette[addr] = 0xff000000 | (r << 16) | (g << 8) | b;
}

// Read a palette RAM entry from the active bank
uint16_t geo_lspc_palram_rd(uint32_t addr) {
    return lspc.palram[((addr >> 1) & 0xfff) + lspc.palbank ? SIZE_4K : 0];
}

// Write a value to the active bank of palette RAM
void geo_lspc_palram_wr(uint32_t addr, uint16_t data) {
    addr >>= 1; // The address should access 16-bit values rather than bytes
    lspc.palram[(addr & 0xfff) + (lspc.palbank ? SIZE_4K : 0)] = data;
    geo_lspc_palconv((addr & 0xfff) + (lspc.palbank ? SIZE_4K : 0), data);
}

// Set the active palette bank
void geo_lspc_palram_bank(unsigned bank) {
    lspc.palbank = bank;
}

// Write the VRAM Address to work with
void geo_lspc_vramaddr_wr(uint16_t addr) {
    lspc.vramaddr = addr;
}

// Read from VRAM
uint16_t geo_lspc_vram_rd(void) {
    return lspc.vram[lspc.vramaddr];
}

// Write to VRAM
void geo_lspc_vram_wr(uint16_t data) {
    lspc.vram[lspc.vramaddr] = data; // Perform the write
    lspc.vramaddr += lspc.vrammod; // Apply the modulo after the write
}

// Return REG_VRAMMOD as a 16-bit unsigned integer
uint16_t geo_lspc_vrammod_rd(void) {
    return lspc.vrammod;
}

// Write a value to REG_VRAMMOD as a 16-bit signed integer
void geo_lspc_vrammod_wr(int16_t mod) {
    lspc.vrammod = mod;
}

// Read REG_LSPCMODE
uint16_t geo_lspc_mode_rd(void) {
    /* Bits 7-15: Raster line counter (with offset of 0xf8)
       Bits 4-6:  000
       Bit 3:     0 = 60Hz, 1 = 50Hz
       Bits 0-2:  Auto animation counter
    */
    // FIXME - support 50Hz
    return ((lspc.scanline + 0xf8) << 7) | lspc.aa_counter;
}

// Write to REG_LSPCMODE
void geo_lspc_mode_wr(uint16_t data) {
    /* Bits 8-15: Auto animation speed - tick every N+1 frames
       Bits 7-5:  Timer interrupt mode
       Bit 4:     Timer interrupt enable
       Bit 3:     Disable auto animation
       Bits 0-2:  Unused
    */
    lspc.aa_reload = (data >> 8) + 1;
    irq2_ctrl = data & 0xf0;
    lspc.aa_disable = data & 0x08;
}

void geo_lspc_init(void) {
    lspc.palbank = 0;
    lspc.vramaddr = 0;
    lspc.vrammod = 0;
    lspc.aa_counter = 0;
    lspc.aa_disable = 0;
    lspc.aa_speed = 0;
    lspc.aa_ticks = 0;
    lspc.scanline = 0;
}

void geo_lspc_scanline(void) {
    if (++lspc.scanline == LSPC_SCANLINES) { // Handle final scanline
        lspc.scanline = 0;

        /* Tick the auto animation timer every frame, and increment the counter
           when the timer reaches the auto animation speed setting. These
           timers run even if auto animation is disabled.
        */
        if (++lspc.aa_ticks >= lspc.aa_speed) {
            // Reset auto animation timer
            lspc.aa_ticks = 0;

            // Reload auto animation speed value
            lspc.aa_speed = lspc.aa_reload;

            // Increment auto animation counter (3 bits)
            lspc.aa_counter = (lspc.aa_counter + 1) & 0x07;
        }
    }
}
