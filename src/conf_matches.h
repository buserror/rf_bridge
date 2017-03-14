/*
 * matches.h
 *
 *  Created on: 27 Feb 2017
 *      Author: michel
 */

#ifndef _MATCHES_H_
#define _MATCHES_H_

#include "msg.h"
#include "utils.h"

typedef struct msg_match_t {
	struct msg_match_t *next;
	union {
		msg_t			msg;
		uint8_t 		b[sizeof(msg_t) + (256/8)];
	};
	int 				mqtt_qos : 4,
					pload_flags : 3, lineno;
	uint64_t			last;	// last time this was sent
	const char * 	msg_txt;
	const char *		mqtt_path;
	const char *		mqtt_pload;
	char 			_data[];
} msg_match_t;

struct conf_mqtt_t;
struct conf_switch_t;

int
parse_matches(
		struct conf_mqtt_t * mqtt,
		struct conf_switch_t * conf,
		fileio_p file,
		char * l );

extern msg_match_t * matches;

#endif /* _MATCHES_H_ */
