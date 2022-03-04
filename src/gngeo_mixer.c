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

#include "gngeo_mixer.h"

#include "ym2610/2610intf.h"
#include "ym2610/ym2610.h"

static int16_t *abuf = NULL; // Buffer to output resampled data into
static size_t samplerate = 44100; // Default sample rate is 48000Hz
static size_t framerate = 60; // Default to 60 for NTSC

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

void gn_mixer_resamp(void) {
    // Update buffer
    YM2610Update_stream(samplerate / framerate);

    // No resampling, just pass the samples to the frontend
    gn_mixer_cb((samplerate / framerate) << 1);
}
