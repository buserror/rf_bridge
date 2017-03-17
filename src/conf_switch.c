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

int
parse_switch(
		struct conf_mqtt_t * mqtt,
		struct conf_switch_t * conf,
		fileio_p file,
		char * line )
{
	char *l = strdupa(line);
	const char * key = strsep(&l, " \t");
	const char * msg = strsep(&l, " \t");
	const char * mqtt_path = strsep(&l, " \t");
	const char * mqtt_pload = strsep(&l, " \t");

	//printf("%d %s %s %s %s\n",  file->linecount, msg,
	//		mqtt_path, mqtt_qos, mqtt_pload);
	if (strcmp(key, "map")) {
		fprintf(stderr, "%s:%d invalid config keyword '%s'\n",
				file->fname, file->linecount, key);
		return -1;
	}

	if (!msg || msg[0] != 'M' || (msg[1] != 'A' && msg[1] != 'M')) {
		fprintf(stderr, "%s:%d invalid message format\n",
				file->fname, file->linecount);
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
			sizeof (msg_match_t);
	msg_match_t *m = calloc(1, size);

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

	if (conf->matches)
		m->next = conf->matches;
	conf->matches = m;

	return 0;
}
