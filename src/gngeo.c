/*
 * Gngeo JG
 *
 * Copyright (c) 2001-2015 Mathieu Peponas
 * Copyright (c) 2022-2023 Rupert Carmichael
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

#include <stdint.h>
#include <stdlib.h>

#include <miniz.h>

#include "gngeo.h"
#include "gngeo_m68k.h"
#include "gngeo_mixer.h"
#include "gngeo_z80.h"

#include "ymfm/ymfm_shim.h"

#define DIV_M68K 2
#define DIV_Z80 6

#define MCYC_PER_FRAME 405504
#define MCYC_PER_LINE 1536

// Log callback
void (*gn_log)(int, const char *, ...);

// Input poll callbacks
unsigned (*gn_input_cb[NUMINPUTS_NG])(unsigned);
unsigned (*gn_input_stat_cb[NUMINPUTS_NG])(void);

// System being emulated
static int sys = 0;
static int region = 0;

// Watchdog (Anti-freezing system to reset games due to bugs/hardware faults)
static uint32_t watchdog;

static uint8_t *l0rom = NULL;
static uint8_t *biosdata = NULL;
static uint8_t *sfixdata = NULL;

neo_mem memory;

uint8_t reg_dipsw = 0;

// Timer Interrupt (IRQ2)
uint8_t irq2_ctrl = 0;
uint32_t irq2_reload = 0;
uint32_t irq2_counter = 0;
static uint32_t irq2_frags = 0;
static uint32_t irq2_dec = 0;

// NVRAM and Memory Card
uint8_t nvram[0x10000];
uint8_t memcard[0x800];

// Extra cycles stored at the end of scanlines/frames
static uint32_t mextcycs = 0;
static uint32_t lextcycs = 0;
static uint32_t zextcycs = 0;

static uint32_t ymcycs = 0;
static uint32_t ymsamps = 0;

static unsigned mcycs = 0;
static unsigned zcycs = 0;
static unsigned icycs = 0;
static unsigned lcycs = 0;

// Set the log callback
void gn_log_set_callback(void (*cb)(int, const char *, ...)) {
    gn_log = cb;
}

// Set the Input Callbacks to allow the emulator to strobe the input state
void gn_input_set_callback(unsigned port, unsigned (*cb)(unsigned)) {
    gn_input_cb[port] = cb;
}
void gn_input_stat_set_callback(unsigned port, unsigned (*cb)(void)) {
    gn_input_stat_cb[port] = cb;
}

// Set the region of the system to be emulated
void gn_set_region(int r) {
    region = r;
}

// Get the region of the system currently being emulated
int gn_get_region(void) {
    return region;
}

// Set the system to be emulated
void gn_set_system(int s) {
    sys = s;
}

// Get the system currently being emulated
int gn_get_system(void) {
    return sys;
}

int gn_bios_load(const char *biospath) {
    const char *biosrom;
    size_t biossize, sfixsize;

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    // Make sure it's actually a zip file
    if (!mz_zip_reader_init_file(&zip_archive, biospath, 0))
        return 0;

    switch (gn_get_system()) {
        default: case SYSTEM_AES: {
            biosrom = gn_get_region() == REGION_JP ?
                "neo-po.bin" : "neo-epo.bin";
            break;
        }
        case SYSTEM_MVS: {
            switch (gn_get_region()) {
                default: case REGION_US: // Winners don't use drugs
                    biosrom = "sp-u2.sp1";
                    break;
                case REGION_JP:
                    biosrom = "vs-bios.rom";
                    break;
                case REGION_AS:
                    biosrom = "sp-s3.sp1";
                    break;
                case REGION_EU:
                    biosrom = "sp-s2.sp1";
                    break;
            }
            break;
        }
        case SYSTEM_UNI: {
            biosrom = "uni-bios_4_0.rom";
            break;
        }
    }

    gn_log(GN_LOG_DBG, "Loading %s\n", biosrom);
    biosdata = mz_zip_reader_extract_file_to_heap(&zip_archive,
        biosrom, &biossize, 0);

    if (biosdata == NULL) {
        mz_zip_reader_end(&zip_archive);
        gn_log(GN_LOG_ERR, "Failed to load %s from BIOS archive!\n", biosrom);
        return 0;
    }

    // Load L0 ROM
    l0rom = mz_zip_reader_extract_file_to_heap(&zip_archive,
        "000-lo.lo", NULL, 0);
    if (l0rom == NULL) {
        mz_zip_reader_end(&zip_archive);
        gn_log(GN_LOG_ERR, "Failed to load 000-lo.lo from BIOS archive!\n");
        return 0;
    }

    if (gn_get_system() != SYSTEM_AES) {
        sfixdata = mz_zip_reader_extract_file_to_heap(&zip_archive,
            "sfix.sfix", &sfixsize, 0);
        if (sfixdata == NULL) {
            mz_zip_reader_end(&zip_archive);
            gn_log(GN_LOG_ERR, "Failed to load sfix.sfix from BIOS archive!\n");
            return 0;
        }

        // convert bios fix char
        convert_all_char(sfixdata, 0x20000, memory.fix_board_usage);
    }

    gn_m68k_romdata_bios(biosdata, biossize, sfixdata, sfixsize);

    mz_zip_reader_end(&zip_archive);
    return 1;
}

void gn_bios_unload(void) {
    if (biosdata)
        free(biosdata);
    if (sfixdata)
        free(sfixdata);
    if (l0rom)
        free(l0rom);
}

// Load Memory Card
int gn_memcard_load(const char *filename) {
    FILE *file;
    size_t filesize, result;

    // Open the file for reading
    file = fopen(filename, "rb");
    if (!file)
        return 2;

    // Find out the file's size
    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (filesize != 0x800)
        return 0;

    // Read the file into the system's NVRAM and then close it
    result = fread(nvram, sizeof(uint8_t), filesize, file);
    if (result != filesize)
        return 0;
    fclose(file);

    return 1; // Success!
}

// Save Memory Card
int gn_memcard_save(const char *filename) {
    FILE *file;
    file = fopen(filename, "wb");
    if (!file)
        return 0;

    // Write and close the file
    fwrite(nvram, 0x800, sizeof(uint8_t), file);
    fclose(file);

    return 1; // Success!
}

// Load NVRAM
int gn_nvram_load(const char *filename) {
    if (gn_get_system() == SYSTEM_AES)
        return 2;

    FILE *file;
    size_t filesize, result;

    // Open the file for reading
    file = fopen(filename, "rb");
    if (!file)
        return 2;

    // Find out the file's size
    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (filesize != 0x10000)
        return 0;

    // Read the file into the system's NVRAM and then close it
    result = fread(nvram, sizeof(uint8_t), filesize, file);
    if (result != filesize)
        return 0;
    fclose(file);

    return 1; // Success!
}

// Save NVRAM
int gn_nvram_save(const char *filename) {
    if (gn_get_system() == SYSTEM_AES)
        return 2;

    FILE *file;
    file = fopen(filename, "wb");
    if (!file)
        return 0;

    // Write and close the file
    fwrite(nvram, 0x10000, sizeof(uint8_t), file);
    fclose(file);

    return 1; // Success!
}

void gn_reset(void) {
    sound_code = 0;
    result_code = 0;
    watchdog = 0;

    gn_m68k_reset();
    gn_z80_reset();
    ymfm_shim_reset(); // Reset the YM2610 to make sure everything is defaulted
}

void gn_init(void) {
    gn_m68k_init();
    gn_z80_init();
    ymfm_shim_init();
    video_set_l0rom(l0rom);
    gn_reset();
}

// Increment the watchdog counter
static inline void gn_watchdog_increment(void) {
    /* If more than 8 frames have passed since the Watchdog was kicked, assume
       a bug or hardware fault and recover by resetting the system.
    */
    if (++watchdog > 8) {
        gn_log(GN_LOG_WRN, "Watchdog reset\n");
        gn_reset();
    }
}

// Reset the Watchdog counter
void gn_watchdog_reset(void) {
    watchdog = 0;
}

void gn_exec(void) {
    mcycs = mextcycs;
    lcycs = lextcycs;
    zcycs = zextcycs;

    current_line = 0;
    ymsamps = 0;

    draw_bg();

    // Each frame starts with the VBLANK interrupt
    gn_m68k_int(1);

    // Reload counter at the beginning of each frame
    if (irq2_ctrl & 0x40)
        irq2_counter = irq2_reload;

    while (mcycs < MCYC_PER_FRAME) {
        icycs = gn_m68k_run(1);
        mcycs += icycs * DIV_M68K;
        lcycs += icycs * DIV_M68K;

        // Handle IRQ2 counter
        irq2_dec = icycs >> 1;
        irq2_frags += icycs & 0x01;
        if (irq2_frags == 2) {
            irq2_frags = 0;
            ++irq2_dec;
        }

        for (uint32_t i = 0; i < irq2_dec; ++i) {
            if (--irq2_counter == 0) {
                /* Reload counter when it reaches 0 - if this bit is not set,
                   rely on unsigned integer underflow to prevent repeated
                   assertion of the IRQ line.
                */
                if (irq2_ctrl & 0x80)
                    irq2_counter += irq2_reload;

                // Timer Interrupt Enabled, trigger when counter reaches 0
                if (irq2_ctrl & 0x10)
                    gn_m68k_int(2);
            }
        }

        // Catch the Z80 and YM2610 up to the 68K
        while (zcycs < mcycs) {
            size_t scycs = gn_z80_run(1);
            zcycs += scycs * DIV_Z80;
            ymcycs += scycs;
            if (ymcycs >= 72) { // 72 for medium fidelity, 8 for high
                ymcycs -= 72;
                ymsamps += ymfm_shim_exec();
            }
        }

        // Handle line related logic
        if (lcycs >= MCYC_PER_LINE) {
            lcycs -= MCYC_PER_LINE;
            if (current_line < 263 && current_line > 24)
                draw_screen_scanline(current_line - 24, 0);
            else if (current_line == 263)
                draw_screen_scanline(current_line - 24, 1);
            ++current_line;
        }
    }

    mextcycs = mcycs - MCYC_PER_FRAME;
    zextcycs = zcycs - MCYC_PER_FRAME;
    lextcycs = lcycs;

    // Increment Frame Counter
    video_fc_increment();

    // Resample audio generated this frame for output to the frontend
    gn_mixer_resamp(ymsamps);

    // Increment the Watchdog counter at the end of each frame
    gn_watchdog_increment();
}
