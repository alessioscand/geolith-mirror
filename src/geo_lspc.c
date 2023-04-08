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

#include <stdio.h>

#include "geo.h"
#include "geo_lspc.h"

static lspc_t lspc;

static uint32_t *vbuf = NULL;
static unsigned fix_banksw = 0;

static uint8_t *fixdata = NULL;
static uint8_t *sfixdata = NULL;
static size_t sfixsize = 0;
static uint8_t *sromdata = NULL;
static size_t sromsize = 0;
static uint8_t *cromdata = NULL;
static size_t cromsize = 0;

static uint8_t *l0romdata = NULL;

// Dynamic output palette with values converted from palette RAM
static uint32_t palette[SIZE_8K];

// Set the pointer to the video buffer
void geo_lspc_set_buffer(uint32_t *ptr) {
    vbuf = ptr;
}

// Set the active Fix ROM
void geo_lspc_set_fix(unsigned f) {
    fixdata = f ? sromdata : sfixdata;
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

// Set the vertical shrink ROM
void geo_lspc_set_l0rom(uint8_t *l0) {
    l0romdata = l0; // Set L0 ROM
}

void geo_lspc_romdata(uint8_t *s, uint32_t ssz, uint8_t *c, uint32_t csz) {
    sromdata = s;
    sromsize = ssz;
    cromdata = c;
    cromsize = csz;
    if (!fixdata)
        fixdata = sromdata;
}

void geo_lspc_sfixdata(uint8_t *s, uint32_t ssz) {
    sfixdata = s;
    sfixsize = ssz;
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
    return palette[(lspc.palbank * SIZE_4K) + 4095];
}

// Draw a line of backdrop pixels
static inline void geo_lspc_bdline(void) {
    uint32_t bdcol = geo_lspc_backdrop();
    uint32_t *ptr = vbuf + (lspc.scanline * LSPC_WIDTH);
    for (unsigned i = 0; i < LSPC_WIDTH; ++i)
        ptr[i] = bdcol;
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
    return lspc.palram[((addr >> 1) & 0x0fff) + (lspc.palbank * SIZE_4K)];
}

// Write a value to the active bank of palette RAM
void geo_lspc_palram_wr(uint32_t addr, uint16_t data) {
    addr >>= 1; // The address should access 16-bit values rather than bytes
    lspc.palram[(addr & 0x0fff) + (lspc.palbank * SIZE_4K)] = data;
    geo_lspc_palconv((addr & 0x0fff) + (lspc.palbank * SIZE_4K), data);
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

static inline void geo_lspc_fixline(void) {
    /* The "fix map" is located in VRAM from 0x7000 to 0x74ff. Each map entry
       is a 16-bit value, where the bottom 12 bits are the tile number and the
       top 4 bits are the palette number. This limits the fix layer to the
       first 16 palettes of whatever palette bank is active. Tiles are mapped
       from top to bottom, 40 columns of 32 rows.

       Map Entry
       =======================================================================
       | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
       -----------------------------------------------------------------------
       | Palette Number    | Tile Number                                     |
       -----------------------------------------------------------------------

       Fix Map
       =======================================================================
       | 0x7000 | 0x7020 | 0x7040 |   ........    | 0x74a0 | 0x74c0 | 0x74e0 |
       | 0x7001 | 0x7021 | 0x7041 |   ........    | 0x74a1 | 0x74c1 | 0x74e1 |
       |                              ........                               |
       | 0x701f | 0x703f | 0x705f |   ........    | 0x74bf | 0x74df | 0x74ff |
       -----------------------------------------------------------------------
    */

    unsigned y = (lspc.scanline - LSPC_LINE_ACTIVE);

    for (unsigned x = 0; x < LSPC_FIXTILES_H; ++x) {
        // Addresses increment vertically downwards
        uint16_t entry = lspc.vram[0x7000 + (y >> 3) + (x << 5)];

        /* The 4 bits making up the palette number can be easily used to create
           an offset into palette memory by virtue of palettes being 16 values
           (a power of 2). The offset can be used in conjunction with the tile
           data to determine the exact value in palette memory that defines the
           final output pixel's colour.
        */
        unsigned poffset = ((entry >> 8) & 0xf0) + (lspc.palbank * SIZE_4K);

        /* Tiles are 8x8 pixels, with 4 bits representing a pixel --  in other
           words, representing the entry in a 16-bit palette. The pixel data is
           arranged in columns, with the second vertical half of the tile
           defined first:
           0x10, 0x18, 0x00, 0x08
           0x11, 0x19, 0x01, 0x09
           ....
           0x17, 0x1f, 0x07, 0x0f

           Each tile is 4 bytes wide and 8 bytes tall, for a total of 32 bytes.
        */
        unsigned tnum = entry & 0x0fff;
        uint8_t *tdata = &fixdata[tnum << 5];

        // Offset into video buffer to draw the tile row into
        uint32_t *voffset = vbuf + (LSPC_WIDTH * y) + (x << 3);

        // Row in the 8 pixel high tile
        unsigned row = y & 0x07;

        // If the palette entry is non-zero, output a colour
        for (unsigned p = 0, f = 0x10; p < 4; ++p, f = (f + 0x08) & 0x18) {
            uint32_t pentry = tdata[f + row] & 0x0f;
            if (pentry) voffset[p << 1] = palette[poffset + pentry];

            pentry = (tdata[f + row] >> 4) & 0x0f;
            if (pentry) voffset[(p << 1) + 1] = palette[poffset + pentry];
        }
    }
}

void geo_lspc_scanline(void) {
    // Draw only during active lines
    if (lspc.scanline >= LSPC_LINE_ACTIVE) {
        geo_lspc_bdline();
        geo_lspc_fixline();
    }

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
