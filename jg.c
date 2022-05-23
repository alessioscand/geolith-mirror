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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libgen.h>

#include <jg/jg.h>
#include <jg/jg_neogeo.h>

#include "emu.h"
#include "gngeo_mixer.h"
#include "gngeo_neo.h"
#include "memory.h"
#include "roms.h"
#include "resfile.h"
#include "video.h"

#define SAMPLERATE 48000
#define FRAMERATE 60
#define CHANNELS 2
#define NUMINPUTS 3

static jg_cb_audio_t jg_cb_audio;
static jg_cb_frametime_t jg_cb_frametime;
static jg_cb_log_t jg_cb_log;
static jg_cb_rumble_t jg_cb_rumble;
static jg_cb_settings_read_t jg_cb_settings_read;

static jg_coreinfo_t coreinfo = {
    "gngeo", "Gngeo JG", "0.8.1", "neogeo", NUMINPUTS, JG_HINT_MEDIA_ARCHIVED
};

static jg_videoinfo_t vidinfo = {
    JG_PIXFMT_XRGB8888, // pixfmt
    352,                // wmax
    256,                // hmax
    304,                // w
    224,                // h
    24,                 // x
    16,                 // y
    352,                // p
    304.0/224.0,        // aspect
    NULL                // buf
};

static jg_audioinfo_t audinfo = {
    JG_SAMPFMT_INT16,
    SAMPLERATE,
    CHANNELS,
    (SAMPLERATE / FRAMERATE) * CHANNELS,
    NULL
};

static jg_pathinfo_t pathinfo;
static jg_fileinfo_t gameinfo;
static jg_inputinfo_t inputinfo[NUMINPUTS];
static jg_inputstate_t *input_device[NUMINPUTS];

// Emulator settings
static jg_setting_t settings_gn[] = {   // name, desc, default, min, max
    { "system", "", 0, 0, 2 }, // 0 = Universe BIOS, 1 = Arcade, 2 = Home
    { "region", "", 3, 0, 3 }, // 0 = US, 1 = JP, 2 = AS, 3 = EU
};

enum {
    SYS,
    REGION,
};

static void gn_settings_read(void) {
    jg_cb_settings_read(settings_gn,
        sizeof(settings_gn) / sizeof(jg_setting_t));
}

// Coin Slots, Service Button, # of Slots
static unsigned gn_input_poll_stat_a(void) {
    unsigned c = 0x07;
    if (input_device[2]->button[0]) // Coin 1
        c &= 0x06;
    if (input_device[2]->button[1]) // Coin 2
        c &= 0x05;
    return c;
}

// P1/P2 Select/Start, Memcard status, AES/MVS setting
static unsigned gn_input_poll_stat_b(void) {
    unsigned s = 0x8f;
    if (input_device[0]->button[4])
        s &= 0x8d;
    if (input_device[0]->button[5])
        s &= 0x8e;
    if (input_device[1]->button[4])
        s &= 0x87;
    if (input_device[1]->button[5])
        s &= 0x8b;
    return s;
}

// Neo Geo Joystick
static unsigned gn_input_poll_js(unsigned port) {
    unsigned b = 0xff;

    if (input_device[port]->button[0])
        b &= ~(1 << 0);
    if (input_device[port]->button[1])
        b &= ~(1 << 1);
    if (input_device[port]->button[2])
        b &= ~(1 << 2);
    if (input_device[port]->button[3])
        b &= ~(1 << 3);
    if (input_device[port]->button[6])
        b &= ~(1 << 4);
    if (input_device[port]->button[7])
        b &= ~(1 << 5);
    if (input_device[port]->button[8])
        b &= ~(1 << 6);
    if (input_device[port]->button[9])
        b &= ~(1 << 7);

    return b;
}

void jg_set_cb_audio(jg_cb_audio_t func) {
    jg_cb_audio = func;
}

void jg_set_cb_frametime(jg_cb_frametime_t func) {
    jg_cb_frametime = func;
}

void jg_set_cb_log(jg_cb_log_t func) {
    jg_cb_log = func;
}

void jg_set_cb_rumble(jg_cb_rumble_t func) {
    jg_cb_rumble = func;
}

void jg_set_cb_settings_read(jg_cb_settings_read_t func) {
    jg_cb_settings_read = func;
}

int jg_init(void) {
    gn_settings_read();
    gn_log_set_callback(jg_cb_log);
    conf.sample_rate = SAMPLERATE;
    gn_mixer_set_rate(SAMPLERATE);
    gn_mixer_init();
    conf.pal = 0; // Not currently supported
    conf.system = settings_gn[SYS].value;
    conf.country = settings_gn[REGION].value;
    return 1;
}

void jg_deinit(void) {
    gn_neo_unload(gameinfo.name);
    gn_mixer_deinit();
}

void jg_reset(int hard) {
    if (hard) { }
}

void jg_exec_frame(void) {
    gn_exec();
}

int jg_game_load(void) {
    gn_set_savedir(pathinfo.save);

    const char *ext = strrchr(gameinfo.path, '.');
    if (ext != NULL)
        ext++;

    if (!strncmp(ext, "neo", 3)) { // NEO file
        gn_set_romdir(pathinfo.bios);
        gn_neo_load(gameinfo.data, gameinfo.size);
        open_nvram((char*)gameinfo.name);
        open_memcard((char*)gameinfo.name);
        gn_init();
    }
    else { // Probably a .zip...
        gn_set_romdir(dirname((char*)gameinfo.path));
        init_game((char*)gameinfo.name);
    }

    inputinfo[0] = jg_neogeo_inputinfo(0, JG_NEOGEO_JS);
    inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_JS);
    inputinfo[2] = jg_neogeo_inputinfo(2, JG_NEOGEO_SYS);

    gn_input_set_callback(0, &gn_input_poll_js);
    gn_input_set_callback(1, &gn_input_poll_js);
    gn_input_stat_set_callback(0, &gn_input_poll_stat_a);
    gn_input_stat_set_callback(1, &gn_input_poll_stat_b);

    jg_cb_frametime(FRAMERATE);

    return 1;
}

int jg_game_unload(void) {
    return 1;
}

int jg_state_load(const char *filename) {
    if (filename) { }
    return 0;
}

int jg_state_save(const char *filename) {
    if (filename) { }
    return 0;
}

void jg_media_select(void) {
}

void jg_media_insert(void) {
}

jg_coreinfo_t* jg_get_coreinfo(const char *sys) {
    if (sys) { }
    return &coreinfo;
}

jg_videoinfo_t* jg_get_videoinfo(void) {
    return &vidinfo;
}

jg_audioinfo_t* jg_get_audioinfo(void) {
    return &audinfo;
}

jg_inputinfo_t* jg_get_inputinfo(int port) {
    return &inputinfo[port];
}

void jg_setup_video(void) {
    video_set_buffer(vidinfo.buf);
}

void jg_setup_audio(void) {
    gn_mixer_set_buffer(audinfo.buf);
    gn_mixer_set_callback(jg_cb_audio);
}

void jg_set_inputstate(jg_inputstate_t *ptr, int port) {
    input_device[port] = ptr;
}

void jg_set_gameinfo(jg_fileinfo_t info) {
    gameinfo = info;
}

void jg_set_auxinfo(jg_fileinfo_t info, int index) {
    if (info.data || index) { }
}

void jg_set_paths(jg_pathinfo_t paths) {
    pathinfo = paths;
}
