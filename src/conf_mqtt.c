/*
 * conf_mqtt.c
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
