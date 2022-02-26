#define _XOPEN_SOURCE 700

#include <stdlib.h>
#include <sys/stat.h>

#include "emu.h"
#include "roms.h"
#include "resfile.h"
#include "unzip.h"

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
 * Load a rom definition file from gngeo.dat (rom/name.drv)
 * return ROM_DEF*, NULL on error
 */
ROM_DEF *res_load_drv(char *name) {
	ROM_DEF *drv;
	char drvfname[32];
	PKZIP *pz;
	ZFILE *z;

	drv = calloc(sizeof(ROM_DEF), 1);

	/* Open the rom driver def */
	pz = gn_open_zip(gngeo_dat);
	if (pz == NULL) {
		free(drv);
		//fprintf(stderr, "Can't open the %s\n", gngeo_dat);
		return NULL;
	}
	sprintf(drvfname, "rom/%s.drv", name);

	if ((z=gn_unzip_fopen(pz,drvfname,0x0)) == NULL) {
		free(drv);
		gn_close_zip(pz);
		//fprintf(stderr, "Can't open rom driver for %s %s\n", name,drvfname);
		return NULL;
	}

	//Fill the driver struct
	zread_char(z, drv->name, 32);
	zread_char(z, drv->parent, 32);
	zread_char(z, drv->longname, 128);
	zread_uint32le(z, &drv->year);
	for (unsigned i = 0; i < 10; i++)
		zread_uint32le(z, &drv->romsize[i]);
	zread_uint32le(z, &drv->nb_romfile);
	for (unsigned i = 0; i < drv->nb_romfile; i++) {
		zread_char(z, drv->rom[i].filename, 32);
		zread_uint8(z, &drv->rom[i].region);
		zread_uint32le(z, &drv->rom[i].src);
		zread_uint32le(z, &drv->rom[i].dest);
		zread_uint32le(z, &drv->rom[i].size);
		zread_uint32le(z, &drv->rom[i].crc);
	}
	gn_unzip_fclose(z);
	gn_close_zip(pz);
	return drv;
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
