/*
Copyright (c) 2022-2026 Rupert Carmichael
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
#if JG_VERSION_NUMBER > 10000
#include <jg/jg_neogeo.h>
#else
#include "jg_neogeo_geo.h"
jg_inputinfo_t* jg_get_inputlist(size_t *num);
jg_setting_t* jg_get_dips(size_t *num);
#endif

#include "geo.h"
#include "geo_cd.h"
#include "geo_disc.h"
#include "geo_lspc.h"
#include "geo_m68k.h"
#include "geo_mixer.h"
#include "geo_neo.h"

#include "version.h"

#define SAMPLERATE 44100
#define SAMPLERATE_ADJUSTED 44396 // Rate corrected for 60Hz vs ~59.19/59.60Hz
#define FRAMERATE 60
#define CHANNELS 2
#define NUMINPUTS 5
#define TOTALINPUTS 8

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
static jg_fileinfo_t biosinfo;
static jg_inputinfo_t inputinfo[NUMINPUTS];
static jg_inputinfo_t inputlist[TOTALINPUTS];
static jg_inputstate_t *input_device[NUMINPUTS];

// Emulator settings
static jg_setting_t settings_geo[] = {
    { "input", "Input Devices",
      "0 = Auto, 1 = Neo Geo Joysticks, 2 = Mahjong, "
      "3 = 4-Player (AS/JP MVS)",
      "Select the desired input device(s)",
      0, 0, 3, JG_SETTING_INPUT
    },
    { "system", "Cartridge System",
      "0 = AES (Console), 1 = MVS (Arcade), 2 = Universe BIOS",
      "Select which type of system/BIOS to emulate",
      SYSTEM_AES, SYSTEM_AES, SYSTEM_UNI, JG_SETTING_RESTART
    },
    { "cdsystem", "CD System",
      "0 = Neo Geo CD (Front Loader), 1 = Neo Geo CD (Top Loader), "
      "2 = Neo Geo CDZ, 3 = Universe BIOS",
      "Select which type of system/BIOS to emulate",
      2, 0, 3, JG_SETTING_RESTART
    },
    { "unihw", "Universe BIOS Hardware",
      "0 = AES (Console), 1 = MVS (Arcade)",
      "Select which type of emulated hardware the Universe BIOS should detect",
      SYSTEM_MVS, SYSTEM_AES, SYSTEM_MVS, JG_SETTING_RESTART
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
    { "memcard_inserted", "Memory Card",
      "0 = Disabled, 1 = Enabled",
      "Enable or Disable the Memory Card",
      1, 0, 1, 0
    },
    { "memcard_wp", "Memory Card Write Protect",
      "0 = Disabled, 1 = Enabled",
      "Enable or Disable the Memory Card Write Protect pin",
      0, 0, 1, 0
    },
    { "palette", "Palette",
      "0 = Resistor Network, 1 = Raw",
      "Select the palette to use",
      0, 0, 1, 0
    },
    { "aspect", "Aspect Ratio",
      "0 = 1:1 PAR, 1 = 45:44 PAR, 2 = 4:3 DAR",
      "Set the aspect ratio",
      0, 0, 2, 0
    },
    { "overscan_t", "Overscan Mask (Top)",
      "0 = 0, 1 = 4, 2 = 8, 3 = 12, 4 = 16",
      "Hide N pixels of Overscan (Top)",
      2, 0, 4, 0
    },
    { "overscan_b", "Overscan Mask (Bottom)",
      "0 = 0, 1 = 4, 2 = 8, 3 = 12, 4 = 16",
      "Hide N pixels of Overscan (Bottom)",
      2, 0, 4, 0
    },
    { "overscan_l", "Overscan Mask (Left)",
      "0 = 0, 1 = 4, 2 = 8, 3 = 12, 4 = 16",
      "Hide N pixels of Overscan (Left)",
      2, 0, 4, 0
    },
    { "overscan_r", "Overscan Mask (Right)",
      "0 = 0, 1 = 4, 2 = 8, 3 = 12, 4 = 16",
      "Hide N pixels of Overscan (Right)",
      2, 0, 4, 0
    },
    { "adpcm_wrap", "ADPCM Accumulator Wrap",
      "0 = Off, 1 = On",
      "ADPCM Accumulator Wrap may be disabled to fix sound effects in buggy "
      "games, for example Ganryu and Nightmare in the Dark. This is a hack!",
      1, 0, 1, 0
    }
};

enum {
    INPUT,
    SYS,
    CDSYS,
    UNIHW,
    REGION,
    FREEPLAY,
    SETTINGMODE,
    MEMCARD_INSERTED,
    MEMCARD_WP,
    PALETTE,
    ASPECT,
    OVERSCAN_T,
    OVERSCAN_B,
    OVERSCAN_L,
    OVERSCAN_R,
    ADPCM_WRAP
};

// Database flags for automatically setting inputs
static uint32_t dbflags = 0;

// DIP Switches (Unconfigurable)
static unsigned dipswitches = 0;

// Values for coin slots 3 and 4 in REG_STATUS_A reads
static unsigned coins34 = 0x00;

// Neo Geo CD/CDZ mode
static unsigned cdmode = 0;

// Trackball relative values (Irritating Maze)
static int32_t trackball_x = 0;
static int32_t trackball_y = 0;

// Keep track of whether the input list has been built or not
static int inputlist_built = 0;

// Coin Slots, Service Button, # of Slots
static unsigned geo_input_poll_stat_a(void) {
    unsigned c = 0x07;

    if (input_device[4]->button[0]) // Coin 1
        c &= 0x06;
    if (input_device[4]->button[1]) // Coin 2
        c &= 0x05;
    if (input_device[4]->button[2]) // Service
        c &= 0x03;

    c |= coins34;

    return c;
}

static unsigned geo_input_poll_stat_a_vliner(void) {
    return 0x07;
}

// P1/P2 Select/Start, Memcard status, AES/MVS setting
static unsigned geo_input_poll_stat_b(void) {
    unsigned s = 0x0f;
    if (settings_geo[MEMCARD_INSERTED].val) {
        if (settings_geo[MEMCARD_WP].val) s |= 0x40;
    }
    else {
        s |= 0x30;
    }

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

// P1/P2 Start, P3/P4 Start, Memcard status, AES/MVS setting
static unsigned geo_input_poll_stat_b_ftc1b(void) {
    unsigned s = 0x0f;
    if (settings_geo[MEMCARD_INSERTED].val) {
        if (settings_geo[MEMCARD_WP].val) s |= 0x40;
    }
    else {
        s |= 0x30;
    }

    if (geo_m68k_reg_poutput() & 0x01) { // Players 2/4 (P1-B, P2-B)
        if (input_device[1]->button[5])
            s &= 0x0e;
        if (input_device[3]->button[5])
            s &= 0x0b;
    }
    else { // Players 1/3 (P1-A, P2-A)
        if (input_device[0]->button[5])
            s &= 0x0e;
        if (input_device[2]->button[5])
            s &= 0x0b;
    }
    return s;
}

static unsigned geo_input_poll_stat_b_mahjong(void) {
    unsigned s = 0x0f;
    if (settings_geo[MEMCARD_INSERTED].val) {
        if (settings_geo[MEMCARD_WP].val) s |= 0x40;
    }
    else {
        s |= 0x30;
    }

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
    unsigned s = 0x0f;
    if (settings_geo[MEMCARD_INSERTED].val) {
        if (settings_geo[MEMCARD_WP].val) s |= 0x40;
    }
    else {
        s |= 0x30;
    }
    return s;
}

// Test button, slot type
static unsigned geo_input_poll_systype(void) {
    unsigned t = 0xc0;
    if (input_device[4]->button[3]) // Test
        t &= 0x40;
    return t;
}

// Poll DIP Switches
static unsigned geo_input_poll_dipsw(void) {
    unsigned d = 0xff;

    // Set runtime-configurable DIP Switches
    if (settings_geo[FREEPLAY].val)
        d &= ~0x40;
    if (settings_geo[SETTINGMODE].val)
        d &= ~0x01;

    return d & ~dipswitches;
}

// V-Liner System Buttons
static unsigned geo_input_poll_sys_vliner(void) {
    unsigned b = 0xff;

    if (input_device[4]->button[0])
        b &= ~(1 << 0);
    if (input_device[4]->button[1])
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

// Neo Geo Joystick (NEO-FTC1B 4-Player Extension Board for 2 MVS cabinets)
static unsigned geo_input_poll_js_ftc1b(unsigned port) {
    unsigned b = 0xff;

    /* Output pin 1 high means use the second player controls on the cabinet.
       The port number passed into the callback in this case specifies the
       cabinet, not the player number, so adjust it to align with the actual
       port to be read from the emulator's perspective.
    */
    uint8_t output = geo_m68k_reg_poutput();
    port = (port << 1) + (output & 0x01);

    /* When output pin 3 is high, it simulates the A button for the first
       player controls on the cabinet, and the B button for the second player
       controls on the cabinet.
    */
    if (output & 0x04)
        b &= ~(1 << (4 | (output & 0x01)));

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

// The Irritating Maze
static unsigned geo_input_poll_irrmaze(unsigned port) {
    unsigned b = 0xff;

    if (port == 0) {
        uint8_t output = geo_m68k_reg_poutput();
        if (output & 0x01) {
            trackball_y -= input_device[0]->axis[1];
            b = (uint8_t)(trackball_y / 8192);
        }
        else {
            trackball_x -= input_device[0]->axis[0];;
            b = (uint8_t)(trackball_x / 8192);
        }

        if (output & 0x20)
            jg_cb_log(JG_LOG_SCR, "Get Blown by Air Jet");
    }
    else {
        if (input_device[0]->button[0]) b &= ~0x10;
        if (input_device[0]->button[1]) b &= ~0x20;
        if (input_device[0]->button[2]) b &= ~0x40;
        if (input_device[0]->button[3]) b &= ~0x80;
    }

    return b;
}

static unsigned geo_input_poll_stat_b_irrmaze(void) {
    unsigned s = 0x0f;
    if (settings_geo[MEMCARD_INSERTED].val) {
        if (settings_geo[MEMCARD_WP].val) s |= 0x40;
    }
    else {
        s |= 0x30;
    }

    if (input_device[0]->button[4])
        s &= ~0x01;
    return s;
}

static void geo_params_input(void) {
    inputinfo[0] = jg_neogeo_inputinfo(0, JG_NEOGEO_JS);
    inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_JS);
    inputinfo[2] = jg_neogeo_inputinfo(2, JG_NEOGEO_UNCONNECTED);
    inputinfo[3] = jg_neogeo_inputinfo(3, JG_NEOGEO_UNCONNECTED);
    inputinfo[4] = jg_neogeo_inputinfo(4, JG_NEOGEO_SYS);

    geo_input_set_callback(0, &geo_input_poll_js);
    geo_input_set_callback(1, &geo_input_poll_js);
    geo_input_sys_set_callback(0, &geo_input_poll_stat_a);
    geo_input_sys_set_callback(1, &geo_input_poll_stat_b);
    geo_input_sys_set_callback(2, &geo_input_poll_systype);
    geo_input_sys_set_callback(3, &geo_input_poll_dipsw);

    // Mahjong
    if ((settings_geo[INPUT].val == 2) ||
        ((dbflags & GEO_DB_MAHJONG) && (settings_geo[INPUT].val == 0))) {
        dipswitches |= 0x04; // Set Mahjong mode
        inputinfo[0] = jg_neogeo_inputinfo(0, JG_NEOGEO_MAHJONG);
        inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_UNCONNECTED);
        geo_input_set_callback(0, &geo_input_poll_mahjong);
        geo_input_set_callback(1, &geo_input_poll_none);
        geo_input_sys_set_callback(1, &geo_input_poll_stat_b_mahjong);
    }
    else if (dbflags & GEO_DB_IRRMAZE) {
        if (settings_geo[SYS].val == SYSTEM_UNI) {
            jg_cb_log(JG_LOG_WRN, "The Irritating Maze must be played with "
                "the Universe BIOS \"Enable Joystick Play\" cheat set.\n");
        }
        else if (settings_geo[SYS].val == SYSTEM_MVS) {
            inputinfo[0] = jg_neogeo_inputinfo(0, JG_NEOGEO_IRRMAZE);
            inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_UNCONNECTED);
            geo_input_set_callback(0, &geo_input_poll_irrmaze);
            geo_input_set_callback(1, &geo_input_poll_irrmaze);
            geo_input_sys_set_callback(1, &geo_input_poll_stat_b_irrmaze);
        }
    }
    else if (dbflags & GEO_DB_VLINER) {
        inputinfo[0] = jg_neogeo_inputinfo(0, JG_NEOGEO_VLINER);
        inputinfo[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_UNCONNECTED);
        geo_input_set_callback(0, &geo_input_poll_vliner);
        geo_input_set_callback(1, &geo_input_poll_none);
        geo_input_sys_set_callback(0, &geo_input_poll_stat_a_vliner);
        geo_input_sys_set_callback(1, &geo_input_poll_stat_b_vliner);
        geo_input_sys_set_callback(4, &geo_input_poll_sys_vliner);
    }
    else if (settings_geo[INPUT].val == 3 &&
        settings_geo[SYS].val == SYSTEM_MVS && (settings_geo[REGION].val ==
        REGION_AS || settings_geo[REGION].val == REGION_JP)) {
        // Set Four Player mode
        dipswitches |= 0x02;

        inputinfo[2] = jg_neogeo_inputinfo(2, JG_NEOGEO_JS);
        inputinfo[3] = jg_neogeo_inputinfo(3, JG_NEOGEO_JS);
        geo_input_set_callback(0, &geo_input_poll_js_ftc1b);
        geo_input_set_callback(1, &geo_input_poll_js_ftc1b);
        geo_input_sys_set_callback(1, &geo_input_poll_stat_b_ftc1b);
    }
}

static void geo_params_video(void) {
    unsigned w = LSPC_WIDTH - ((settings_geo[OVERSCAN_L].val << 2) +
        (settings_geo[OVERSCAN_R].val << 2));
    unsigned h = LSPC_HEIGHT - ((settings_geo[OVERSCAN_T].val << 2) +
        (settings_geo[OVERSCAN_B].val << 2) + 16);

    vidinfo.w = w;
    vidinfo.h = h;
    vidinfo.x = settings_geo[OVERSCAN_L].val << 2;
    vidinfo.y = (settings_geo[OVERSCAN_T].val << 2) + 16;

    switch (settings_geo[ASPECT].val) {
        case 0: vidinfo.aspect = w / (double)h; break; // 1:1
        case 1: vidinfo.aspect = (w * (45.0 / 44.0)) / (double)h; break;
        case 2: vidinfo.aspect = 4.0 / 3.0; break;
    }

    geo_lspc_set_palette(settings_geo[PALETTE].val);
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
    geo_set_region(settings_geo[REGION].val);

    if (cdmode)
        geo_set_system(SYSTEM_CDF + settings_geo[CDSYS].val);
    else
        geo_set_system(settings_geo[SYS].val);

    geo_set_adpcm_wrap(settings_geo[ADPCM_WRAP].val);
    geo_mixer_set_rate(SAMPLERATE_ADJUSTED);
    geo_mixer_init();
    geo_params_video();

    // Initialize the rest of the emulator
    geo_init();
    geo_set_region(settings_geo[REGION].val);

    return 1;
}

void jg_deinit(void) {
    geo_mixer_deinit();
    geo_deinit();
}

void jg_reset(int hard) {
    geo_reset(hard);
}

void jg_exec_frame(void) {
    geo_exec();
}

int jg_game_load(void) {
    // Load the BIOS if necessary
    if (biosinfo.size) {
        if (!geo_bios_load_mem(biosinfo.data, biosinfo.size))
            geo_log(JG_LOG_ERR, "Failed to load %s\n", biosinfo.path);
    }
    else {
        char biospath[512];
        char *bfile;

        if (cdmode) {
            if (settings_geo[CDSYS].val < 2) // Neo Geo CD
                bfile = "neocd.zip";
            else // Neo Geo CDZ or Universe BIOS
                bfile = "neocdz.zip";
        }
        else if (settings_geo[SYS].val == SYSTEM_AES)
            bfile = "aes.zip";
        else
            bfile = "neogeo.zip";

        snprintf(biospath, sizeof(biospath), "%s/%s", pathinfo.bios, bfile);

        if (!geo_bios_load_file(biospath))
            geo_log(JG_LOG_ERR, "Failed to load %s\n", biospath);

        if (settings_geo[CDSYS].val < 2) {
            // Neo Geo CD needs to load "000-lo.lo" from neocdz.zip
            snprintf(biospath, sizeof(biospath),
                "%s/neocdz.zip", pathinfo.bios);
            if (!geo_bios_load_file_aux(biospath))
                geo_log(JG_LOG_ERR, "Failed to load %s\n", biospath);
        }
    }

    /* Set coin slot 3 and 4 bits for REG_STATUS_A reads depending on hardware.
       Geolith never uses slots 3 and 4 when emulating MVS hardware, so these
       bits need to be set high (they are active low).
    */
    if ((settings_geo[SYS].val == SYSTEM_MVS) ||
        (settings_geo[SYS].val == SYSTEM_UNI &&
         settings_geo[UNIHW].val == SYSTEM_MVS)) {
        coins34 = 0x18;
    }

    if (cdmode) {
        // Load the CD
        geo_disc_open(gameinfo.path);
        geo_cd_postload();
    }
    else {
        // Load the NEO file
        geo_neo_load(gameinfo.data, gameinfo.size);
        dbflags = geo_neo_flags();

        // Special handling for Irritating Maze
        if (dbflags & GEO_DB_IRRMAZE && settings_geo[SYS].val == SYSTEM_MVS) {
            char biospath[512];
            snprintf(biospath, sizeof(biospath),
                "%s/irrmaze.zip", pathinfo.bios);
            if (!geo_bios_load_file_aux(biospath))
                geo_log(JG_LOG_ERR, "Failed to load %s\n", biospath);
        }
    }

    // Load saved data
    char savename[292];
    char *datatype[] = { "NVRAM", "Cartridge RAM", "Memory Card",
            "CD Backup RAM" };
    char *fext[] = { "nv", "srm", "mcr", "brm" };

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

    geo_params_input();

    jg_cb_frametime(FRAMERATE);

    return 1;
}

int jg_game_unload(void) {
    // Save NVRAM, Cartridge RAM, and Memory Card
    char savename[292];
    char *datatype[] = { "NVRAM", "Cartridge RAM", "Memory Card",
            "CD Backup RAM" };
    char *fext[] = { "nv", "srm", "mcr", "brm" };

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

    // Clean up CD related components
    if (cdmode) {
        geo_disc_close();
        geo_cd_deinit();
    }

    cdmode = 0;
    coins34 = 0;
    dbflags = 0;
    dipswitches = 0;

    return 1;
}

int jg_state_load(const char *filename) {
    trackball_x = 0;
    trackball_y = 0;
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
    return geo_state_size();
}

void jg_media_select(void) {
}

void jg_media_insert(void) {
}

void jg_cheat_clear(void) {
}

void jg_cheat_set(const char *code) {
    (void)code;
}

void jg_rehash(void) {
    geo_params_input();
    geo_params_video();
    geo_set_adpcm_wrap(settings_geo[ADPCM_WRAP].val);
}

void jg_data_push(uint32_t type, int port, const void *ptr, size_t size) {
    (void)type; (void)port; (void)ptr; (void)size;
}

jg_coreinfo_t* jg_get_coreinfo(const char *sys) {
    if (!strcmp(sys, "neogeocd"))
        cdmode = 1;
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

jg_inputinfo_t* jg_get_inputlist(size_t *num) {
    if (!inputlist_built) {
        inputlist[0] = jg_neogeo_inputinfo(0, JG_NEOGEO_JS);
        inputlist[1] = jg_neogeo_inputinfo(1, JG_NEOGEO_JS);
        inputlist[2] = jg_neogeo_inputinfo(2, JG_NEOGEO_JS);
        inputlist[3] = jg_neogeo_inputinfo(3, JG_NEOGEO_JS);
        inputlist[4] = jg_neogeo_inputinfo(0, JG_NEOGEO_SYS);
        inputlist[5] = jg_neogeo_inputinfo(0, JG_NEOGEO_MAHJONG);
        inputlist[6] = jg_neogeo_inputinfo(0, JG_NEOGEO_VLINER);
        inputlist[7] = jg_neogeo_inputinfo(0, JG_NEOGEO_IRRMAZE);
        inputlist_built = 1;
    }
    *num = TOTALINPUTS;
    return inputlist;
}

jg_setting_t* jg_get_settings(size_t *numsettings) {
    *numsettings = sizeof(settings_geo) / sizeof(jg_setting_t);
    return settings_geo;
}

jg_setting_t* jg_get_dips(size_t *num) {
    /* Neo Geo MVS has DIP switches, but they are currently normal settings.
       This way they are global instead of per-game.
    */
    *num = 0;
    return NULL;
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
    if (index) // Only one auxiliary file supported in this emulator
        return;
    biosinfo = info;
}

void jg_set_paths(jg_pathinfo_t paths) {
    pathinfo = paths;
}
