#ifndef RESFILE_H
#define RESFILE_H

#include "roms.h"

void gn_set_datafile(const char *f);
int res_verify_datafile(const char *file);
ROM_DEF *res_load_drv(char *name);
void *res_load_data(char *name);

#endif
