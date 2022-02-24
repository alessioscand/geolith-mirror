/*
 * gnutil.c
 *
 * Contain various utility stuff used by gngeo
 *
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gnutil.h"

char gnerror[1024];

/* check if dir_name exist. Create it if not */
int check_dir(char *dir_name) {
    DIR *d;

    if (!(d = opendir(dir_name)) && (errno == ENOENT)) {
        mkdir(dir_name, 0755);
        return GN_FALSE;
    }
    if(d)
    	closedir(d);
    return GN_TRUE;
}

char *get_gngeo_dir(void) {
    static char *filename = NULL;
    int len = strlen(getenv("HOME")) + strlen("/.gngeo/") + 1;

    if (!filename) {
        filename = malloc(len * sizeof (char));
        sprintf(filename, "%s/.gngeo/", getenv("HOME"));
    }
    check_dir(filename);
    //printf("get_gngeo_dir %s\n",filename);
    return filename;
}

void gn_set_error_msg(char *fmt,...) {
	va_list pvar;
	va_start(pvar, fmt);
	vsnprintf(gnerror,GNERROR_SIZE,fmt,pvar);
}
