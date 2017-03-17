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

#ifndef _CONF_H_
#define _CONF_H_

#include <stdint.h>

typedef struct conf_mqtt_flags_t {
	uint32_t 	qos : 4,
				retain : 1;
} conf_mqtt_flags_t;

typedef struct conf_mqtt_t {
	char 	root[32];
	char	hostname[96];
	int		port; // broker port
	conf_mqtt_flags_t def;
} conf_mqtt_t;

#include "conf_switch.h"

typedef struct conf_switch_t {
	conf_mqtt_flags_t msg;
	msg_switch_t * matches;
} conf_switch_t;

typedef struct conf_pir_t {
	conf_mqtt_flags_t msg;
	union {
		msg_t			mask;
		uint8_t 		b[sizeof(msg_t) + (256/8)];
	};
	msg_switch_t * pir;
} conf_pir_t;

typedef struct conf_sensor_t {
	conf_mqtt_flags_t msg;
	int count;
	struct {
		char name[32];
	} sensor[16];
} conf_sensor_t;

typedef struct conf_t {
	conf_mqtt_t	mqtt;
	conf_switch_t	switches;
	conf_sensor_t	sensors;
	conf_pir_t	pirs;
} conf_t;

int
parse_mqtt(
		struct conf_mqtt_t * mqtt,
		fileio_p file,
		char * l );
int
parse_mqtt_flags(
		struct conf_mqtt_flags_t * flags,
		fileio_p file,
		char * line );
int
parse_sensor(
		struct conf_sensor_t * sensor,
		fileio_p file,
		char * line );

#endif
