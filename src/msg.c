/*
 * msg.c
 *
 *  Created on: 27 Feb 2017
 *      Author: michel
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include "msg.h"

/* Add a bit to the buffer, update the checksum */
void
msg_stuffbit(
		msg_p m,
		uint8_t b)
{
	uint8_t bn = m->bitcount % 8;
	m->msg[m->bitcount / 8] |= b << (7 - bn);
	m->bitcount++;
	if (bn == 7) {
		m->chk += m->msg[(m->bitcount - 1) / 8];
		m->msg[m->bitcount / 8] = 0;
	}
	m->bytecount = 1 + (m->bitcount / 8);
}

/* Shift the whole buffer right or left, depending */
void
msg_shift(
		msg_p m,
		int8_t shift)
{
	uint16_t *ml = (uint16_t*)m->msg;
	uint8_t size = m->bytecount;

	for (uint8_t i = 0; i < size; i++) {
		uint16_t w0 = ntohs(ml[i]);
		uint16_t w1 = i < ((256 / 32) - 1) ? ntohs(ml[i + 1]) : 0;
		uint16_t w = (w0 << (8 - shift)) | (w1 >> (8 + shift));
		ml[i] = htons(w);
	}
	/* adjust bit counts if we add/removed any */
	m->bitcount = m->bitcount - shift;
}

msg_p
msg_init(
		msg_p m,
		uint8_t type)
{
	if (!m)
		return NULL;
	m->chk = 0x55;
	m->type = type;
	m->pulses = type == 'P';
	m->bitcount = m->bytecount = 0;
	m->pulse_duration = m->checksum_valid = 0;
	m->msg[0] = 0;
	return m;
}

void
msg_display(
		FILE *out,
		msg_p m,
		const char * pfx)
{
	uint8_t chk = 0x55;
	fprintf(out, "%s%sM%c", pfx ? pfx : "", pfx && *pfx ? " " : "", m->type);
	if (m->pulse_duration)
		fprintf(out, "!%02x", m->pulse_duration);
	fprintf(out, ":");
	for (uint8_t i = 0; i < (m->bitcount + 7) / 8; i++) {
		fprintf(out, "%02x", m->msg[i]);
		chk += m->msg[i];
	}
	chk += m->bitcount;
	chk += m->pulse_duration;
	fprintf(out, "#%02x*%02x\n", m->bitcount, chk);
}


/*
 * Return 0 if a double character hex value was decoded, otherwise,
 * return that character offending, also increment the string pointer
 */
static uint8_t
getsbyte(
		const char ** str,
		uint8_t * res )
{
	uint8_t cnt = 0;
	*res = 0;
	while (cnt < 2) {
		uint8_t s = (*str)[0];
		*res <<= 4;
		if (s >= '0' && s <= '9')		*res |= s - '0';
		else if (s >= 'a' && s <= 'f')	*res |= s - 'a' + 10;
		else if (s >= 'A' && s < 'F')	*res |= s - 'A' + 10;
		else return s;
		*str = *str + 1;
		cnt++;
	}
	return 0;
}

/*
 * Parse a message string, fill up 'm'. Note that is no boundary check here
 * TODO: Add some sort of boundary check
 */
int
msg_parse(
		msg_p m,
		uint16_t maxsize,
		const char *line )
{
	if (*line == '#')
		return -1;
	if (*line != 'M')
		return -1;
	msg_init(m, line[1]);

	const char * cur = line + 2;
	char what = 0, has_checksum = 0;
	while (strlen(cur) >= 2 && *cur > ' ') {
		uint8_t d;
		uint8_t newwhat = getsbyte(&cur, &d);
		if (newwhat) {
			what = newwhat;
			cur++;
			continue;
		}
		switch (what) {
			case ':': {	/* msg data */
				if (m->bytecount < maxsize)
					m->msg[m->bytecount++] = d;
				m->chk += d;
			}	break;
			case '#': /* number of bits in sequence */
				m->bitcount = d;
				m->chk += d;
				break;
			case '!': /* pulse duration */
				m->pulse_duration = d;
				m->chk += d;
				/* don't really need this at this end */
				break;
			case '*': /* checksum */
				has_checksum = 1;
				m->checksum_valid = d == m->chk;
				break;
		}
	}
	return !has_checksum || m->checksum_valid ? 0 : 1;
}
