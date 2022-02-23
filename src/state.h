#ifndef _STATE_H_
#define _STATE_H_

#if defined(HAVE_LIBZ) && defined (HAVE_MMAP)
#include <zlib.h>
#else
#include <stdio.h>
#define gzopen fopen
#define gzread(f,data,size) fread(data,size,1,f)
#define gzwrite(f,data,size) fwrite(data,size,1,f)
#define gzclose fclose
#define gzFile FILE*
#define gzeof feof
#define gzseek fseek

#endif

#define STREAD  0
#define STWRITE 1

#define ST_VER1 1
#define ST_VER2 2
#define ST_VER3 3

extern uint8_t state_version;

int load_state(char *game,int slot);
int save_state(char *game,int slot);
uint32_t how_many_slot(char *game);
int mkstate_data(gzFile gzf,void *data,int size,int mode);

void neogeo_init_save_state(void);

#endif

