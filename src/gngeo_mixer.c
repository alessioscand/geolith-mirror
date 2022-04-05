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

#include <stddef.h>
#include <stdint.h>

#include <speex/speex_resampler.h>

#include "gngeo_mixer.h"
#include "ymfm/ymfm_shim.h"

// 55555 becomes 56340 to compensate for 60.0Hz vs ~59.18Hz
#define SAMPLERATE_YM2610 56340 // Medium Fidelity
//#define SAMPLERATE_YM2610 530880 // High Fidelity

static int16_t *abuf = NULL; // Buffer to output resampled data into
static size_t samplerate = 44100; // Default sample rate is 44100Hz
static size_t framerate = 60; // Default to 60 for NTSC

static SpeexResamplerState *resampler = NULL;
static int err;

// Callback to notify the fronted that N samples are ready
static void (*gn_mixer_cb)(size_t);

// Set the pointer to the output audio buffer
void gn_mixer_set_buffer(int16_t *ptr) {
    abuf = ptr;
}

// Set the callback that notifies the frontend that N audio samples are ready
void gn_mixer_set_callback(void (*cb)(size_t)) {
    gn_mixer_cb = cb;
}

// Set the output sample rate
void gn_mixer_set_rate(size_t rate) {
    samplerate = rate;
}

// Deinitialize the resampler
void gn_mixer_deinit(void) {
    if (resampler) {
        speex_resampler_destroy(resampler);
        resampler = NULL;
    }
}

// Bring up the Speex resampler
void gn_mixer_init(void) {
    resampler = speex_resampler_init(2, SAMPLERATE_YM2610, samplerate, 3, &err);
}

void gn_mixer_resamp(size_t in_ym) {
    int16_t *ybuf = ymfm_shim_get_buffer();
    spx_uint32_t insamps = in_ym;
    spx_uint32_t outsamps = samplerate / framerate;

    err = speex_resampler_process_interleaved_int(resampler,
        (spx_int16_t*)ybuf, &insamps, (spx_int16_t*)abuf, &outsamps);

    gn_mixer_cb(outsamps << 1);
}
