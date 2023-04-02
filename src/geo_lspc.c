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

#include "geo_lspc.h"

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
    /* Type 0: Nonoe
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

void geo_lspc_init(void) {
}
