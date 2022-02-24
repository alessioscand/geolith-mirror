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

void gn_set_error_msg(char *fmt,...) {
	va_list pvar;
	va_start(pvar, fmt);
	vsnprintf(gnerror,GNERROR_SIZE,fmt,pvar);
}
