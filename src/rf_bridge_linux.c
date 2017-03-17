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
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "conf.h"

#ifdef MQTT
#include <mosquitto.h>
#if LIBMOSQUITTO_VERSION_NUMBER <= 16000
#undef MQTT
#endif
#endif

#ifdef MQTT
#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>

struct mosquitto *mosq = NULL;
#endif

conf_t g_conf = {
	.mqtt = {
		.hostname = "localhost",
		.port = 1883,
		.def = {
			.qos = 1,
			.retain = 1,
		},
	},
	.sensors.msg = {
		.qos = 1,
		.retain = 0,
	},
	.switches.msg = {
		.qos = 1,
		.retain = 0,
	},
};

const char *serial_path = NULL;
int serial_fd = -1;

static uint64_t gettime_ms()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (((uint64_t)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

// overflow substraction for the counters
uint8_t ovf_sub(uint8_t v1, uint8_t v2) {
	return v1 > v2 ? 255 - v1 + v2 : v2 - v1;
}
// absolute value substraction for durations etc
uint8_t abs_sub(uint8_t v1, uint8_t v2) {
	return v1 > v2? v1 - v2 : v2 - v1;
}


/* Ambient Weather F007th */
/* details are at https://forum.arduino.cc/index.php?topic=214436.15 */
static uint8_t
weather_chk(
		const uint8_t *buff,
		uint8_t length)
{
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

static void
weather_decode(
		conf_mqtt_t *mqtt,
		conf_sensor_t *sensors,
		msg_p m)
{
	uint8_t * msg = m->msg;
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

		char *root;
		if (sensors->sensor[channel].name)
			asprintf(&root, "%s/sensor/%s",
					mqtt->root,
					sensors->sensor[channel].name);
		else
			asprintf(&root, "%s/sensor/%d",
					mqtt->root, channel);

		char *v;
		asprintf(&v, "{"
				"\"c\":%d.%d,"
				"\"h\":%d,"
				"\"lbat\":%s,"
				"\"ch\":%d"
				"}",
				temp / 10, temp % 10,
				hum,
				bat ? "true":"false",
				channel );
		printf("%s %s\n", root, v);
#ifdef MQTT
		if (mqtt->root[0])
			mosquitto_publish(mosq, NULL, root, strlen(v),
					v, sensors->msg.qos, sensors->msg.retain);
#endif
	}
}


static void
display(
		msg_p m )
{
	if (m->bitcount >= 64) {
		// look for weather sensor

		/* idea here is to shift the header around to try to find the constant
		 * header, and if found, we offset the whole buffer to match */
		uint16_t *ml = (uint16_t*)m->msg;
		int shift = 0;
		for (int i = 0; i < 8; i++, shift++) {
			uint32_t w = (ntohs(ml[0]) << (8 - shift)) |
							(ntohs(ml[1]) >> (8 + shift));

			if (w == 0x0145) {
				// weather station message is shifted by that amount
				msg_shift(m, shift);

			//	printf("Weather %08x shift %d bcount %d\n", w, shift, bcount);
				weather_decode(&g_conf.mqtt, &g_conf.sensors, m);
				break;
			}
		}
	}
	if (m->decoded)
		msg_display(stdout, m, "");
}


#ifdef MQTT
/*
 * We use a cheap trick for detecting on/off and also messages that have been
 * sent by /us/ (so we don't create a feedback loop)
 */
static void
mq_message_cb(
		struct mosquitto *mosq,
		void *userdata,
		const struct mosquitto_message *message )
{
	int flags = 0;

	if (message->payloadlen) {
		/* if it's US having received it via RF and published, ignore it */
		if (strstr(message->payload, "\"src\":\"rf\""))
			return;
		if (strstr(message->payload, "\"on\":true"))
			flags |= 1;
		if (strstr(message->payload, "\"on\":false"))
			flags |= 2;
		printf(">> %s %s\n", message->topic, (char*)message->payload);
	}

	msg_switch_t * m = g_conf.switches.matches;
	while (m) {
		if (!strcmp(message->topic, m->mqtt_path)) {
			if (m->pload_flags == flags) {
				uint64_t now = gettime_ms();
				if (now - m->last > 500) {
					m->last = now;
					msg_display(stdout, &m->msg, "SEND");

					/* I feel slightly dirty here, but it allows
					 * the serial port to stay available for writing commands
					 * and stuff, and /normally/ messages aren't that often.
					 * I'm sure linux will cope..?
					 */
					FILE *o = fopen(serial_path, "w");

					if (o) {
						msg_display(o, &m->msg, "");
						fclose(o);
						usleep(200000);
					}
				}
			}
		}
		m = m->next;
	}
}

static void
mq_connect_cb(
		struct mosquitto *mosq,
		void *userdata,
		int result)
{
	if (result) {
		fprintf(stderr, "MQTT: Connect failed\n");
		return;
	}

	msg_switch_t * m = g_conf.switches.matches;
	while (m) {
		mosquitto_subscribe(mosq, NULL, m->mqtt_path, 2);
		m = m->next;
	}
}

static void
mqtt_connect(
	conf_mqtt_t * mqtt )
{

	if (!mqtt->root[0]) {
		printf("MQTT Not configured\n");
		return;
	}
	mosquitto_lib_init();

	char *client;
	char hn[128];
	gethostname(hn, sizeof(hn));
	asprintf(&client, "%s/%s/%d", hn, "rf_bridge", getpid());
	mosq = mosquitto_new(client, true, 0);

	if (!mqtt->hostname[0])
		strncpy(mqtt->hostname, hn, sizeof(mqtt->hostname));

	mosquitto_connect_callback_set(mosq, mq_connect_cb);
	mosquitto_message_callback_set(mosq, mq_message_cb);

	int rc = mosquitto_connect_async(mosq, mqtt->hostname, mqtt->port, 60);
	if (rc) {
		perror("mosquitto_connect");
		exit(1);
	}
	mosquitto_loop_start(mosq);
	printf("MQTT started\n");
}
#endif /* MQTT */



int
main(
		int argc,
		const char *argv[])
{
	const char *conf_filename = NULL;
	char line[1024];

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") && i < (argc-1)) {
			strncpy(g_conf.mqtt.hostname, argv[++i], sizeof(g_conf.mqtt.hostname));
		} else if (!strcmp(argv[i], "-r") && i < (argc-1)) {
			strncpy(g_conf.mqtt.root, argv[++i], sizeof(g_conf.mqtt.root));
		} else if (!strcmp(argv[i], "--no-mqtt")) {
			g_conf.mqtt.root[0] = 0;
		} else if ((!strcmp(argv[i], "-c") || !!strcmp(argv[i], "--conf")) &&
				i < (argc-1)) {
			conf_filename = argv[++i];
		} else if (!serial_path)
			serial_path = argv[i];
		else {
			fprintf(stderr, "%s invalid argument %s\n", argv[0], argv[i]);
			exit(1);
		}
	}
	if (argc == 1 || !serial_path) {
		fprintf(stderr,
				"%s: [-h <mqtt_hostname>] "
				"[-r <mqtt root name>] [-c|--conf <config filename] "
				"<serial port device file>\n",
				argv[0]);
		exit(1);
	}
	if (conf_filename) {
		fileio_t f = {
				.f = fopen(conf_filename, "r"),
				.fname = conf_filename,
		};
		if (!f.f) {
			perror(f.fname);
			exit(1);
		}
		enum {
			conf_switches = 0, // default
			conf_mqtt,
			conf_pirs,
			conf_sensors
		};
		int state = conf_switches;
		static const char * states[] = {
			"[switches]", "[mqtt]", "[pir]", "[sensors]", NULL
		};
		while (fgets(line, sizeof(line), f.f)) {
			f.linecount++;
			// strip empty lines, comments
			while (*line && line[strlen(line)-1] <= ' ')
				line[strlen(line)-1] = 0;
			char * l = line;
			while (*l == ' ' || *l == '\t')
				l++;
			if (!*l || *l == '#') continue;

			if (l[0] == '[') {
				state = -1;
				for (int i = 0; states[i] && state == -1; i++)
					if (!strcmp(states[i], line))
						state = i;
				if (state == -1) {
					fprintf(stderr, "%s:%d: invalid section %s\n",
							f.fname, f.linecount, line);
					break;
				}
				continue;
			}
			switch (state) {
				case conf_mqtt:
					if (parse_mqtt(&g_conf.mqtt, &f, l))
						exit(1);
					break;
				case conf_sensors:
					if (parse_sensor(&g_conf.sensors, &f, l))
						exit(1);
					break;
				case conf_pirs:
					if (parse_pir(&g_conf.mqtt, &g_conf.pirs, &f, l))
						exit(1);
					break;
				case conf_switches:
					if (parse_switch(&g_conf.mqtt, &g_conf.switches, &f, l))
						exit(1);
					break;
			}
		}
		fclose(f.f);
	}
#ifdef MQTT
	if (g_conf.mqtt.root[0])
		mqtt_connect(&g_conf.mqtt);
#else
	fprintf(stderr, "%s MQTT is not compiled in!\n", argv[0]);
#endif
	/* in case it's a serial port do stuff to it */
	{
		const char * stty =
			"stty 115200 clocal -icanon -hupcl -cread -opost -echo -F %s >/dev/null 2>&1";
		char * d = malloc(strlen(stty) + strlen(serial_path) + 10);

		sprintf(d, stty, serial_path);
		printf("%s\n", d);
		if (system(d))
			;	// ok to fail
		free(d);
	}
	FILE * f = fopen(serial_path, "r");
	if (!f) {
		perror(serial_path);
		exit(1);
	}
	printf("Ready...\n");
	msg_display(stdout, &g_conf.pirs.mask, "pir mask");
	msg_full_t u;
	while (fgets(line, sizeof(line), f)) {
		// strip line
		while (*line && line[strlen(line)-1] <= ' ')
			line[strlen(line)-1] = 0;
		if (!*line) continue;
		printf("%s\n", line);

		if (msg_parse(&u.m, 512, line) != 0)
			continue;

		if (!u.m.checksum_valid)
			continue;

		msg_p d = &u.m;
		display(d);
		/*
		 * Check for 'switch' messages
		 */
		msg_switch_t *m = g_conf.switches.matches;
		uint16_t want = ((uint16_t*)d->msg)[0];
		uint64_t now = gettime_ms();
		int done = 0;
		while (m) {
			if (*((uint16_t*)m->msg.msg) == want &&
					!memcmp(m->msg.msg, d->msg, d->bytecount)) {
				if (now - m->last > 500) {
#ifdef MQTT
					if (g_conf.mqtt.root[0])
						mosquitto_publish(mosq, NULL,
							m->mqtt_path,
							strlen(m->mqtt_pload), m->mqtt_pload,
							g_conf.sensors.msg.qos,
							g_conf.sensors.msg.retain);
					printf("%s %s\n", m->mqtt_path, m->mqtt_pload);
#endif
				}
				done++;
				m->last = now;
			}
			m = m->next;
		}
		if (done)
			continue;
		/*
		 * now look for the PIRs, with mask
		 */
		m = g_conf.pirs.pir;
		msg_p mask = &g_conf.pirs.mask;
		while (m) {
			if (d->bitcount >= m->msg.bitcount) {
				int ok = 1;
				for (int i = 0; i < d->bytecount && ok; i++) {
					uint8_t b1 = d->msg[i] & mask->msg[i];
					uint8_t b2 = m->msg.msg[i] & mask->msg[i];

					if (b1 != b2) {
						ok = 0;
					//	if (i) msg_display(stdout, &m->msg, "almost?");
					}
				}
				if (ok) {
					msg_display(stdout, &m->msg, "MATCH!");
					if (now - m->last > 5000) {
#ifdef MQTT
						if (g_conf.mqtt.root[0])
							mosquitto_publish(mosq, NULL,
								m->mqtt_path,
								strlen(m->mqtt_pload), m->mqtt_pload,
								g_conf.pirs.msg.qos,
								g_conf.pirs.msg.retain);
						printf("%s %s\n", m->mqtt_path, m->mqtt_pload);
#endif
					}
					done++;
					m->last = now;
				}
			}
			m = m->next;
		}
	}
	fclose(f);
}
