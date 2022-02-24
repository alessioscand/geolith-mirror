/*
 * gnutil.h
 *
 */

#ifndef GNUTIL_H_
#define GNUTIL_H_

#include <stdio.h>

int check_dir(char *dir_name);
void gn_set_error_msg(char *fmt,...);

#define DEBUG_LOG printf

#define GNERROR_SIZE 1024
extern char gnerror[GNERROR_SIZE];



#endif /* GNUTIL_H_ */
