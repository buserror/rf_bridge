/*
	RF433 transceiver firmware.

	This program was made to interface an basic cheapo transceiver 433Mhz
	module with a (linux) host, to let it handle 'grown up' already
	decoded messages for example to use with MQTT, home automation,
	Alexa/Echo and so forth.

	This was made to filter in 433MHZ messages from various remotes and
	sensors, so some appropriate processing on the fly, and pass that
	onward to a host computer for 'real' processing.

	The idea is to have a free running pulse trail detection, and being
	able to notice when it's no longer noise. Firmware can also detect
	Amplitutde-Key Shifting (ASK) or if it's manchester encoding and
	decode both on the fly.

	In the other of operation, firmware can receive the same message
	format with pulses length, and transmit them using a 433MHZ
	transmitter.

	Copyright 2016 Michel Pollet <buserror@gmail.com>

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

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

#include "avr_cr.h"
#include "rf_bridge_uart.h"
#include "rf_bridge_pins.h"
#include "rf_bridge_common.h"

#define ARRAY_SIZE(xx) (sizeof(xx)/sizeof(xx[0]))

#ifdef DEBUG
#define D(w) w
#else
#define D(w)
#endif

/*
 * Main firmware states; always fallback to SyncSearch
 */
enum {
	state_SyncSearch = 0,
	state_Decoding_ASK,
	state_Decoding_Manchester,
	state_DecodeRawPulses,
	state_DecodeDone,
	state_ReceivingCommand,
};
volatile uint8_t	running_state = state_SyncSearch;

/* Flag: Are we displaying raw pulses, or already decoded messages */
uint8_t display_pulses = 0;

volatile uint8_t tickcount = 0;

/*
 * everything is pretty much geared to use uint8_t integer overflow
 * naturally, without having to worry about boundaries etc, so the pulse
 * count buffer is more or less fixed at that size, and everything else
 * like cursors will also use the same 8 bits overflows.
 */
volatile uint8_t pulse[256][2];		// circular buffer of pulse durations
volatile uint8_t current_pulse = 0;	// current 'filling' cursor
volatile uint8_t msg_start = 0,
				msg_end = 0;			// markers for the decoders

/*
 * Transceiver state; we are half duplex, can't receive and transmit, as
 * it'd be rather silly to receive back garbled version of what we send.
 * (garbled because of the proximity of the transmitter)
 */
enum {
	mode_Idle = 0,
	mode_Receiving,
	mode_Transmitting,
};
volatile uint8_t transceiver_mode = mode_Receiving;

/*
 * This is the 'sensitive' part here. Nothing fancy, everything needs
 * to be quick as the frequency of the timer is quite high; so in receive
 * mode we just do some filtered edge detection (cheaply).
 *
 * On transmit we just go over the buffer setting the output state as we
 * go along, decrementing remaining pulses as we go forward.
 */
ISR(TIMER0_COMPA_vect)	// handler for Output Compare 1 overflow interrupt
{
	static uint8_t bit = 0;			// bool
	static uint8_t shifter = 0;		// shift register for edge detect

	D(pin_set(pin_Debug2);)

	switch (transceiver_mode) {
		case mode_Receiving: {
			/* debouncing, wait until we got clear levels to
			 * change the real 'bit' value */
			uint8_t b = pin_get(pin_Receiver);
			shifter = (shifter << 1) | b;
			if ((shifter & 0x07) == 0x07)
				b = 1;
			else if ((shifter & 0x07) == 0x0)
				b = 0;

			/* increment the pulse count for the phase (bit) we are in */
			if (pulse[current_pulse][b] < 127)
				pulse[current_pulse][b]++;
			/* if raising edge, switch pulse counter to the next one */
			if (!bit && b) {
				current_pulse++;
				pulse[current_pulse][0] = pulse[current_pulse][1] = 0;
			}
			bit = b;
			D(pin_set_to(pin_Debug0, bit);)
		}	break;
		case mode_Transmitting: {
			if (pulse[current_pulse][bit])
				pulse[current_pulse][bit]--;
			if (pulse[current_pulse][bit])
				break;
			bit = !bit;
			pin_set_to(pin_Transmitter, bit);
			if (bit) {
				current_pulse++;
				if (current_pulse == msg_end) {
					transceiver_mode = mode_Idle;
					pin_set_to(pin_Transmitter, bit);
				}
			}
		}	break;
	}
	D(pin_clr(pin_Debug2);)

	tickcount++;
	sei();
}


// overflow substraction for the counters
uint8_t ovf_sub(uint8_t v1, uint8_t v2) {
	return v1 > v2 ? 255 - v1 + v2 : v2 - v1;
}
// absolute value substraction for durations etc
uint8_t abs_sub(uint8_t v1, uint8_t v2) {
	return v1 > v2? v1 - v2 : v2 - v1;
}

/* used by the syncsearch, and used by the manchester decoder too */
uint8_t syncduration = 0;
uint8_t chk = 0, byte = 0;
uint8_t bcount = 0;

/*
 * stuff the next bit in the 8 bits buffer, and output it when full.
 * With enough memory, we could use a much larger buffer and do the print
 * at the end, but it's not really necessary as we send it all to a
 * computer with much more resources.
 */
static void stuffbit(uint8_t b, uint8_t last) {
	uint8_t bn = bcount % 8;
	byte |= b << (7 - bn);
	bcount++;
	if (last || bn == 7) {
		chk += byte;
		printf_P(PSTR("%02x"), byte);
		byte = 0;
	}
}

/*
 * Search for 8 pulses of ~equal duration. Even manchester starts with
 * at least 8 of them like that, while ASK will always be at least
 * 8 bits anyway, so it's a good discriminant
 */
void cr_syncsearch()
{
	uint8_t pi = current_pulse;
	uint8_t syncstart = 0;
	uint8_t synclen = 0;
	uint8_t manchester = 0;
	do {
		while (pi == current_pulse)
			cr_yield(0);

		while (pi != current_pulse && synclen < 8) {
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
				syncduration += (d - syncduration) / 2;
				synclen++;
			}
			pi++;
		}

		if (synclen == 8) {
			D(pin_set(pin_Debug1);)
			msg_start = syncstart;

			if (display_pulses)
				running_state = state_DecodeRawPulses;
			else if (manchester > 5)
				running_state = state_Decoding_Manchester;
			else
				running_state = state_Decoding_ASK;
			// init decoders
			chk = 0x55;
			bcount = 0;
			byte = 0;
			msg_end = 0;
			D(pin_clr(pin_Debug1);)
		} else if (synclen == 0) {
			if (!uart_rx_isempty(&uart_rx))
				running_state = state_ReceivingCommand;
		}
		if (running_state != state_SyncSearch) {
			cr_yield(0);
			synclen = manchester = syncduration = 0;
			pi = msg_start;// play catchup
		}
	} while (1);
}


/*
 * After we got a sync, and it's been decided it's ASK, go on
 * and do the decoding on the fly until and end of pulse
 */
void cr_decode_ask()
{
	do {
		cr_yield(0);

		uint8_t pi = msg_start;

		uint8_t pcount = 0;

		/*
		 * we look for 20 bits of valid data before we go ahead and
		 * decide it's a nice message. Pointless to print garbage if it
		 * doesn't match what we need. This will happen pretty often
		 */
		while (pcount < 20) {
			while (pi == current_pulse)
				cr_yield(0);
			uint8_t d = pulse[pi][0] + pulse[pi][1];
			if (abs_sub(d, syncduration) <= 8) {
				pcount++;
				pi++;
			} else
				break;
		}
		/*
		 * Ok, seems we're happy we got a message, print it until
		 * a long pulse
		 */
		if (pcount == 20) {
			pi = msg_start;	/* restart at beginning */
			printf_P(PSTR("MA:"));
			D(pin_set_to(pin_Debug3, 0);)
			do {
				// wait for bits
				while (pi == current_pulse)
					cr_yield(0);
				while (pi != current_pulse && !msg_end) {
					uint8_t b = pulse[pi][1] > pulse[pi][0];
					D(pin_set_to(pin_Debug3, b);)
					msg_end = pulse[pi][0] >= 127;
					stuffbit(b, msg_end);
					pi++;
					D(pin_set_to(pin_Debug3, 0);)
				}
			} while (!msg_end);
		}
		running_state = state_DecodeDone;
		msg_start = pi;
		D(pin_set_to(pin_Debug3, 0);)
	} while (1);
}

/*
 * After we got a sync, and it's been decided it's basic encoding, go on
 * and do the decoding on the fly until and end of pulse
 */
void cr_decode_manchester()
{
	do {
		cr_yield(0);

		uint8_t pi = msg_start;
		printf_P(PSTR("MM:"));

		// We know what a half pulse is, it's synclen / 2
		uint8_t bit = 0, phase = 1;
		uint8_t demiclock = 0;
		uint8_t stuffclock = 0;
		uint8_t margin = syncduration / 4;

		do {
			// wait for bits
			while (pi == current_pulse)
				cr_yield(0);

			/*
			 * Count demi-clocks; stuff the current bit value at each
			 * cycles, and change the bit values when we get a phase
			 * that is more than a demi syncduration.
			 */
			while (pi != current_pulse && !msg_end) {
				msg_end = pulse[pi][0] >= 127;

				if (stuffclock != demiclock) {
					if (stuffclock & 1)
						stuffbit(bit, msg_end);
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
						stuffbit(bit, msg_end);
					stuffclock++;
				}

				if (phase == 0) pi++;
				phase = !phase;
			}
		} while (!msg_end);
		running_state = state_DecodeDone;
		msg_start = pi;
	} while (1);
}

/*
 * Raw print of the pulses. Used for debug and in 'learning mode'
 * for remotes, buttons and so forth.
 */
void cr_decode_pulses()
{
	do {
		cr_yield(0);

		uint8_t pi = msg_start;
		printf_P(PSTR("MP:"));
		do {
			// wait for bits
			while (pi == current_pulse)
				cr_yield(0);
			while (pi != current_pulse && !msg_end) {
				msg_end = pulse[pi][0] >= 127;
				printf_P(PSTR("%02x%02x"), pulse[pi][1], pulse[pi][0]);
				chk += pulse[pi][1] + pulse[pi][0];
				bcount++;
				pi++;
			}
		} while (!msg_end);
		running_state = state_DecodeDone;
		msg_start = pi;
	} while (1);
}

/*
 * Reads a character form the uart FIFO, return 0xff if we timeouted
 */
uint8_t uart_recv()
{
	uint16_t timeout = 0;
	uint8_t tick = tickcount;
	while (uart_rx_isempty(&uart_rx) && timeout < 1000) {
		if (tick != tickcount) {
			tick++;
			if (tick == 255)
				timeout++;
		} else
			cr_yield(0);
	}
	return uart_rx_isempty(&uart_rx) ? 0xff : uart_rx_read(&uart_rx);
}

void cr_receive_cmd()
{
	static struct {
		uint8_t i, state;
		const char *pfx;
	} cmd[3] = {
			{ .pfx = "MP:", .state = 1 },
			{ .pfx = "PULSE", .state = 2 },
			{ .pfx = "MSG", .state = 3 },
	};

	do {
		cr_yield(0);
		uint8_t state = 0, cmp = 0;
		do {
			uint8_t b;
			cmp = 0;
			do {
				b = uart_recv();
				uart_putchar(b, 0);// local echo, temp?
				for (uint8_t ci = 0; ci < ARRAY_SIZE(cmd) && !state; ci++) {
					if (cmd[ci].i == cmp && cmd[ci].pfx[cmp] == b) {
						cmd[ci].i++;
						if (cmd[ci].pfx[cmd[ci].i] == 0)
							state = cmd[ci].state;
					}
				}
				cmp++;
			} while (b >= ' ' && b != 0xff && !state && cmp < 24);
			for (uint8_t ci = 0; ci < ARRAY_SIZE(cmd) && !state; ci++)
				cmd[ci].i = 0;
			uint8_t err = b == 255;
			switch (state) {
				case 1: {

				}	break;
				case 2: {
					display_pulses = 1; err = 0;
				}	break;
				case 3: {
					display_pulses = 0; err = 0;
				}	break;
			}
			if (err) printf_P(PSTR("!%d\n"), err);
			else if (state) printf_P(PSTR("*OK\n"));
			break;
		} while (1);
		running_state = state_SyncSearch;
		msg_start = msg_end = current_pulse = 0;
	} while (1);
}

AVR_TASK(syncsearch, 64);
AVR_TASK(decode_ask, 64);
AVR_TASK(decode_manchester, 96);
AVR_TASK(decode_pulses, 64);
AVR_TASK(receive_cmd, 96);

void rf_bridge_run()
{
	// RF pin input and output
	pin_input(pin_Receiver);
	pin_clr(pin_Receiver); // no pullup on data pin
	pin_output(pin_Transmitter);
	pin_clr(pin_Transmitter);

	sei();
	printf_P(PSTR("* Starting RF Firmware\n"));

	/* start coroutines on their own stacks */
	cr_start(syncsearch, cr_syncsearch);
	cr_start(decode_ask, cr_decode_ask);
	cr_start(decode_manchester, cr_decode_manchester);
	cr_start(decode_pulses, cr_decode_pulses);
	cr_start(receive_cmd, cr_receive_cmd);

	while (1) {
		sleep_cpu(); // wakes after a timer tick, or UART etc

		switch (running_state) {
			case state_SyncSearch:
				transceiver_mode = mode_Receiving;
				cr_resume(syncsearch);
				break;
			case state_Decoding_ASK:
				cr_resume(decode_ask);
				break;
			case state_Decoding_Manchester:
				cr_resume(decode_manchester);
				break;
			case state_DecodeRawPulses:
				cr_resume(decode_pulses);
				break;
			case state_DecodeDone: {
				chk += bcount;
				chk += syncduration;
				if (bcount)
					printf_P(PSTR(":%02x!%0x*%02x\n"),
							bcount, syncduration, chk);
				running_state = state_SyncSearch;
				msg_end = 0;
			}	break;
			case state_ReceivingCommand: {
				transceiver_mode = mode_Idle;
				cr_resume(receive_cmd);
			}	break;
		}
	}
}
