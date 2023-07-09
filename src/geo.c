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

#include <stdint.h>
#include <stdlib.h>

#include <miniz.h>

#include "geo.h"
#include "geo_lspc.h"
#include "geo_m68k.h"
#include "geo_memcard.h"
#include "geo_mixer.h"
#include "geo_rtc.h"
#include "geo_z80.h"

#include "ymfm/ymfm_shim.h"

#define DIV_M68K 2
#define DIV_Z80 6
#define DIV_YM2610 72 // 72 for medium fidelity, 8 for high

#define MCYC_PER_LINE 1536
#define MCYC_PER_FRAME (MCYC_PER_LINE * 264) // 405504
#define MCYC_HBLANK 1424

// Log callback
void (*geo_log)(int, const char *, ...);

// Input poll callbacks
unsigned (*geo_input_cb[NUMINPUTS_NG])(unsigned);
unsigned (*geo_input_sys_cb[NUMINPUTS_SYS])(void);

// Neo Geo System
ngsys_t ngsys;

// System being emulated
static int sys = 0;
static int region = 0;

static uint8_t *l0rom = NULL;
static uint8_t *biosdata = NULL;
static uint8_t *sfixdata = NULL;
static uint8_t *smrom = NULL;

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

// Scanline drawing
static unsigned sldrawn = 0;
static unsigned slcyc = MCYC_HBLANK;

// Set the log callback
void geo_log_set_callback(void (*cb)(int, const char *, ...)) {
    geo_log = cb;
}

// Set the Input Callbacks to allow the emulator to strobe the input state
void geo_input_set_callback(unsigned port, unsigned (*cb)(unsigned)) {
    geo_input_cb[port] = cb;
}

// Set the Status A/B and System Type Register Callbacks
void geo_input_sys_set_callback(unsigned port, unsigned (*cb)(void)) {
    geo_input_sys_cb[port] = cb;
}

// Set the region of the system to be emulated
void geo_set_region(int r) {
    region = r;
}

// Get the region of the system currently being emulated
int geo_get_region(void) {
    return region;
}

// Set the system to be emulated
void geo_set_system(int s) {
    sys = s;
}

// Get the system currently being emulated
int geo_get_system(void) {
    return sys;
}

// Set the master cycle in the scanline after which the data is drawn
void geo_set_drawcyc(unsigned cyc) {
    slcyc = cyc;
}

// Calculate a mask for an arbitrarily sized memory block
uint32_t geo_calc_mask(unsigned nbits, unsigned val) {
    /* Masks over blocks of memory with a size which is not a power of two need
       every bit below the most significant set bit to also be set. Loop
       through the value from left to right to find the first bit set, then
       subtract 1 from the next highest power of two to get a mask which covers
       all possible values in the space being addressed.

       Example: If there are 5 addressable banks, banks 0-4 must be
                addressable. In this case, the mask must be 0b0111 or 0x7 for
                all banks to be covered when a value is masked with AND.
    */
    if (val)
        val -= 1;

    for (unsigned i = 1 << (nbits - 1); i != 0; i >>= 1) {
        if (i & val)
            return (i << 1) - 1;
    }
    return 0;
}

int geo_bios_load(const char *biospath) {
    const char *biosrom;
    size_t biossize = 0, sfixsize = 0, smsize = 0;

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    // Make sure it's actually a zip file
    if (!mz_zip_reader_init_file(&zip_archive, biospath, 0))
        return 0;

    switch (geo_get_system()) {
        default: case SYSTEM_AES: {
            biosrom = geo_get_region() == REGION_JP ?
                "neo-po.bin" : "neo-epo.bin";
            break;
        }
        case SYSTEM_MVS: {
            switch (geo_get_region()) {
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

    geo_log(GEO_LOG_DBG, "Loading %s\n", biosrom);
    biosdata = mz_zip_reader_extract_file_to_heap(&zip_archive,
        biosrom, &biossize, 0);

    if (biosdata == NULL) {
        mz_zip_reader_end(&zip_archive);
        geo_log(GEO_LOG_ERR,
            "Failed to load %s from BIOS archive!\n", biosrom);
        return 0;
    }

    // Load L0 ROM
    l0rom = mz_zip_reader_extract_file_to_heap(&zip_archive,
        "000-lo.lo", NULL, 0);
    if (l0rom == NULL) {
        mz_zip_reader_end(&zip_archive);
        geo_log(GEO_LOG_ERR, "Failed to load 000-lo.lo from BIOS archive!\n");
        return 0;
    }
    geo_lspc_set_l0rom(l0rom);

    if (geo_get_system() != SYSTEM_AES) {
        // SFIX
        sfixdata = mz_zip_reader_extract_file_to_heap(&zip_archive,
            "sfix.sfix", &sfixsize, 0);
        if (sfixdata == NULL) {
            mz_zip_reader_end(&zip_archive);
            geo_log(GEO_LOG_ERR,
                "Failed to load sfix.sfix from BIOS archive!\n");
            return 0;
        }
        geo_lspc_sfixdata(sfixdata, sfixsize);

        // SM1
        smrom = mz_zip_reader_extract_file_to_heap(&zip_archive,
            "sm1.sm1", &smsize, 0);
        if (smrom == NULL) {
            mz_zip_reader_end(&zip_archive);
            geo_log(GEO_LOG_ERR,
                "Failed to load sm1.sm1 from BIOS archive!\n");
            return 0;
        }
        geo_z80_smdata(smrom, smsize);
    }

    geo_m68k_biosdata(biosdata, biossize);

    mz_zip_reader_end(&zip_archive);
    return 1;
}

void geo_bios_unload(void) {
    if (biosdata)
        free(biosdata);
    if (l0rom)
        free(l0rom);
    if (sfixdata)
        free(sfixdata);
    if (smrom)
        free(smrom);
}

// Load NVRAM, Cartridge RAM, or Memory Card data
int geo_savedata_load(unsigned datatype, const char *filename) {
    uint8_t *dataptr = NULL;
    size_t datasize = 0;

    switch (datatype) {
        case GEO_SAVEDATA_NVRAM: {
            if (geo_get_system() == SYSTEM_AES)
                return 2;
            dataptr = ngsys.nvram;
            datasize = SIZE_64K;
            break;
        }
        case GEO_SAVEDATA_CARTRAM: {
            dataptr = ngsys.cartram;
            datasize = SIZE_8K;
            break;
        }
        case GEO_SAVEDATA_MEMCARD: {
            dataptr = ngsys.memcard;
            datasize = SIZE_2K;
            break;
        }
        default: return 2;
    }

    FILE *file;
    size_t filesize, result;

    // Open the file for reading
    file = fopen(filename, "rb");
    if (!file)
        return 0;

    // Find out the file's size
    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (filesize != datasize)
        return 0;

    // Read the file into the memory card and then close it
    result = fread(dataptr, sizeof(uint8_t), filesize, file);
    if (result != filesize)
        return 0;
    fclose(file);

    return 1; // Success!
}

// Save NVRAM, Cartridge RAM, or Memory Card data
int geo_savedata_save(unsigned datatype, const char *filename) {
    uint8_t *dataptr = NULL;
    size_t datasize = 0;

    switch (datatype) {
        case GEO_SAVEDATA_NVRAM: {
            if (geo_get_system() == SYSTEM_AES)
                return 2;
            dataptr = ngsys.nvram;
            datasize = SIZE_64K;
            break;
        }
        case GEO_SAVEDATA_CARTRAM: {
            if (!ngsys.cartram_present)
                return 2;
            dataptr = ngsys.cartram;
            datasize = SIZE_8K;
            break;
        }
        case GEO_SAVEDATA_MEMCARD: {
            dataptr = ngsys.memcard;
            datasize = SIZE_2K;
            break;
        }
        default: return 2;
    }

    FILE *file;
    file = fopen(filename, "wb");
    if (!file)
        return 0;

    // Write and close the file
    fwrite(dataptr, datasize, sizeof(uint8_t), file);
    fclose(file);

    return 1; // Success!
}

void geo_reset(void) {
    ngsys.sound_code = 0;
    ngsys.sound_reply = 0;
    ngsys.watchdog = 0;

    geo_m68k_reset();
    geo_z80_reset();
    ymfm_shim_reset(); // Reset the YM2610 to make sure everything is defaulted
    geo_lspc_init();
}

void geo_init(void) {
    geo_m68k_init();
    geo_z80_init();
    ymfm_shim_init();
    geo_rtc_init();
    geo_lspc_init();
    geo_reset();

    ngsys.irq2_ctrl = 0;
    ngsys.irq2_reload = 0;
    ngsys.irq2_counter = 0;
    ngsys.irq2_frags = 0;
    ngsys.irq2_dec = 0;

    memset(ngsys.nvram, 0x00, SIZE_64K);
    memset(ngsys.cartram, 0x00, SIZE_8K);
    memset(ngsys.extram, 0x00, SIZE_128K);

    // Pre-format the memory card in case there is no existing memory card data
    geo_memcard_format(ngsys.memcard);
}

// Increment the watchdog counter
static inline void geo_watchdog_increment(void) {
    /* If more than 8 frames have passed since the Watchdog was kicked, assume
       a bug or hardware fault and recover by resetting the system.
    */
    if (++ngsys.watchdog > 8) {
        geo_log(GEO_LOG_WRN, "Watchdog reset\n");
        geo_reset();
    }
}

// Reset the Watchdog counter
void geo_watchdog_reset(void) {
    ngsys.watchdog = 0;
}

void geo_exec(void) {
    mcycs = mextcycs;
    lcycs = lextcycs;
    zcycs = zextcycs;
    ymsamps = 0;

    // Each frame starts with the VBLANK interrupt
    geo_m68k_interrupt(IRQ_VBLANK);

    // Reload counter at the beginning of each frame
    if (ngsys.irq2_ctrl & IRQ_TIMER_RELOAD_VBLANK)
        ngsys.irq2_counter = ngsys.irq2_reload;

    while (mcycs < MCYC_PER_FRAME) {
        icycs = geo_m68k_run(1);
        mcycs += icycs * DIV_M68K;
        lcycs += icycs * DIV_M68K;

        // If this is an arcade system, update the RTC
        if (sys)
            geo_rtc_sync(icycs);

        // Handle IRQ2 counter
        ngsys.irq2_dec = icycs >> 1;
        ngsys.irq2_frags += icycs & 0x01;
        if (ngsys.irq2_frags == 2) {
            ngsys.irq2_frags = 0;
            ++ngsys.irq2_dec;
        }

        for (uint32_t i = 0; i < ngsys.irq2_dec; ++i) {
            if (--ngsys.irq2_counter == 0) {
                /* Reload counter when it reaches 0 - if this bit is not set,
                   rely on unsigned integer underflow to prevent repeated
                   assertion of the IRQ line.
                */
                if (ngsys.irq2_ctrl & IRQ_TIMER_RELOAD_COUNT0)
                    ngsys.irq2_counter += ngsys.irq2_reload;

                // Timer Interrupt Enabled
                if (ngsys.irq2_ctrl & IRQ_TIMER_ENABLED)
                    geo_m68k_interrupt(IRQ_TIMER);
            }
        }

        // Catch the Z80 and YM2610 up to the 68K
        while (zcycs < mcycs) {
            size_t scycs = geo_z80_run(1);
            zcycs += scycs * DIV_Z80;
            ymcycs += scycs;
            if (ymcycs >= DIV_YM2610) {
                ymcycs -= DIV_YM2610;
                ymsamps += ymfm_shim_exec();
            }
        }

        if (!sldrawn && (lcycs >= slcyc)) { // Handle drawing of scanline data
            geo_lspc_scanline();
            sldrawn = 1;
        }
        else if (lcycs >= MCYC_PER_LINE) { // Handle end of line
            lcycs -= MCYC_PER_LINE;
            sldrawn = 0;
        }
    }

    mextcycs = mcycs - MCYC_PER_FRAME;
    zextcycs = zcycs - MCYC_PER_FRAME;
    lextcycs = lcycs;

    // Resample audio generated this frame for output to the frontend
    geo_mixer_resamp(ymsamps);

    // Increment the Watchdog counter at the end of each frame
    geo_watchdog_increment();
}
