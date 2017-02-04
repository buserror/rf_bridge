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

/* local includes */
#define CR_MAIN
#include "avr_cr.h"

#ifdef SIMAVR
/* simavr specific, not strictly needed until simulating */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega328p");
#endif

#include "rf_bridge_common.h"
#include "rf_bridge_pins.h"

extern FILE mystdout;

int main()
{
	stdout = &mystdout;

	// LED on
	pin_output(pin_LED);
	pin_set_to(pin_LED, 0);

	// http://www.nongnu.org/avr-libc/user-manual/group__util__setbaud.html
	UCSR0A |= (1 << U2X0);
	UCSR0C = (3 << UCSZ00); // 8 bits
	UBRR0H = 0;
	UBRR0L = 0x10;
	// enable receiver, transmitter and the interrupts
	UCSR0B |= (1 << UDRIE0) | (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);

	// https://et06.dunweber.com/atmega_timers/
	// 16mhz -> 32.7k timer for resampling
	TCCR0A = (1 << WGM01); // CTC mode
	TCCR0B = (1 << CS01);
	OCR0A = 0x3d / 2;
    TIMSK0  |= (1 << OCIE0A);

#ifdef DEBUG
	// debug pins for logic analyser
	pin_output(pin_Debug0);
	pin_output(pin_Debug1);
	pin_output(pin_Debug2);
	pin_output(pin_Debug3);
#endif

	rf_bridge_run();
}
