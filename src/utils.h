/*
 * utils.h
 *
 *  Created on: 27 Feb 2017
 *      Author: michel
 */

#ifndef _UTILS_H_
#define _UTILS_H_


typedef struct fileio_t {
	FILE *f;
	int linecount;
	const char *fname;
} fileio_t, *fileio_p;

#endif /* _UTILS_H_ */
