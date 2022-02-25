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
#include <libgen.h>

#include <jg/jg.h>
#include <jg/jg_neogeo.h>

#include "emu.h"
#include "memory.h"
#include "roms.h"
#include "resfile.h"
#include "video.h"
#include "ym2610/2610intf.h"

#define SAMPLERATE 44100
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

static char dfilepath[256];

static void gn_settings_read(void) {
    jg_cb_settings_read(settings_gn,
        sizeof(settings_gn) / sizeof(jg_setting_t));
}

static void gn_input_refresh(void) {
    // Coin slots
    memory.intern_coin = 0x07;
    if (input_device[2]->button[0])
        memory.intern_coin &= 0x06;
    if (input_device[2]->button[1])
        memory.intern_coin &= 0x05;

    // Start buttons
    memory.intern_start = 0x8F;
    if (input_device[0]->button[5])
        memory.intern_start &= 0xfe;
    if (input_device[1]->button[5])
        memory.intern_start &= 0xfb;

    // Player 1
    memory.intern_p1 = 0xff;
    if (input_device[0]->button[0])
        memory.intern_p1 &= 0xfe;
    if (input_device[0]->button[1])
        memory.intern_p1 &= 0xfd;
    if (input_device[0]->button[2])
        memory.intern_p1 &= 0xfb;
    if (input_device[0]->button[3])
        memory.intern_p1 &= 0xf7;
    if (input_device[0]->button[6])
        memory.intern_p1 &= 0xef;
    if (input_device[0]->button[7])
        memory.intern_p1 &= 0xdf;
    if (input_device[0]->button[8])
        memory.intern_p1 &= 0xbf;
    if (input_device[0]->button[9])
        memory.intern_p1 &= 0x7f;

    // Player 2
    memory.intern_p2 = 0xff;
    if (input_device[1]->button[0])
        memory.intern_p2 &= 0xfe;
    if (input_device[1]->button[1])
        memory.intern_p2 &= 0xfd;
    if (input_device[1]->button[2])
        memory.intern_p2 &= 0xfb;
    if (input_device[1]->button[3])
        memory.intern_p2 &= 0xf7;
    if (input_device[1]->button[6])
        memory.intern_p2 &= 0xef;
    if (input_device[1]->button[7])
        memory.intern_p2 &= 0xdf;
    if (input_device[1]->button[8])
        memory.intern_p2 &= 0xbf;
    if (input_device[1]->button[9])
        memory.intern_p2 &= 0x7f;
}

extern uint16_t play_buffer[16384];

static void gn_audio(void) {
    YM2610Update_stream(audinfo.spf / CHANNELS);
    int16_t *pbuf = (int16_t*)play_buffer;
    int16_t *abuf = (int16_t*)audinfo.buf;
    for (int i = 0; i < audinfo.spf; ++i)
    abuf[i] = pbuf[i];
    jg_cb_audio(audinfo.spf);
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
    conf.raster = 1;
    conf.pal = 0; // Not currently supported
    conf.system = settings_gn[SYS].value;
    conf.country = settings_gn[REGION].value;
    return 1;
}

void jg_deinit(void) {
    close_game();
}

void jg_reset(int hard) {
}

void jg_exec_frame(void) {
    gn_input_refresh();
    main_loop();
    gn_audio();
}

int jg_game_load(void) {
    snprintf(dfilepath, sizeof(dfilepath), "%s/gngeo_data.zip", pathinfo.core);
    gn_set_datafile(dfilepath);
    gn_set_savedir(pathinfo.save);
    gn_set_romdir(dirname((char*)gameinfo.path));

    init_game((char*)gameinfo.name);

    inputinfo[0] = jg_neogeo_inputinfo(0, JG_NEOGEO_JS);
    inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_JS);
    inputinfo[2] = jg_neogeo_inputinfo(2, JG_NEOGEO_SYS);

    jg_cb_frametime(FRAMERATE);

    return 1;
}

int jg_game_unload(void) {
    return 1;
}

int jg_state_load(const char *filename) {
    return 0;
}

int jg_state_save(const char *filename) {
    return 0;
}

void jg_media_select(void) {
}

void jg_media_insert(void) {
}

jg_coreinfo_t* jg_get_coreinfo(const char *sys) {
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
}

void jg_set_inputstate(jg_inputstate_t *ptr, int port) {
    input_device[port] = ptr;
}

void jg_set_gameinfo(jg_fileinfo_t info) {
    gameinfo = info;
}

void jg_set_auxinfo(jg_fileinfo_t info, int index) {
}

void jg_set_paths(jg_pathinfo_t paths) {
    pathinfo = paths;
}
