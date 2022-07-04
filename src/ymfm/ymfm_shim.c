/*
Copyright (c) 2022 Rupert Carmichael
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ymfm.h"
#include "ymfm_opn.h"
#include "ymfm_shim.h"

#include "emu.h"

#define SIZE_YMBUF 2048

static int16_t ymbuf[SIZE_YMBUF];
static size_t bufpos;
static int busytimer;
static int busyfrac;
static int timer[2];
static int divisor;
static int32_t output[3];

uint8_t ymfm_external_read(uint32_t type, uint32_t address) {
    switch (type) {
        case ACCESS_ADPCM_A:
            return address > memory.rom.adpcma.size ?
                0 : memory.rom.adpcma.p[address];
        case ACCESS_ADPCM_B:
            return address > memory.rom.adpcmb.size ?
                0 : memory.rom.adpcmb.p[address];
        default:
            return 0;
    }
}

void ymfm_external_write(uint32_t type, uint32_t address, uint8_t data) {
    if (type || address || data) { }
}

void ymfm_sync_mode_write(uint8_t data) {
    fm_engine_mode_write(data);
}

void ymfm_sync_check_interrupts(void) {
    fm_engine_check_interrupts();
}

void ymfm_set_timer(uint32_t tnum, int32_t duration) {
    if (duration >= 0)
        timer[tnum] += (duration / divisor);
    else // -1 means disabled
        timer[tnum] = duration;
}

void ymfm_set_busy_end(uint32_t clocks) {
    busytimer += clocks / divisor;
    busyfrac += clocks % divisor;
    if (busyfrac >= divisor) {
        ++busytimer;
        busyfrac -= divisor;
    }
}

bool ymfm_is_busy(void) {
    return busytimer ? true : false;
}

void ymfm_update_irq(bool asserted) {
    asserted ? cpu_z80_raise_irq(0) : cpu_z80_lower_irq();
}

static inline void ymfm_shim_timer_tick(void) {
    if (busytimer)
        --busytimer;

    for (int i = 0; i < 2; ++i) {
        if (timer[i] < 0)
            continue;
        else if (--timer[i] == 0)
            fm_engine_timer_expired(i);
    }
}

// Grab the pointer to the buffer
int16_t* ymfm_shim_get_buffer(void) {
    bufpos = 0;
    return &ymbuf[0];
}

void ymfm_shim_init(void) {
    divisor = 144; // 144 for medium fidelity, 16 for high
    ym2610_init();
    ym2610_set_fidelity(OPN_FIDELITY_MED);
    fm_engine_init();
}

void ymfm_shim_reset(void) {
    ym2610_reset();
}

size_t ymfm_shim_exec(void) {
    ymfm_shim_timer_tick();
    ym2610_generate(output);

    // Mix stereo FM/ADPCM output (0,1) with mono SSG output (2)
    ymbuf[bufpos++] = output[0] + output[2];
    ymbuf[bufpos++] = output[1] + output[2];

    return 1;
}
