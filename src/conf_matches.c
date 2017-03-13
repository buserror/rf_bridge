/*
 * matches.c
 *
 *  Created on: 27 Feb 2017
 *      Author: michel
 */

#include "conf_matches.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

msg_match_t * matches = NULL;
/* TODO: Put that in the environment */
extern const char *mqtt_root;

int
parse_matches(
		fileio_p file,
		char * l )
{
	const char * key = strsep(&l, " \t");
	const char * msg = strsep(&l, " \t");
	const char * mqtt_path = strsep(&l, " \t");
	const char * mqtt_qos = strsep(&l, " \t");
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
			strlen(mqtt_root) + 1 +
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
	sprintf(d, "%s/%s", mqtt_root, mqtt_path);
	m->mqtt_path = d; d += strlen(d) + 1;
	if (mqtt_pload) {
		strcpy(d, mqtt_pload); m->mqtt_pload = d; d+= strlen(d) + 1;
		if (strstr(mqtt_pload, "\"on\":true"))
			m->pload_flags |= 1;
		if (strstr(mqtt_pload, "\"on\":false"))
			m->pload_flags |= 2;
	}
	if (mqtt_qos)
		m->mqtt_qos = atoi(mqtt_qos);
	m->lineno = file->linecount;

	if (matches)
		m->next = matches;
	matches = m;

	return 0;
}
