/*
 * Gngeo JG
 *
 * Copyright (C) 2022 Rupert Carmichael
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>

#include "ym2610/2610intf.h"
#include "video.h"
#include "emu.h"
#include "memory.h"
#include "debug.h"
#include "conf.h"
#include "gnutil.h"
#include "roms.h"

#include <jg/jg.h>
#include <jg/jg_neogeo.h>

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
    "name", "Gngeo JG", "0.8.1", "neogeo", NUMINPUTS, JG_HINT_MEDIA_ARCHIVED
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
    cf_init();
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
    char *rompath = dirname((char*)gameinfo.path);
    char dfilepath[256];
    snprintf(dfilepath, sizeof(dfilepath), "%s/gngeo_data.zip", pathinfo.core);
    cf_create_string_item("rompath", "", "PATH", 'i', rompath);
    cf_create_string_item("datafile", "", "PATH", 'd', dfilepath);
    cf_get_item_by_name("rompath")->flags|=CF_SYSTEMOPT;
    cf_get_item_by_name("datafile")->flags|=CF_SYSTEMOPT;

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
