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

#ifndef RESFILE_H
#define RESFILE_H

#include "roms.h"

void gn_set_datafile(const char *f);
int res_verify_datafile(const char *file);
ROM_DEF *res_load_drv(char *name);
void *res_load_data(char *name);

#endif
