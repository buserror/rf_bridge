
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
	msg_match_t * matches;
} conf_switch_t;

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
