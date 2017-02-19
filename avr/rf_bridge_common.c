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

#define STACK_DEBUG

#ifdef STACK_DEBUG
#include <string.h> // memset
#endif

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
struct {
	uint8_t display_pulses: 1, display_stacks: 1;
} flags;

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
	mode_StartTransmit,
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
ISR(TIMER0_COMPA_vect)	// handler for Output Compare 0 overflow interrupt
{
	static uint8_t bit = 0;			// bool
	static uint8_t shifter = 0;		// shift register for edge detect
	static uint8_t tp[2];

	D(pin_set(pin_Debug2);)

	switch (transceiver_mode) {
		case mode_Receiving: {
			/* debouncing, wait until we got clear levels to
			 * change the real 'bit' value */
			static uint8_t b;
			b = pin_get(pin_Receiver);
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
			if (tp[bit])
				tp[bit]--;
			if (tp[bit])
				break;
			bit = !bit;
			if (bit) {
				current_pulse++;
				tp[0] = pulse[current_pulse][0];
				tp[1] = pulse[current_pulse][1];
				if (current_pulse == msg_end) {
					transceiver_mode = mode_Idle;
					bit = 0; // we're done, return to 0
				} else
					bit = !!tp[1];
			}
			pin_set_to(pin_Transmitter, bit);
		}	break;
		case mode_StartTransmit: {
			bit = 1;
			transceiver_mode = mode_Transmitting;
			current_pulse = msg_start;
			tp[0] = pulse[current_pulse][0];
			tp[1] = pulse[current_pulse][1];
			pin_set_to(pin_Transmitter, 1);
		}	break;
	}
	D(pin_clr(pin_Debug2);)

	tickcount++;
	sei();
}

// absolute value substraction for durations etc
static uint8_t abs_sub(uint8_t v1, uint8_t v2) {
	return v1 > v2? v1 - v2 : v2 - v1;
}

/* used by the syncsearch, and used by the manchester decoder too */
uint8_t syncduration = 0;
uint8_t chk = 0, byte = 0;
uint8_t bcount = 0;
uint8_t msg_type = 'P';

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
AVR_CR(cr_syncsearch)
{
	uint8_t pi = current_pulse;
	uint8_t syncstart = 0;
	uint8_t synclen = 0;
	uint8_t manchester = 0;
	do {
		while (pi == current_pulse) {
			if (synclen == 0) {
				if (!uart_rx_isempty(&uart_rx))
					running_state = state_ReceivingCommand;
			}
			cr_yield(0);
		}
		while (pi != current_pulse && synclen < 8) {
			uint8_t d = pulse[pi][0] + pulse[pi][1];

			if (d < 0x20 || abs_sub(d, syncduration) > 8) {
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

			if (flags.display_pulses)
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
AVR_CR(cr_decode_ask)
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
			uart_putchar('M', 0);
			uart_putchar('A', 0);
			uart_putchar(':', 0);
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
AVR_CR(cr_decode_manchester)
{
	do {
		cr_yield(0);

		uint8_t pi = msg_start;
		uart_putchar('M', 0);
		uart_putchar('M', 0);
		uart_putchar(':', 0);

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
AVR_CR(cr_decode_pulses)
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

static uint8_t recv_match_string_P(
		const char * w)
{
	uint8_t i = 0;
	uint8_t b = pgm_read_byte(w); // we know we already matches the first one
	while (b == pgm_read_byte(w + i)) {
		i++;
		if (!pgm_read_byte(w + i))
			break;
		b = uart_recv();
	}
	return b;
}

/*
 * Return 0 if a double character hex value was decoded, otherwise,
 * return the character that was received (or 0xff for timeout)
 */
static uint8_t getsbyte(
		uint8_t * res )
{
	uint8_t cnt = 0;
	*res = 0;
	while (cnt < 2) {
		uint8_t s = uart_recv();
		*res <<= 4;
		if (s >= '0' && s <= '9')		*res |= s - '0';
		else if (s >= 'a' && s <= 'f')	*res |= s - 'a' + 10;
		else if (s >= 'A' && s < 'F')	*res |= s - 'A' + 10;
		else return s;
		cnt++;
	}
	return 0;
}

/*
 * This is not meant to be terribly pretty, it's made to be small, use
 * almost no sram and the minimal amount of stack. Thus the goto's
 *
 * I didn't want to create a 'line buffer' and a late parser; it would
 * eat 512 bytes of SRAM as we can send 255 bits.
 */
AVR_CR(cr_receive_cmd)
{
	do {
		cr_yield(0);
		uint8_t state = 0;
		uint8_t err = 0;
		static uint8_t b;
		static uint8_t byte;

		b = uart_recv();
		if (b == 0xff)
			goto again;
		transceiver_mode = mode_Idle;
		if (b == 'M') {
			uint8_t msg_type = uart_recv();
			if (msg_type == 0xff)
				goto again;

			switch (msg_type) {
			case 'A':
				syncduration = 0x63;	/* default ASK bit duration */
				break;
			case 'M':
				syncduration = 0x40;/* default manchester clock * 2 */
				break;
			case 'P':
				break;
			default:
				err = b;
				goto skipline;
			}
			bcount = 0;
			uint8_t chk = 0x55;
			do {
				b = uart_recv();

newkey:
				switch(b) {
				case ':': /* raw data */
					do {
						// process end of message or timeout
						if ((b = getsbyte(&byte)))
							goto newkey;
						chk += byte;
						// here we /know we got a valid hex value
						switch (msg_type) {
						case 'A':
							for (uint8_t b = 0; b < 8; b++) {
								uint8_t bit = (byte >> (7-b)) & 1;
								pulse[bcount][bit] =
										syncduration - (syncduration/4);
								pulse[bcount][!bit] =
										syncduration / 4;
								bcount++;
							}
							break;
						case 'M':	// TODO
							break;
						case 'P':
							break;
						}
					} while (1);
					break;
				case '*': /* checksum */
					if (getsbyte(&b))
						goto skipline;
				//	printf_P(PSTR("< %d chk %02x/%02x\n"),
				//			current_pulse, b, chk);
					if (b == chk) {
						state++;
						pulse[bcount][0] = 127; // long low pulse
						pulse[bcount][1] = 0;
						bcount++;
						pulse[bcount][0] = 127; // long low pulse
						pulse[bcount][1] = 0;
						msg_end = bcount + 1;
						msg_start = 0;
						if (msg_end <= 16)	// too small, don't bother
							goto skipline;
						b = 3; // retries
						while (b--) {
							transceiver_mode = mode_StartTransmit;
							while (transceiver_mode != mode_Idle) {
								cr_yield(1);
							}
						}
						goto skipline;
					} else {
						err = '*';
						goto skipline;
					}
					break;
				case '!': /* pulse duration */
					if (getsbyte(&syncduration))
						goto skipline;
					chk += syncduration;
					break;
				case '#': /* number of bits total */
					if (getsbyte(&bcount))
						goto skipline;
					chk += bcount;
					break;
				default:
					err = b;
					goto skipline;
				}
			} while (1);
		} else if (b == 'P') {
			if ((b = recv_match_string_P(PSTR("PULSE\n"))) == '\n') {
				flags.display_pulses = 1;
				state++;
			} else
				err = b;
		} else if (b == 'D') {
			if ((b = recv_match_string_P(PSTR("DEMOD\n"))) == '\n') {
				flags.display_pulses = 0;
				state++;
			} else
				err = b;
		}
#ifdef STACK_DEBUG
		else if (b == 'S') {
			if ((b = recv_match_string_P(PSTR("STACK\n"))) == '\n') {
				flags.display_stacks = 1;
				state++;
			} else
				err = b;
		}
#endif
skipline:
		/* wait for end of line (if not already in 'b'), or timeout */
		while (b >= ' ' && b != 0xff)
			b = uart_recv();
		if (err) printf_P(PSTR("!%d\n"), err);
		else if (state) printf_P(PSTR("*OK\n"));
#ifdef SIMAVR
		b = 255;
		while (b--) {
			sleep_cpu();
		}
		cli();cli();	 sleep_cpu();
#endif
again:
		running_state = state_SyncSearch;
		msg_start = msg_end = current_pulse = 0;
	} while (1);
}

/*
 * use the STACK command to dump the usage of the stacks, if
 * you have enabled STACK_DEBUG. The stacks are trimmed to the minimum
 * needed, so any strange behaviour should be looked at here, first
 */
AVR_TASK(syncsearch, 48 * 2);
AVR_TASK(decode_ask, 108);
AVR_TASK(decode_manchester, 108);
AVR_TASK(decode_pulses, 48 * 2);
AVR_TASK(receive_cmd, 96 * 2);

void rf_bridge_run() __attribute__((noreturn)) __attribute__((naked));

void rf_bridge_run()
{
	// RF pin input and output
	pin_input(pin_Receiver);
	pin_clr(pin_Receiver); // no pullup on data pin
	pin_output(pin_Transmitter);
	pin_clr(pin_Transmitter);

	sei();
	printf_P(PSTR("* Starting RF Firmware\n"));

#ifdef STACK_DEBUG
	memset(syncsearch.stack, 0xff, sizeof(syncsearch.stack));
	memset(decode_ask.stack, 0xff, sizeof(decode_ask.stack));
	memset(decode_manchester.stack, 0xff, sizeof(decode_manchester.stack));
	memset(decode_pulses.stack, 0xff, sizeof(decode_pulses.stack));
	memset(receive_cmd.stack, 0xff, sizeof(receive_cmd.stack));
#endif

#ifdef SIMAVR
	/* add a message to the buffer */
	const char * msg = "MA!30:40553300#19*66\n";
	printf(msg);
	for (int8_t i = 0; msg[i]; i++)
		uart_rx_write(&uart_rx, msg[i]);
#endif

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
					printf_P(PSTR("#%02x!%0x*%02x\n"),
							bcount, syncduration, chk);
				running_state = state_SyncSearch;
				msg_end = 0;
			}	break;
			case state_ReceivingCommand: {
				cr_resume(receive_cmd);
			}	break;
		}

#ifdef STACK_DEBUG
		if (flags.display_stacks) {
			flags.display_stacks = 0;

			uint16_t i;
			uint16_t max;
#define print_stack(_name) \
			max = sizeof(_name).stack;\
			for (i = 0; i < max && _name.stack[i] == 0xff; i++) ; \
			printf_P(PSTR(#_name " %d/%d\n"), max-i, max);

			print_stack(syncsearch);
			print_stack(decode_ask);
			print_stack(decode_manchester);
			print_stack(decode_pulses);
			print_stack(receive_cmd);
		}
#endif
	}
}
