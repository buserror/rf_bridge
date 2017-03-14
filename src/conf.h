
#ifndef _CONF_H_
#define _CONF_H_

#include <stdint.h>

typedef struct conf_mqtt_t {
	char 	root[32];
	char	hostname[96];
	int		port; // broker port
	struct {
		uint32_t 	qos : 4,
					retain : 1;
	} def;
} conf_mqtt_t;

#include "conf_matches.h"

typedef struct conf_switch_t {
	msg_match_t * matches;
} conf_switch_t;

typedef struct conf_sensor_t {
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


#endif
