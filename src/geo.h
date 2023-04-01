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

/* Every genius makes it more complicated. It takes a super-genius to make it
   simpler.
            - Terry A. Davis
*/

#ifndef GEOLITH_H
#define GEOLITH_H

#define VERSION "0.1.0"

#define SIZE_1K     0x000400
#define SIZE_2K     0x000800
#define SIZE_4K     0x001000
#define SIZE_8K     0x002000
#define SIZE_16K    0x004000
#define SIZE_32K    0x008000
#define SIZE_64K    0x010000
#define SIZE_128K   0x020000

#define SYSTEM_AES  0x00 // Console
#define SYSTEM_MVS  0x01 // Arcade
#define SYSTEM_UNI  0x02 // Universe BIOS

#define REGION_US   0x00 // USA
#define REGION_JP   0x01 // Japan
#define REGION_AS   0x02 // Asia
#define REGION_EU   0x03 // Europe

#define NUMINPUTS_NG    2
#define NUMINPUTS_SYS   4

enum geo_loglevel {
    GEO_LOG_DBG,
    GEO_LOG_INF,
    GEO_LOG_WRN,
    GEO_LOG_ERR,
    GEO_LOG_SCR
};

int geo_bios_load(const char*);
void geo_bios_unload(void);

int geo_memcard_load(const char*);
int geo_memcard_save(const char*);

int geo_nvram_load(const char*);
int geo_nvram_save(const char*);

void geo_exec(void);
void geo_init(void);
void geo_reset(void);

void geo_log_set_callback(void (*)(int, const char *, ...));
void geo_input_set_callback(unsigned, unsigned (*)(unsigned));
void geo_input_sys_set_callback(unsigned, unsigned (*)(void));

int geo_get_region(void);
void geo_set_region(int);

int geo_get_system(void);
void geo_set_system(int);

extern void (*geo_log)(int, const char *, ...);

extern unsigned (*geo_input_cb[NUMINPUTS_NG])(unsigned);
extern unsigned (*geo_input_sys_cb[NUMINPUTS_SYS])(void);

void geo_watchdog_reset(void);

// Timer Interrupt (IRQ2)
extern uint8_t irq2_ctrl;
extern uint32_t irq2_reload;
extern uint32_t irq2_counter;

extern uint8_t nvram[SIZE_64K];
extern uint8_t memcard[SIZE_2K];

// 68K/Z80 Communication
extern uint8_t sound_code;
extern uint8_t result_code;

#endif
