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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libgen.h>

#include <jg/jg.h>
#include <jg/jg_neogeo.h>

#include "geo.h"
#include "geo_lspc.h"
#include "geo_m68k.h"
#include "geo_mixer.h"
#include "geo_neo.h"
#include "version.h"

#define SAMPLERATE 48000
#define FRAMERATE 60
#define CHANNELS 2
#define NUMINPUTS 3

static jg_cb_audio_t jg_cb_audio;
static jg_cb_frametime_t jg_cb_frametime;
static jg_cb_log_t jg_cb_log;
static jg_cb_rumble_t jg_cb_rumble;

static jg_coreinfo_t coreinfo = {
    "geolith", "Geolith", JG_VERSION, "neogeo", NUMINPUTS, 0
};

static jg_videoinfo_t vidinfo = {
    JG_PIXFMT_XRGB8888,     // pixfmt
    LSPC_WIDTH,             // wmax
    LSPC_SCANLINES,         // hmax
    LSPC_WIDTH_VISIBLE,     // w
    LSPC_HEIGHT_VISIBLE,    // h
    8,                      // x
    24,                     // y
    LSPC_WIDTH,             // p
    304.0/224.0,            // aspect
    NULL                    // buf
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
static jg_setting_t settings_geo[] = {
    { "input", "Input Devices",
      "0 = Auto, 1 = Neo Geo Joysticks, 2 = Mahjong",
      "Select the desired input device(s)",
      0, 0, 2, JG_SETTING_RESTART
    },
    { "system", "Emulated System",
      "0 = AES (Console), 1 = MVS (Arcade), 2 = Universe BIOS",
      "Select which type of system/BIOS to emulate",
      SYSTEM_AES, SYSTEM_AES, SYSTEM_UNI, JG_SETTING_RESTART
    },
    { "region", "Region",
      "0 = US, 1 = JP, 2 = AS, 3 = EU",
      "Select the region",
      REGION_US, REGION_US, REGION_EU, JG_SETTING_RESTART
    },
    { "freeplay", "Freeplay (DIP)",
      "0 = Off, 1 = On",
      "Play for free!",
      0, 0, 1, 0
    },
    { "settingmode", "Setting Mode (DIP)",
      "0 = Off, 1 = On",
      "Activate Hardware Settings Menu at Boot",
      0, 0, 1, 0
    },
    { "showoverscan", "Show Overscan",
      "0 = Off, 1 = On",
      "Show entire video frame including normally cropped overscan area",
      0, 0, 1, 0
    }
};

enum {
    INPUT,
    SYS,
    REGION,
    FREEPLAY,
    SETTINGMODE,
    SHOWOVERSCAN
};

static const char *defs_ngsys[4] = { "Coin1", "Coin2", "Service", "Test" };

static const char *defs_mahjong[21] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
    "Pon", "Chi", "Kan", "Reach", "Ron", "Select", "Start"
};

static const char *defs_vliner[11] = {
    "Up", "Down", "Left", "Right", "Big", "Small", "D-Up", "Start",
    "Operator", "ClearCredit", "HopperOut"
};

static uint32_t dbflags = 0;

// Coin Slots, Service Button, # of Slots
static unsigned geo_input_poll_stat_a(void) {
    unsigned c = 0x07;
    if (input_device[2]->button[0]) // Coin 1
        c &= 0x06;
    if (input_device[2]->button[1]) // Coin 2
        c &= 0x05;
    if (input_device[2]->button[2]) // Service
        c &= 0x03;
    return c;
}

static unsigned geo_input_poll_stat_a_vliner(void) {
    return 0x07;
}

// P1/P2 Select/Start, Memcard status, AES/MVS setting
static unsigned geo_input_poll_stat_b(void) {
    unsigned s = 0x0f;
    if (input_device[0]->button[4])
        s &= 0x0d;
    if (input_device[0]->button[5])
        s &= 0x0e;
    if (input_device[1]->button[4])
        s &= 0x07;
    if (input_device[1]->button[5])
        s &= 0x0b;
    return s;
}

static unsigned geo_input_poll_stat_b_mahjong(void) {
    unsigned s = 0x0f;

    if (settings_geo[SYS].val == SYSTEM_AES) {
        if ((geo_m68k_reg_poutput() & 0x04))
            s &= 0x0d;
    }

    if (input_device[0]->button[19])
        s &= 0x0d;
    if (input_device[0]->button[20])
        s &= 0x0e;

    return s;
}

static unsigned geo_input_poll_stat_b_vliner(void) {
    return 0x0f;
}

// Test button, slot type
static unsigned geo_input_poll_systype(void) {
    unsigned t = 0xc0;
    if (input_device[2]->button[3]) // Test
        t &= 0x40;
    return t;
}

// Poll DIP Switches
static unsigned geo_input_poll_dipsw(void) {
    unsigned d = 0xff;
    if (settings_geo[FREEPLAY].val)
        d &= ~0x40;
    if (settings_geo[INPUT].val == 2) // Mahjong
        d &= ~0x04;
    if (settings_geo[SETTINGMODE].val)
        d &= ~0x01;
    return d;
}

// V-Liner System Buttons
static unsigned geo_input_poll_sys_vliner(void) {
    unsigned b = 0xff;

    if (input_device[2]->button[0])
        b &= ~(1 << 0);
    if (input_device[2]->button[1])
        b &= ~(1 << 1);

    if (input_device[0]->button[8])
        b &= ~(1 << 4);
    if (input_device[0]->button[9])
        b &= ~(1 << 5);
    if (input_device[0]->button[10])
        b &= ~(1 << 7);

    return b;
}

// Port Unconnected
static unsigned geo_input_poll_none(unsigned port) {
    if (port) { }
    return 0xff;
}

// Neo Geo Joystick
static unsigned geo_input_poll_js(unsigned port) {
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

// Neo Geo Mahjong Controller
static unsigned geo_input_poll_mahjong(unsigned port) {
    unsigned b = 0xff;

    if (port)
        return b;

    uint8_t output = geo_m68k_reg_poutput();

    if (output & 0x01) {
        for (unsigned i = 0; i < 7; ++i) {
            if (input_device[0]->button[i])
                b &= ~(1 << i);
        }
    }
    if (output & 0x02) {
        for (unsigned i = 0; i < 7; ++i) {
            if (input_device[0]->button[i + 7])
                b &= ~(1 << i);
        }
    }
    if (output & 0x04) {
        for (unsigned i = 0; i < 5; ++i) {
            if (input_device[0]->button[i + 14])
                b &= ~(1 << i);
        }
    }

    return b;
}

// V-Liner Play Buttons
static unsigned geo_input_poll_vliner(unsigned port) {
    unsigned b = 0xff;
    for (unsigned i = 0; i < 8; ++i) {
        if (input_device[port]->button[i])
            b &= ~(1 << i);
    }
    return b;
}

static void geo_params_video(void) {
    if (settings_geo[SHOWOVERSCAN].val) {
        vidinfo.w = LSPC_WIDTH;
        vidinfo.h = LSPC_HEIGHT;
        vidinfo.x = vidinfo.y = 0;
        vidinfo.aspect = 320.0/256.0;
    }
    else {
        vidinfo.w = LSPC_WIDTH_VISIBLE;
        vidinfo.h = LSPC_HEIGHT_VISIBLE;
        vidinfo.x = 8;
        vidinfo.y = 24;
        vidinfo.aspect = 304.0/224.0;
    }
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

int jg_init(void) {
    geo_log_set_callback(jg_cb_log);
    geo_mixer_set_rate(SAMPLERATE);
    geo_mixer_init();
    geo_set_region(settings_geo[REGION].val);
    geo_set_system(settings_geo[SYS].val);
    geo_params_video();

    // Initialize the rest of the emulator
    geo_init();

    return 1;
}

void jg_deinit(void) {
    geo_mixer_deinit();
}

void jg_reset(int hard) {
    geo_reset(hard);
}

void jg_exec_frame(void) {
    geo_exec();
}

int jg_game_load(void) {
    // Load the BIOS
    char biospath[512];
    snprintf(biospath, sizeof(biospath), "%s/%s", pathinfo.bios,
        settings_geo[SYS].val == SYSTEM_AES ? "aes.zip" : "neogeo.zip");
    if (!geo_bios_load(biospath)) {
        geo_log(JG_LOG_ERR, "Failed to load %s\n", biospath);
        return 0;
    }

    // Load the NEO file
    geo_neo_load(gameinfo.data, gameinfo.size);
    dbflags = geo_neo_flags();

    // Load saved data
    char savename[292];
    char *datatype[] = { "NVRAM", "Cartridge RAM", "Memory Card" };
    char *fext[] = { "nv", "srm", "mcr" };

    for (unsigned i = 0; i < GEO_SAVEDATA_MAX; ++i) {
        snprintf(savename, sizeof(savename),
            "%s/%s.%s", pathinfo.save, gameinfo.name, fext[i]);
        int savestat = geo_savedata_load(i, (const char*)savename);

        if (savestat == 1)
            jg_cb_log(JG_LOG_DBG, "%s Loaded: %s\n", datatype[i], savename);
        else if (savestat == 2)
            jg_cb_log(JG_LOG_DBG,
                "Invalid data type for current system/board: %s\n",
                datatype[i]);
        else
            jg_cb_log(JG_LOG_DBG, "%s Load Failed: %s\n",
                datatype[i], savename);
    }

    inputinfo[0] = jg_neogeo_inputinfo(0, JG_NEOGEO_JS);
    inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_JS);
    //inputinfo[2] = jg_neogeo_inputinfo(2, JG_NEOGEO_SYS);
    inputinfo[2] = (jg_inputinfo_t){
        JG_INPUT_EXTERNAL, 2, "neogeosystem", "Neo Geo System",
        defs_ngsys, 0, 4
    };

    geo_input_set_callback(0, &geo_input_poll_js);
    geo_input_set_callback(1, &geo_input_poll_js);
    geo_input_sys_set_callback(0, &geo_input_poll_stat_a);
    geo_input_sys_set_callback(1, &geo_input_poll_stat_b);
    geo_input_sys_set_callback(2, &geo_input_poll_systype);
    geo_input_sys_set_callback(3, &geo_input_poll_dipsw);

    // Mahjong
    if ((settings_geo[INPUT].val == 2) ||
        ((dbflags & GEO_DB_MAHJONG) && (settings_geo[INPUT].val == 0))) {

        inputinfo[0] = (jg_inputinfo_t){
            JG_INPUT_CONTROLLER, 0, "neogeomahjong",
            "Neo Geo Mahjong Controller", defs_mahjong, 0, 21
        };
        inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_UNCONNECTED);
        geo_input_set_callback(0, &geo_input_poll_mahjong);
        geo_input_set_callback(1, &geo_input_poll_none);
        geo_input_sys_set_callback(1, &geo_input_poll_stat_b_mahjong);
    }
    else if (dbflags & GEO_DB_IRRMAZE) {
        if (settings_geo[SYS].val != SYSTEM_UNI) {
            jg_cb_log(JG_LOG_WRN, "The Irritating Maze must be played using "
                "the Universe BIOS with the \"Enable Joystick Play\" option "
                "set.\n");
        }
    }
    else if (dbflags & GEO_DB_VLINER) {
        inputinfo[0] = (jg_inputinfo_t){
            JG_INPUT_CONTROLLER, 0, "neogeovliner",
            "V-Liner", defs_vliner, 0, 11
        };
        inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_UNCONNECTED);
        geo_input_set_callback(0, &geo_input_poll_vliner);
        geo_input_set_callback(1, &geo_input_poll_none);
        geo_input_sys_set_callback(0, &geo_input_poll_stat_a_vliner);
        geo_input_sys_set_callback(1, &geo_input_poll_stat_b_vliner);
        geo_input_sys_set_callback(4, &geo_input_poll_sys_vliner);
    }

    jg_cb_frametime(FRAMERATE);

    return 1;
}

int jg_game_unload(void) {
    // Save NVRAM, Cartridge RAM, and Memory Card
    char savename[292];
    char *datatype[] = { "NVRAM", "Cartridge RAM", "Memory Card" };
    char *fext[] = { "nv", "srm", "mcr" };

    for (unsigned i = 0; i < GEO_SAVEDATA_MAX; ++i) {
        snprintf(savename, sizeof(savename),
            "%s/%s.%s", pathinfo.save, gameinfo.name, fext[i]);
        int savestat = geo_savedata_save(i, (const char*)savename);

        if (savestat == 1)
            jg_cb_log(JG_LOG_DBG, "%s Saved: %s\n", datatype[i], savename);
        else if (savestat == 2)
            jg_cb_log(JG_LOG_DBG,
                "Invalid data type for current system/board: %s\n",
                datatype[i]);
        else
            jg_cb_log(JG_LOG_DBG, "%s Save Failed: %s\n",
                datatype[i], savename);
    }

    // Clean up memory allocated for BIOS ROMs
    geo_bios_unload();

    return 1;
}

int jg_state_load(const char *filename) {
    return geo_state_load(filename);
}

void jg_state_load_raw(const void *data) {
    geo_state_load_raw(data);
}

int jg_state_save(const char *filename) {
    return geo_state_save(filename);
}

const void* jg_state_save_raw(void) {
    return geo_state_save_raw();
}

size_t jg_state_size(void) {
    return 0;
}

void jg_media_select(void) {
}

void jg_media_insert(void) {
}

void jg_cheat_clear(void) {
}

void jg_cheat_set(const char *code) {
    if (code) { }
}

void jg_rehash(void) {
    geo_params_video();
}

void jg_data_push(uint32_t type, int port, const void *ptr, size_t size) {
    if (type || port || ptr || size) { }
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

jg_setting_t* jg_get_settings(size_t *numsettings) {
    *numsettings = sizeof(settings_geo) / sizeof(jg_setting_t);
    return settings_geo;
}

void jg_setup_video(void) {
    geo_lspc_set_buffer(vidinfo.buf);
}

void jg_setup_audio(void) {
    geo_mixer_set_buffer(audinfo.buf);
    geo_mixer_set_callback(jg_cb_audio);
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
