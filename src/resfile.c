/*
 * Gngeo JG
 *
 * Copyright (c) 2001-2015 Mathieu Peponas
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

#define _XOPEN_SOURCE 700

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "emu.h"
#include "roms.h"
#include "resfile.h"
#include "unzip.h"

#include "gngeo_romsets.h"

static const char *gngeo_dat;

void gn_set_datafile(const char *f) {
    gngeo_dat = f;
}

void zread_char(ZFILE *gz, char *c, int len) {
    gn_unzip_fread(gz, (uint8_t*)c, len);
}
void zread_uint8(ZFILE *gz, uint8_t *c) {
    gn_unzip_fread(gz, c, 1);
}
void zread_uint32le(ZFILE *gz, uint32_t *c) {
    gn_unzip_fread(gz, (uint8_t*)c, sizeof(uint32_t));
#ifdef WORDS_BIGENDIAN
    *c=SDL_Swap32(*c); // FIXME
#endif
}

int res_verify_datafile(const char *file) {
    struct stat sb;

    if (!file) file = gngeo_dat;

    if (lstat(file,&sb)==-1) {
        gn_log(GN_LOG_ERR, "File not found: %s\n", file);
        return 0;
    }

    if (S_ISREG(sb.st_mode)) return 1;
    gn_log(GN_LOG_ERR, "Invalid file: %s\n", file);
    return 0;
}

/*
 * Load a rom definition
 * return ROM_DEF*, NULL on error
 */
ROM_DEF *res_load_drv(char *name) {
    for (size_t i = 0; i < sizeof(romsets) / sizeof(ROM_DEF); ++i)
        if (!strcmp(name, romsets[i].name))
            return &romsets[i];
    return NULL;
}

void *res_load_data(char *name) {
    PKZIP *pz;
    uint8_t * buffer;
    unsigned int size;

    pz = gn_open_zip(gngeo_dat);
    if (!pz)
        return NULL;
    buffer = gn_unzip_file_malloc(pz, name, 0x0, &size);
    gn_close_zip(pz);
    return buffer;
}
