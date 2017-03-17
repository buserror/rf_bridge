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
parse_mqtt_flags(
		struct conf_mqtt_flags_t * flags,
		fileio_p file,
		char * line )
{
	char *l = strdupa(line);
	const char * key = strsep(&l, " \t=");
	const char * value = strsep(&l, " \t");

	if (!strcmp(key, "qos") && value && isdigit(value[0])) {
		int v = atoi(value);
		flags->qos = v;
	} else if (!strcmp(key, "retain")) {
		if (!value)
			flags->retain = 1;
		else {
			if (isdigit(value[0]))
				flags->retain = atoi(value) != 0;
			else
				flags->retain = strcmp(value, "true");
		}
	} else
		return 1;

	return 0;
}

int
parse_mqtt(
		struct conf_mqtt_t * mqtt,
		fileio_p file,
		char * line )
{
	char *l = strdupa(line);
	const char * key = strsep(&l, " \t=");
	const char * value = strsep(&l, " \t");

	if (!strcmp(key, "hostname")) {
		if (!value)
			strncpy(mqtt->hostname, "localhost", sizeof(mqtt->hostname));
		else
			strncpy(mqtt->hostname, value, sizeof(mqtt->hostname));
	} else if (!strcmp(key, "port")) {
		uint16_t port = 0;
		if (value)
			port = atoi(value);
		if (!port)
			port = 1883;
		mqtt->port = port;
	} else if (parse_mqtt_flags(&mqtt->def, file, line)) {
		fprintf(stderr, "%s:%d invalid config keyword '%s'\n",
				file->fname, file->linecount, key);
		return -1;
	}

	return 0;
}
