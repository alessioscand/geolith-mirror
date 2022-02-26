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

#ifndef UNZIP_H_
#define UNZIP_H_

#include <stdio.h>
#include <stdint.h>

#include <zlib.h>

typedef struct ZFILE {
    //char *name;
    int pos;
    z_streamp zb;
    uint8_t *inbuf;
    FILE *f;
    int csize,uncsize;
    int cmeth; /* compression method */
    int readed;
}ZFILE;

typedef struct PKZIP {
    FILE *file;
    int cd_offset; /* Central dir offset */
    int cd_size;
    int cde_offset;
    uint16_t nb_item;
    uint8_t *map;
}PKZIP;

void gn_unzip_fclose(ZFILE *z);
int gn_unzip_fread(ZFILE *z,uint8_t *data,unsigned int size);
ZFILE *gn_unzip_fopen(PKZIP *zf,const char *filename,uint32_t file_crc);
PKZIP *gn_open_zip(const char *file);
uint8_t *gn_unzip_file_malloc(PKZIP *zf,const char *filename,uint32_t file_crc,unsigned int *outlen);
void gn_close_zip(PKZIP *zf);

#endif /* UNZIP_H_ */
