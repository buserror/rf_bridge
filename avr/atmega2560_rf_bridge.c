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
AVR_MCU(F_CPU, "atmega2560");

AVR_MCU_VCD_PORT_PIN('H', 4, "TX"); // transmitter
AVR_MCU_VCD_PORT_PIN('H', 5, "RX"); // receiver
AVR_MCU_VCD_PORT_PIN('H', 3, "Antenna"); // pin_Antenna
AVR_MCU_EXTERNAL_PORT_PULL('H', (1 << 3), 0); // pulldown here
AVR_MCU_VCD_PORT_PIN('E', 4, "Debug0"); // Debug0
AVR_MCU_VCD_PORT_PIN('E', 5, "Decode"); // Debug1
AVR_MCU_VCD_PORT_PIN('G', 5, "Sync"); // Debug2

AVR_MCU_VCD_IRQ(TIMER0_COMPA);
AVR_MCU_VCD_IRQ(TIMER0_COMPB);
AVR_MCU_VCD_IRQ(USART0_UDRE);
//AVR_MCU_VCD_IRQ_PENDING(USART0_UDRE);
//AVR_MCU_VCD_ALL_IRQ_PENDING();

const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
	{ AVR_MCU_VCD_SYMBOL("UDR0"), .what = (void*)&UDR0, },
	{ AVR_MCU_VCD_SYMBOL("UDRE0"), .mask = (1 << UDRE0), .what = (void*)&UCSR0A, },
	{ AVR_MCU_VCD_SYMBOL("UDRIE0"), .mask = (1 << UDRIE0), .what = (void*)&UCSR0B, },
	{ AVR_MCU_VCD_SYMBOL("GPIOR1"), .what = (void*)&GPIOR1, },
	{ AVR_MCU_VCD_SYMBOL("GPIOR2"), .what = (void*)&GPIOR2, },
	/* we subvert these two registers as debug for simavr traces */
	{ AVR_MCU_VCD_SYMBOL("Pulse"), .what = (void*)&SPSR, },
	{ AVR_MCU_VCD_SYMBOL("Duration"), .what = (void*)&SPCR, },
};

#endif

#include "rf_bridge_common.h"
#include "rf_bridge_pins.h"

extern FILE mystdout;

int main() __attribute__((naked));

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
	OCR0B = 0x3d / 2;

#ifdef DEBUG
	// debug pins for logic analyser
	pin_output(pin_Debug0);
	pin_output(pin_Debug1);
	pin_output(pin_Debug2);
	pin_output(pin_Debug3);
#endif

	rf_bridge_run();
}
