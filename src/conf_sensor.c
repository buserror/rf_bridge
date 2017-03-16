/*
 * conf_sensor.c
 *
 *  Created on: 16 Mar 2017
 *      Author: michel
 */
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "conf.h"

int
parse_sensor(
		struct conf_sensor_t * sensor,
		fileio_p file,
		char * line )
{
	char *l = strdupa(line);
	const char * key = strsep(&l, " \t");
	const char * index = strsep(&l, " \t");
	const char * name = strsep(&l, " \t");

	if (!strcmp(key, "name")) {
		if (!index || !isdigit(index[0]) || !name) {
			fprintf(stderr, "%s:%d invalid sensor\n",
					file->fname, file->linecount);
			return -1;
		}
		int id = atoi(index);
		if (id >= (sizeof(sensor->sensor) / sizeof(sensor->sensor[0])) ||
				id < 0) {
			fprintf(stderr, "%s:%d invalid sensor index %d\n",
					file->fname, file->linecount, id);
			return -1;
		}
		strncpy(sensor->sensor[id].name, name,
				sizeof(sensor->sensor[0].name));
	} else if (parse_mqtt_flags(&sensor->msg, file, line)) {
		fprintf(stderr, "%s:%d invalid config keyword '%s'\n",
				file->fname, file->linecount, key);
		return -1;
	}
	return 0;
}


