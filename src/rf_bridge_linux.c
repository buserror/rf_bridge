#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>

#ifdef MQTT
#include <mosquitto.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>

struct mosquitto *mosq = NULL;

struct {
	const char *name;
} mqtt_weather_name[8] = {
		[0].name = "outside",
		[1].name = "lounge",
		[2].name = "lab",
};
#endif

const char *mqtt_path = NULL;


unsigned debug_sync;

uint8_t pulse[256][2];

// overflow substraction for the counters
uint8_t ovf_sub(uint8_t v1, uint8_t v2) {
	return v1 > v2 ? 255 - v1 + v2 : v2 - v1;
}
// absolute value substraction for durations etc
uint8_t abs_sub(uint8_t v1, uint8_t v2) {
	return v1 > v2? v1 - v2 : v2 - v1;
}

// easy as pie here
uint8_t msg[256/8];
uint8_t bcount = 0;
uint8_t chk = 0;


typedef struct msg_t {
	uint32_t	pulses: 1, type: 7, chk: 8, bitcount;
	uint32_t	pulse_duration: 8, checksum_valid: 1, bytecount;
	uint8_t		msg[0];
} msg_t, *msg_p;


/* Add a bit to the buffer, update the checksum */
static inline void stuffbit(uint8_t b) {
	uint8_t bn = bcount % 8;
	msg[bcount / 8] |= b << (7 - bn);
	bcount++;
	if (bn == 7) {
		chk += msg[(bcount - 1) / 8];
		msg[bcount / 8] = 0;
	}
}

/* Shift the whole buffer right or left, depending */
static void shift_buffer(int8_t shift) {
	uint16_t *ml = (uint16_t*)msg;

	for (uint8_t i = 0; i < (256 / (sizeof(ml[0]) * 8)); i++) {
		uint16_t w0 = ntohs(ml[i]);
		uint16_t w1 = i < ((256 / 32) - 1) ? ntohs(ml[i + 1]) : 0;
		uint16_t w = (w0 << (8 - shift)) | (w1 >> (8 + shift));
		ml[i] = htons(w);
	}
	/* adjust bit counts if we add/removed any */
	bcount = bcount - shift;
}

/* Ambient Weather F007th */
/* details are at https://forum.arduino.cc/index.php?topic=214436.15 */
static uint8_t weather_chk(const uint8_t *buff, uint8_t length) {
	uint8_t mask = 0x7C;
	uint8_t checksum = 0x64;

	for (uint8_t byteCnt = 0; byteCnt < length; byteCnt++) {
		uint8_t data = buff[byteCnt];

		for (int8_t bitCnt = 7; bitCnt >= 0; bitCnt--) {
			// Rotate mask right
			uint8_t bit = mask & 1;
			mask = (mask >> 1) | (mask << 7);
			if (bit)
				mask ^= 0x18;
			// XOR mask into checksum if data bit is 1
			if (data & 0x80)
				checksum ^= mask;
			data <<= 1;
		}
	}
	return checksum;
}

static void weather_decode() {
	uint8_t chk = weather_chk(msg + 1, 5);
	int temp = ((msg[3] & 0x7) << 8) | msg[4];
	temp -= 400 + 320;
	temp = (temp * 5) / 9;
	if (msg[3] & 0x08) temp *= -1;
	uint8_t hum = msg[5];
	uint8_t bat = msg[3] & 0x80;
	uint8_t station = msg[2];
	uint8_t channel = (msg[3] >> 4) & 7;

	if (chk == msg[6]) {
		if (0)
			printf("%% Station:%3d Chan: %d Hum:%2d%% Temp:%2d.%dC %s\n",
				station, channel, hum,
				temp / 10, temp % 10,
				bat ? " LOW BAT":"");

#ifdef MQTT
		char *root;
		if (mqtt_weather_name[channel].name)
			asprintf(&root, "%s/sensor/%s", mqtt_path, mqtt_weather_name[channel].name);
		else
			asprintf(&root, "%s/sensor/%d", mqtt_path, channel);

		char *v;
		asprintf(&v, "{"
				"\"c\":%d.%d,"
				"\"h\":%d,"
				"\"lbat\":%s,"
				"\"ch\":%d"
				"}"
				,
				temp / 10, temp % 10,
				hum,
				bat ? "true":"false",
				channel
			);
		printf("%s %s\n", root, v);
		mosquitto_publish(mosq, NULL, root, strlen(v), v, 1, true);
#endif

	}
}

uint8_t manchester = 0;

void
decoder(
		msg_p m)
{
	uint8_t end = m->bytecount;
	uint8_t start = 0;
	uint8_t pi = 0;

	uint8_t syncstart = 0;
	uint8_t syncduration = 0;
	uint8_t synclen = 0;
	manchester = 0;

	typedef uint8_t pulse_t[2];
	pulse_t *pulse = m->msg;

	/* this piece scans the pulse stream for some sort of synchronization.
	 * typically 4 pulses of roughly equal length. When found, the phases
	 * are also compared; if they are roughly equal, it is likely to be the
	 * start sequence of some manchester encoded stream. Otherwise, it's much
	 * more likely to be generic encoding.
	 */
	while (pi != end && synclen < 8) {
		uint8_t d = pulse[pi][0] + pulse[pi][1];
		if (d < 12 || abs_sub(d, syncduration) > 8) {
			syncstart = pi;
			syncduration = d;
			synclen = 0;
			manchester = 0;
		} else {
			if (abs_sub(pulse[pi][1], pulse[pi][0]) < 12)
				manchester++;
			else
				manchester = 0;
			if (debug_sync > 1)
				printf("sync %d delta %d/%d = %d\n", synclen,
					syncduration, d, syncduration - d);
			syncduration += (d - syncduration) / 2;
			synclen++;
		}
		pi++;
	}
	if (debug_sync)
		printf("syncstart %d synclen = %d, manchester: %d\n", syncstart,
			synclen, manchester);
	if (pi == end) {
		printf("MN:%d\n", ovf_sub(start, end));
		return;
	}
	bcount = 0;
	msg[0] = 0;
	chk = 0x55;
	if (!manchester) {
		pi = syncstart;
		while (pi != end) {
			uint8_t bit = pulse[pi][1] > pulse[pi][0];
			stuffbit(bit);
			pi++;
		}
	} else {
		// We know what a half pulse is, it's synclen / 2
		pi = syncstart + (synclen - manchester);
		if (synclen - manchester)
			printf("** Adjusted start %d huh %d\n", pi,
					synclen - manchester);
		uint8_t bit = 0, phase = 1;
		uint8_t demiclock = 0;
		uint8_t stuffclock = 0;
		uint8_t margin = syncduration / 4;

		/*
		 * Could demi-clocks; stuff the current bit value at each cycles,
		 * and change the bit values when we get a phase that is more than
		 * a demi synclen.
		 */
		while (pi != end) {
			if (stuffclock != demiclock) {
				if (stuffclock & 1)
					stuffbit(bit);
				stuffclock++;
			}
			// if the phase is double the demiclock, change polarity
			if (abs_sub(pulse[pi][phase], syncduration) < margin) {
				bit = phase;
				demiclock++;
			}
			demiclock++;
			if (stuffclock != demiclock) {
				if (stuffclock & 1)
					stuffbit(bit);
				stuffclock++;
			}

			if (phase == 0) pi++;
			phase = !phase;
		}
	}
}

void display() {
	if (bcount >= 64) {
		// look for weather sensor

		/* idea here is to shift the header around to try to find the constant
		 * header, and if found, we offset the whole buffer to match */
		uint16_t *ml = (uint16_t*)msg;
		int shift = 0;
		for (int i = 0; i < 8; i++, shift++) {
			uint32_t w = (ntohs(ml[0]) << (8 - shift)) |
							(ntohs(ml[1]) >> (8 + shift));

			if (w == 0x0145) {
				// weather station message is shifted by that amount
				shift_buffer(shift);

			//	printf("Weather %08x shift %d bcount %d\n", w, shift, bcount);
				weather_decode();
				break;
			}
		}
	}
#if 0
	if (bcount < 16)
		return;
	chk = 0x55;
	printf("%s:", manchester ? "MM" : "MA");
	for (uint8_t i = 0; i < (bcount + 7) / 8; i++) {
		printf("%02x", msg[i]);
		chk += msg[i];
	}
	chk += bcount;
	printf("#%02x*%02x\n", bcount, chk);
#endif
}

/*
 * Return 0 if a double character hex value was decoded, otherwise,
 * return the character that was received (or 0xff for timeout)
 */
static uint8_t getsbyte(
		char ** str,
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

msg_p msg_init(msg_p m, uint8_t type)
{
	if (!m)
		return m;
	m->chk = 0x55;
	m->type = type;
	m->pulses = type == 'P';
	m->bitcount = m->bytecount = 0;
	m->pulse_duration = m->checksum_valid = 0;
	m->msg[0] = 0;
	return m;
}


typedef struct msg_match_t {
	struct msg_match_t *next;
	union {
		msg_t			msg;
		uint8_t 		b[sizeof(msg_t) + (256/8)];
	};
	int 			mqtt_qos : 4, lineno;

	const char * 	msg_txt;
	const char *	mqtt_path;
	const char *	mqtt_pload;
	char 			_data[];
} msg_match_t;

msg_match_t * matches = NULL;

int main(int argc, const char *argv[])
{
	const char *serial_path = NULL;
	const char *mqtt_hostname = NULL;
	const char *mqtt_password = NULL;
	const char *mapping_path = NULL;
	char line[1024];

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") && i < (argc-1)) {
			mqtt_hostname = argv[++i];
		} else if (!strcmp(argv[i], "-r") && i < (argc-1)) {
			mqtt_path = argv[++i];
		} else if (!strcmp(argv[i], "-p") && i < (argc-1)) {
			mqtt_password = argv[++i];
		} else if (!strcmp(argv[i], "-m") && i < (argc-1)) {
			mapping_path = argv[++i];
		} else if (!serial_path)
			serial_path = argv[i];
		else {
			fprintf(stderr, "%s invalid argument %s\n", argv[0], argv[i]);
			exit(1);
		}
	}
	if (argc == 1 || !serial_path) {
		fprintf(stderr,
				"%s: [-h <mqtt_hostname>] [-p <mtqq_password>] "
				"[-r <mqtt root name>] [-m <message mapping filename] "
				"<serial port device file>\n",
				argv[0]);
		exit(1);
	}
	if (mapping_path) {
		FILE *mf = fopen(mapping_path, "r");
		int linecount = 0;
		if (!mf) {
			perror(mapping_path);
			exit(1);
		}
		while (fgets(line, sizeof(line), mf)) {
			linecount++;
			// strip empty lines, comments
			while (*line && line[strlen(line)-1] <= ' ')
				line[strlen(line)-1] = 0;
			char * l = line;
			while (*l == ' ' || *l == '\t')
				l++;
			if (!*l || *l == '#') continue;

			const char * msg = strsep(&l, " \t");
			const char * mqtt_path = strsep(&l, " \t");
			const char * mqtt_qos = strsep(&l, " \t");
			const char * mqtt_pload = strsep(&l, " \t");
printf("%d %s %s %s %s\n",  linecount, msg, mqtt_path, mqtt_qos, mqtt_pload);
			if (!msg || msg[0] != 'M' || (msg[1] != 'A' && msg[1] != 'M')) {
				fprintf(stderr, "%s:%d invalid message format\n",
						mapping_path, linecount);
				continue;
			}
			if (!mqtt_path) {
				fprintf(stderr, "%s:%d missing MQTT path\n",
						mapping_path, linecount);
				continue;
			}

			int size = strlen(msg) + 1 + strlen(mqtt_path) + 1 +
					(mqtt_pload ? strlen(mqtt_pload) + 1 : 0) +
					sizeof (msg_match_t);
			msg_match_t *m = calloc(1, size);
			char *d = m->_data;
			strcpy(d, msg); m->msg_txt = d; d+= strlen(d) + 1;
			strcpy(d, mqtt_path); m->mqtt_path = d; d+= strlen(d) + 1;
			if (mqtt_pload) {
				strcpy(d, mqtt_pload); m->mqtt_pload = d; d+= strlen(d) + 1;
			}
			if (mqtt_qos)
				m->mqtt_qos = atoi(mqtt_qos);
			m->lineno = linecount;

			if (matches)
				m->next = matches;
			matches = m;
		}
		fclose(mf);
	}
#ifdef MQTT
	if (!mqtt_hostname)
		mqtt_hostname = getenv("MQTT");
	if (!mqtt_hostname)
		mqtt_hostname = getenv("MQTT_HOST");
	if (!mqtt_password)
		mqtt_password = getenv("MQTT_PASS");

	if (mqtt_hostname) {
		mosquitto_lib_init();

		char *client;
		char hn[128];
		gethostname(hn, sizeof(hn));
		asprintf(&client, "%s/%s/%d", hn, basename(strdup(argv[0])), getpid());
		mosq = mosquitto_new(client, true, 0);

		if (!mqtt_path)
			mqtt_path = hn;	// safe, we don't return anytime soon
		// TODO: CHANGE? login default to hostname
		if (mqtt_password)
			mosquitto_username_pw_set(mosq, hn, mqtt_password);

		int rc = mosquitto_connect_async(mosq, mqtt_hostname, 1883, 60);
		if (rc) {
			perror("mosquitto_connect");
			exit(1);
		}
		mosquitto_loop_start(mosq);
		printf("MQTT started\n");
	}
#else
	if (mqtt_hostname) {
		fprintf(stderr, "%s MQTT is disabled!\n", argv[0]);
		exit(1);
	}
#endif
	/* in case it's a serial port do stuff to it */
	{
		const char * stty =
			"stty -clocal -icanon -hupcl -cread -opost -echo <%s >/dev/null 2>&1";
		char * d = malloc(strlen(stty) + strlen(serial_path) + 10);

		sprintf(d, stty, serial_path);
		printf("%s\n", d);
		system(d);
		free(d);
	}
	FILE * f = fopen(serial_path, "r");
	if (!f) {
		perror(serial_path);
		exit(1);
	}
	union {
		msg_t m;
		uint8_t filler[sizeof(msg_t) + 512];
	} u;
	while (fgets(line, sizeof(line), f)) {
		// strip line
		while (*line && line[strlen(line)-1] <= ' ')
			line[strlen(line)-1] = 0;
		if (!*line) continue;
		printf("%s\n", line);

		if (*line == '#')
			continue;
		if (*line != 'M')
			continue;
		msg_init(&u.m, line[1]);

		char * cur = line + 2;
		char what = 0;
		while (strlen(cur) >= 2) {
			uint8_t d;
			uint8_t newwhat = getsbyte(&cur, &d);
			if (newwhat) {
				what = newwhat;
				continue;
			}
			switch (what) {
				case ':': {	/* msg data */
					u.m.msg[u.m.bytecount++] = d;
					u.m.chk += d;
				}	break;
				case '#': /* number of bits in sequence */
					u.m.bitcount = d;
					u.m.chk += d;
					break;
				case '!': /* pulse duration */
					u.m.pulse_duration = d;
					u.m.chk += d;
					/* don't really need this at this end */
					break;
				case '*': /* checksum */
					u.m.checksum_valid = d == u.m.chk;
					if (u.m.checksum_valid) {
						if (u.m.bitcount)
							decoder(&u.m);
						display(&u.m);
					}
					break;
			}
		}
	}
	fclose(f);
}
