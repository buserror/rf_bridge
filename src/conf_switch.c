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

#include "conf.h"


static int
parse_mapping(
		struct conf_mqtt_t * mqtt,
		struct msg_switch_t ** queue,
		fileio_p file,
		char * line )
{
	char *l = strdupa(line);
	const char * msg = strtok_r(l, " \t", &l);
	const char * mqtt_path = strtok_r(l, " \t", &l);
	const char * mqtt_pload = strtok_r(l, " \t", &l);

	if (!msg || msg[0] != 'M') {
		fprintf(stderr, "%s:%d invalid message format '%s'\n",
				file->fname, file->linecount, msg);
		return -1;
	}
	if (!mqtt_path) {
		fprintf(stderr, "%s:%d missing MQTT path\n",
				file->fname, file->linecount);
		return -1;
	}

	int size = strlen(msg) + 1 +
			strlen(mqtt->root) + 1 +
			strlen(mqtt_path) + 1 +
			(mqtt_pload ? strlen(mqtt_pload) + 1 : 0) +
			sizeof (msg_switch_t);
	msg_switch_t *m = calloc(1, size);

	if (msg_parse(&m->msg, 256, msg) != 0) {
		fprintf(stderr, "%s:%d Can't parse '%s'\n",
				file->fname, file->linecount, msg);
		free(m);
		return -1;
	}
	char *d = m->_data;
	strcpy(d, msg); m->msg_txt = d; d += strlen(d) + 1;
	sprintf(d, "%s/%s", mqtt->root, mqtt_path);
	m->mqtt_path = d; d += strlen(d) + 1;
	if (mqtt_pload) {
		strcpy(d, mqtt_pload); m->mqtt_pload = d; d+= strlen(d) + 1;
		if (strstr(mqtt_pload, "\"on\":true"))
			m->pload_flags |= 1;
		if (strstr(mqtt_pload, "\"on\":false"))
			m->pload_flags |= 2;
	}
	m->lineno = file->linecount;

	if (*queue)
		m->next = *queue;
	*queue = m;

	return 0;
}

int
parse_switch(
		struct conf_mqtt_t * mqtt,
		struct conf_switch_t * conf,
		fileio_p file,
		char * line )
{
	char *l = strdupa(line);
	const char * key = strtok_r(l, " \t=", &l);

	if (!strcmp(key, "map")) {
		parse_mapping(mqtt, &conf->matches, file, l);
	} else if (parse_mqtt_flags(&conf->msg, file, line)) {
		fprintf(stderr, "%s:%d invalid config keyword '%s'\n",
				file->fname, file->linecount, key);
		return -1;
	}

	return 0;
}


int
parse_pir(
		struct conf_mqtt_t * mqtt,
		struct conf_pir_t * conf,
		fileio_p file,
		char * line )
{
	char *l = strdupa(line);
	const char * key = strtok_r(l, " \t=", &l);

	if (!strcmp(key, "pir")) {
		parse_mapping(mqtt, &conf->pir, file, l);
	} else if (!strcmp(key, "mask")) {
		const char * msg = strtok_r(l, " \t", &l);
		printf("msg %s\n", msg);
		if (!msg || msg_parse(&conf->mask, 128, msg) != 0) {
			fprintf(stderr, "%s:%d Can't parse '%s'\n",
					file->fname, file->linecount, msg);
			return -1;
		}
	} else if (parse_mqtt_flags(&conf->msg, file, line)) {
		fprintf(stderr, "%s:%d invalid config keyword '%s'\n",
				file->fname, file->linecount, key);
		return -1;
	}

	return 0;
}
