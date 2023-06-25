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

static void (*geo_lspc_fixline)(void);

static uint32_t *vbuf = NULL;
static unsigned numtiles = 0;

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

// Horizontal Shrink LUT -- from neogeodev
static unsigned lut_hshrink[0x10][0x10] = {
    { 0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0 }, // (15 pixel skipped, 1 remaining)
    { 0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0 }, // (14 pixels skipped...)
    { 0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0 },
    { 0,0,1,0,1,0,0,0,1,0,0,0,1,0,0,0 },
    { 0,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0 },
    { 0,0,1,0,1,0,1,0,1,0,0,0,1,0,1,0 },
    { 0,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0 },
    { 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0 },
    { 1,0,1,0,1,0,1,0,1,1,1,0,1,0,1,0 },
    { 1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,0 },
    { 1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,1 },
    { 1,0,1,1,1,0,1,1,1,1,1,0,1,0,1,1 },
    { 1,0,1,1,1,0,1,1,1,1,1,0,1,1,1,1 },
    { 1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1 },
    { 1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1 }, // (...1 pixel skipped)
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }, // (no pixels skipped, full size)
};

// Set the pointer to the video buffer
void geo_lspc_set_buffer(uint32_t *ptr) {
    vbuf = ptr;
}

// Set the active Fix ROM
void geo_lspc_set_fix(unsigned f) {
    fixdata = f ? sromdata : sfixdata;
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
    numtiles = csz >> 7;
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
    lspc.vrambank = addr & 0x8000;
}

// Read from VRAM
uint16_t geo_lspc_vram_rd(void) {
    return lspc.vram[lspc.vramaddr];
}

// Write to VRAM
void geo_lspc_vram_wr(uint16_t data) {
    // Writing beyond the boundary is not a winning endeavour
    if (lspc.vramaddr < 0x8800)
        lspc.vram[lspc.vramaddr] = data; // Perform the write

    // Apply the modulo after the write, wrapping within the correct bank
    lspc.vramaddr = ((lspc.vramaddr + lspc.vrammod) & 0x7fff) | lspc.vrambank;
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
    /* Bits 8-15: Auto animation speed
       Bits 5-7:  Timer interrupt mode
       Bit 4:     Timer interrupt enable
       Bit 3:     Disable auto animation
       Bits 0-2:  Unused
    */
    lspc.aa_reload = (data >> 8);
    irq2_ctrl = data & 0xf0;
    lspc.aa_disable = data & 0x08;
}

void geo_lspc_init(void) {
    lspc.palbank = 0;
    lspc.vramaddr = 0;
    lspc.vrammod = 0;
    lspc.aa_counter = 0;
    lspc.aa_disable = 0;
    lspc.aa_reload = 0;
    lspc.aa_timer = 0;
    lspc.scanline = 0;
}

// No Fix Layer Banking (Default)
static void geo_lspc_fixline_default(void) {
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

    unsigned line = lspc.scanline - LSPC_LINE_BORDER_TOP;

    for (unsigned x = 0; x < LSPC_FIXTILES_H; ++x) {
        // Addresses increment vertically downwards
        uint16_t entry = lspc.vram[0x7000 + (line >> 3) + (x << 5)];

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
        uint32_t *voffset = vbuf + (LSPC_WIDTH * lspc.scanline) + (x << 3);

        // Row in the 8 pixel high tile
        unsigned row = line & 0x07;

        // If the palette entry is non-zero, output a colour
        uint32_t pentry = 0;
        for (unsigned p = 0, f = 0x10; p < 4; ++p, f = (f + 0x08) & 0x18) {
            pentry = tdata[f + row] & 0x0f;
            if (pentry) voffset[p << 1] = palette[poffset + pentry];

            pentry = (tdata[f + row] >> 4) & 0x0f;
            if (pentry) voffset[(p << 1) + 1] = palette[poffset + pentry];
        }
    }
}

// Per-Line Banking (Type 1)
static void geo_lspc_fixline_line(void) {
    /* Most everything is the same as the default fix layer drawing routine,
       with the exception of banking offsets which work for a minimum of two
       rows whenever switched. The documentation on this is less than ideal
       at the time this code was written, and the algorithm was shamelessly
       taken from MAME. This is an area where more reverse engineering would
       be beneficial, but it works well enough for the three games using it to
       run without issue.
    */
    unsigned line = lspc.scanline - LSPC_LINE_BORDER_TOP;
    unsigned trow = line >> 3; // Tile row

    unsigned offsets[34];
    unsigned bank = 0;
    unsigned k = 0;
    unsigned y = 0;
    while (y < 32) {
        /* A value of 0x0200 in the 0x7500-0x753f block indicates a bank switch
           should be done. A corresponding value in 0x7580-0x75bf containing
           all 1s in the upper 8 bits will determine the bank using the
           complement of the lowest 3 bits.
        */
        if (lspc.vram[0x7500 + k] == 0x0200 &&
            (lspc.vram[0x7580 + k] & 0xff00) == 0xff00) {
            // Choose one of 4 banks of 4096 tiles
            bank = (~lspc.vram[0x7580 + k] & 0x03) * SIZE_4K;
            offsets[y++] = bank;
        }
        offsets[y++] = bank;
        k += 2;
    }

    for (unsigned x = 0; x < LSPC_FIXTILES_H; ++x) {
        uint16_t entry = lspc.vram[0x7000 + trow + (x << 5)];
        unsigned poffset = ((entry >> 8) & 0xf0) + (lspc.palbank * SIZE_4K);
        unsigned tnum = (entry & 0x0fff) + offsets[(trow - 2) & 0x1f];
        uint8_t *tdata = &fixdata[tnum << 5];
        uint32_t *voffset = vbuf + (LSPC_WIDTH * lspc.scanline) + (x << 3);
        unsigned row = line & 0x07;
        uint32_t pentry = 0;
        for (unsigned p = 0, f = 0x10; p < 4; ++p, f = (f + 0x08) & 0x18) {
            pentry = tdata[f + row] & 0x0f;
            if (pentry) voffset[p << 1] = palette[poffset + pentry];

            pentry = (tdata[f + row] >> 4) & 0x0f;
            if (pentry) voffset[(p << 1) + 1] = palette[poffset + pentry];
        }
    }
}

// Per-Tile Banking (Type 2)
static void geo_lspc_fixline_tile(void) {
    unsigned line = lspc.scanline - LSPC_LINE_BORDER_TOP;
    unsigned trow = line >> 3; // Tile row

    for (unsigned x = 0; x < LSPC_FIXTILES_H; ++x) {
        uint16_t entry = lspc.vram[0x7000 + (line >> 3) + (x << 5)];
        unsigned poffset = ((entry >> 8) & 0xf0) + (lspc.palbank * SIZE_4K);

        /* The lower 12 bits of 16-bit words stored at 0x7500-0x75df are used
           to control the bank offset for 6 consecutive horizontal tiles -- 2
           bits each, complemented, select one of 4 banks of 4096 tiles. This
           algorithm is shamelessly taken from MAME.
        */
        unsigned offset =
            ~(lspc.vram[0x7500 + ((trow - 1) & 0x1f) + 32 * (x / 6)] >>
            (5 - (x % 6)) * 2) & 0x03;
        unsigned tnum = (entry & 0x0fff) + (offset * SIZE_4K);
        uint8_t *tdata = &fixdata[tnum << 5];

        // Offset into video buffer to draw the tile row into
        uint32_t *voffset = vbuf + (LSPC_WIDTH * lspc.scanline) + (x << 3);

        // Row in the 8 pixel high tile
        unsigned row = line & 0x07;

        // If the palette entry is non-zero, output a colour
        uint32_t pentry = 0;
        for (unsigned p = 0, f = 0x10; p < 4; ++p, f = (f + 0x08) & 0x18) {
            pentry = tdata[f + row] & 0x0f;
            if (pentry) voffset[p << 1] = palette[poffset + pentry];

            pentry = (tdata[f + row] >> 4) & 0x0f;
            if (pentry) voffset[(p << 1) + 1] = palette[poffset + pentry];
        }
    }
}

// Set the Fix Layer bank switching type
void geo_lspc_set_fix_banksw(unsigned f) {
    /* Type 0: None
       Type 1: Per-line banking
       Type 2: Per-tile banking
    */
    switch (f) {
        default: {
            geo_lspc_fixline = &geo_lspc_fixline_default; break;
        }
        case FIX_BANKSW_LINE: {
            geo_lspc_fixline = &geo_lspc_fixline_line; break;
        }
        case FIX_BANKSW_TILE: {
            geo_lspc_fixline = &geo_lspc_fixline_tile; break;
        }
    }
}

static inline unsigned geo_lspc_tpix(unsigned tbase, unsigned x, unsigned y) {
    /* Sprite Tile Decoding
       Tiles are 8x8 with palette index values represented in a planar format,
       which spans 4 bytes per horizontal line. Each bit in a byte represents
       one component of a 4-bit palette entry:
       =================================
       | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | Take the tile's base address in C ROM
       ================================= and shift right by the X offset in the
    v0 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 1 | tile to isolate the correct bit. Do
    v1 | 0 | 1 | 0 | 0 | 0 | 1 | 0 | 1 | this for 4 consecutive bytes, then
    v2 | 1 | 0 | 1 | 1 | 0 | 0 | 1 | 1 | shift and OR the bits together to
    v3 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | create a 4-bit palette entry.
       ---------------------------------

       Bytes 0 and 1 come from odd numbered C ROMs, while bytes 2 and 3 come
       from even numbered C ROMs. Because the data from the C ROM pairs are
       interleaved every byte, the second byte is in the third position, and
       the third byte is in the second position.

       Notes: Since there are 4 bytes per row of pixels, multiply the tile's
              base address by 4 to select the specific row in the tile.
              The pixel data is stored right to left -- this means it is in
              reverse order from how it is displayed unless horizontal flip
              is enabled for the tile.
    */
    unsigned v0 = (cromdata[(tbase + 0) + (y << 2)]) >> (x);
    unsigned v1 = (cromdata[(tbase + 2) + (y << 2)]) >> (x);
    unsigned v2 = (cromdata[(tbase + 1) + (y << 2)]) >> (x);
    unsigned v3 = (cromdata[(tbase + 3) + (y << 2)]) >> (x);
    return (v0 & 0x01) | (v1 & 0x01) << 1 | (v2 & 0x01) << 2 | (v3 & 0x01) << 3;
}

static inline void geo_lspc_pixel(unsigned x, unsigned y, uint32_t c) {
    if (x < LSPC_WIDTH && y < LSPC_HEIGHT)
        vbuf[x + (y * LSPC_WIDTH)] = c;
}

static inline void geo_lspc_sprline(void) {
    unsigned line = lspc.scanline - LSPC_LINE_BORDER_TOP;
    unsigned sprcount = 0; // Counter for sprite limit

    unsigned xpos = 0;
    unsigned ypos = 0;
    unsigned sprsize = 0;
    unsigned hshrink = 0x0f; // Start at full width (no shrinking)
    unsigned vshrink = 0xff; // Start at full height (no shrinking)

    for (unsigned i = 0; i < 381; ++i) {
        if (lspc.vram[0x8200 + i] & 0x40) { // Sticky/Chain bit set
            /* Attach this sprite to the edge of the previous one, and do not
               set a new Y position or size/height. Account for any horizontal
               shrinking from the last sprite by using its hshrink value
            */
            xpos += (hshrink + 1);
        }
        else {
            xpos = (lspc.vram[0x8400 + i] >> 7) & 0x1ff;
            ypos = (lspc.vram[0x8200 + i] >> 7) & 0x1ff; // 512-Y
            sprsize = lspc.vram[0x8200 + i] & 0x3f; // Height in tiles
            vshrink = lspc.vram[0x8000 + i] & 0xff;

            // Special value, loops border FIXME need test case/documentation
            if (sprsize == 33)
                sprsize = 32;
        }

        // Set horizontal shrinking value
        hshrink = (lspc.vram[0x8000 + i] >> 8) & 0x0f;

        // Sprite Row - vertical offset for the line of the sprite to be drawn
        unsigned srow = (line - (0x200 - ypos)) & 0x1ff;

        // If no rows of the sprite are on this line, this iteration is done
        if ((srow >= (sprsize << 4)) || (sprsize == 0))
            continue;

        // 96 sprites may be drawn per line
        if (sprcount++ == 96)
            break;

        /* L0 ROM is a 64K ROM chip containing vertical shrinking values. The
           data is made up of 256 tables of 256 bytes. The vshrink value from
           SBC2 selects the table. For the first 256 lines (top half of a full
           sprite), the index in the table is the line number of the sprite
           currently being drawn (sprite row). For the last 256 lines (bottom
           half of a full sprite), the index in the table is complemented.
           For the first half, the upper nybble of the value contains the index
           in the tilemap (0-15), with the lower nybble being the line number
           of the tile. For the second half, the upper nybble is the index with
           a XOR 0x1f applied (16-31, meaning the 8th bit is always set), with
           the lower nybble being the line number in the tile with a XOR 0xf
           applied.
        */
        srow = srow > 0xff ?
            (l0romdata[(vshrink << 8) + ((~srow) & 0xff)] ^ 0x1ff) :
            l0romdata[(vshrink << 8) + (srow & 0xff)];

        /* Each sprite has a 64-word table, where each pair of values
           correspond to a tile in the sprite, top to bottom. Even 16-bit words
           contain tile number LSBs, while odd 16-bit words contain palette,
           tile number MSBs, auto-animation bits, and the vertical and
           horizontal flip bits.

           Sprite tiles are 16 pixels high, with a maximum height of 32 tiles,
           for a total of 512 pixels. Dividing the sprite row by 8 will
           determine which word pair to use, and masking off the lowest bit
           will give the offset to the even word. Adding 1 to this gives the
           odd word. The max value is 62, so use 0x3e as the mask.
        */

        // Offset to the even word of this tile's map entry
        unsigned tmapoffset = (i << 6) + ((srow >> 3) & 0x3e);

        unsigned tnum = (lspc.vram[tmapoffset] |
            ((lspc.vram[tmapoffset + 1] & 0x00f0) << 12)) % numtiles;

        // Horizontal and Vertical flip
        unsigned hflip = lspc.vram[tmapoffset + 1] & 0x01;
        unsigned vflip = lspc.vram[tmapoffset + 1] & 0x02;

        // Auto-animation bits
        unsigned aabits = (lspc.vram[tmapoffset + 1] & 0x0c) >> 2;

        // Palette offset -- 0th entry of nth palette
        unsigned poffset = ((lspc.vram[tmapoffset + 1] >> 4) & 0x0ff0) +
            (lspc.palbank * SIZE_4K);

        /* Auto-animation
           ===================================================
           | Bit 3 | Bit 2 | Effect                          |
           ===================================================
           |     0 |     0 | No auto-animation for this tile |
           |     0 |     1 | Auto-animation over 4 tiles     |
           |     1 |     0 | Auto-animation over 8 tiles     |
           |     1 |     1 | Auto-animation over 8 tiles     |
           ---------------------------------------------------
           When Bit 3 is set, auto-animation happens over 8 tiles, regardless
           of Bit 2. Bit 2 activates auto-animation over 4 tiles when it is the
           only one set. The auto-animation counter values replace the bottom 2
           or 3 bits of the tile number.
        */
        if (!lspc.aa_disable) {
            switch (aabits) {
                case 1: {
                    tnum &= ~0x03;
                    tnum |= lspc.aa_counter & 0x03;
                    break;
                }
                case 2: case 3: {
                    tnum &= ~0x07;
                    tnum |= lspc.aa_counter & 0x07;
                    break;
                }
                default: {
                    break;
                }
            }
        }

        /* Tiles are 128 bytes: 16 * 16 pixels * 4 bits per pixel = 1024 bits
           Multiply the tile number by 128 to get the offset of the tile in
           C ROM.
        */
        unsigned toffset = tnum << 7;

        // Y value in the sprite tile to be drawn, flipped if necessary
        unsigned y = vflip ? (0x0f - (srow & 0x0f)) : (srow & 0x0f);

        unsigned pentry = 0; // Palette entry

        /* If hflip is enabled, the horizontal order of the tiles is reversed,
           as are the pixels in each tile. To make this work in a single loop,
           some magic was done using XOR to either enable or disable a 64 byte
           tile address offset in the middle of the loop, and to invert the
           order in which pixels are drawn.
        */
        unsigned ftile = hflip ? 0x00 : 0x08; // Reverse tile address
        unsigned fpix = hflip ? 0x07 : 0x00; // Reverse pixel drawing order

        // Draw position must be separate from loop iterator due to hshrink
        unsigned drawpos = 0;

        for (unsigned p = 0; p < 16; ++p) {
            if (lut_hshrink[hshrink][p]) {
                pentry = geo_lspc_tpix(toffset + (((0x08 & p) ^ ftile) << 3),
                    (p & 0x07) ^ fpix, y);
                if (pentry) {
                    geo_lspc_pixel((xpos + drawpos) & 0x1ff, lspc.scanline,
                        palette[poffset + pentry]);
                }
                ++drawpos; // Increment for coloured and transparent pixels
            }
        }
    }
}

static inline void geo_lspc_aa(void) {
    /* An internal 8-bit timer is ticked down each frame. When it
       underflows, two things happen:
       - The timer is reloaded with the 8 highest bits of REG_LSPCMODE
       - An internal 3-bit animation counter is incremented
    */
    if (--lspc.aa_timer == 0xff) {
        lspc.aa_counter = (lspc.aa_counter + 1) & 0x07;
        lspc.aa_timer = lspc.aa_reload;
    }
}

void geo_lspc_scanline(void) {
    // Always draw backdrop
    geo_lspc_bdline();

    // Draw only during active lines
    if (lspc.scanline >= LSPC_LINE_BORDER_TOP &&
        lspc.scanline < (LSPC_LINE_BORDER_TOP + LSPC_LINE_BORDER_BOTTOM)) {
        geo_lspc_sprline();
        geo_lspc_fixline();
    }

    if (++lspc.scanline == LSPC_SCANLINES) // Handle final scanline
        lspc.scanline = 0;
    else if (lspc.scanline == LSPC_LINE_BORDER_TOP)
        geo_lspc_aa();
}
