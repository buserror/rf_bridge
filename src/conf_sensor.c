/*
	RF to MQTT Bridge.

	Copyright 2017 Michel Pollet <buserror@gmail.com>

	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
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


