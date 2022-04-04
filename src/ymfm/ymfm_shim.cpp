/*
MIT License

Copyright (c) 2022 Rupert Carmichael

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "ymfm.h"
#include "ymfm_opn.h"
#include "ymfm_shim.h"

extern "C" {
    #include "memory.h"
}

#define SIZE_YMBUF 2048

static int16_t ymbuf[SIZE_YMBUF];
static size_t bufpos;
static int busytimer;
static int busyfrac;
static int timer[2];
static int divisor;
static int32_t output[3];

extern "C" {
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
}
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

void ymfm_shim_deinit(void) {
}

void ymfm_shim_reset(void) {
    ym2610_reset();
}

uint8_t ymfm_shim_rd(uint16_t addr) {
    return ym2610_read(addr);
}

void ymfm_shim_wr(uint16_t addr, uint8_t data) {
    ym2610_write(addr, data);
}

size_t ymfm_shim_exec(void) {
    ymfm_shim_timer_tick();
    ym2610_generate(output);

    // Mix stereo FM/ADPCM output (0,1) with mono SSG output (2)
    ymbuf[bufpos++] = output[0] + output[2];
    ymbuf[bufpos++] = output[1] + output[2];

    return 1;
}
