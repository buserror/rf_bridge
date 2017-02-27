/*
 * msg.h
 *
 *  Created on: 27 Feb 2017
 *      Author: michel
 */

#ifndef _MSG_H_
#define _MSG_H_

#include <stdio.h>
#include <stdint.h>

typedef struct msg_t {
	uint32_t		pulses: 1, decoded: 1, type: 7, chk: 8, bitcount;
	uint32_t		pulse_duration: 8, checksum_valid: 1,
				max_size : 11, bytecount;
	uint8_t		msg[0];
} msg_t, *msg_p;

typedef union {
	msg_t m;
	uint8_t filler[sizeof(msg_t) + 512];
} msg_full_t;

msg_p
msg_init(
		msg_p m,
		uint8_t type);

void
msg_stuffbit(
		msg_p m,
		uint8_t b);

void
msg_shift(
		msg_p m,
		int8_t shift);

void
msg_display(
		FILE *out,
		msg_p m,
		const char * pfx);

int
msg_parse(
		msg_p m,
		uint16_t maxsize,
		const char *line );

#endif /* _MSG_H_ */
