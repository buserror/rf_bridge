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

#ifndef _CONF_SWITCH_H_
#define _CONF_SWITCH_H_

#include "msg.h"
#include "utils.h"

typedef struct msg_switch_t {
	struct msg_switch_t *next;
	union {
		msg_t			msg;
		uint8_t 		b[sizeof(msg_t) + (256/8)];
	};
	uint32_t				pload_flags : 3, lineno;
	uint64_t			last;	// last time this was sent
	const char * 	msg_txt;
	const char *		mqtt_path;
	const char *		mqtt_pload;
	char 			_data[];
} msg_switch_t;

struct conf_mqtt_t;
struct conf_switch_t;

int
parse_switch(
		struct conf_mqtt_t * mqtt,
		struct conf_switch_t * conf,
		fileio_p file,
		char * l );

struct conf_pir_t;
int
parse_pir(
		struct conf_mqtt_t * mqtt,
		struct conf_pir_t * conf,
		fileio_p file,
		char * l );

#endif /* _CONF_SWITCH_H_ */
