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
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "avr_cr.h"
#include "rf_bridge_uart.h"

#ifdef DEBUG
#define D(w) w
#else
#define D(w)
#endif

uart_tx_t uart_tx;
uart_rx_t uart_rx;

#ifndef USART0_UDRE_vect
#define USART0_UDRE_vect USART_UDRE_vect
#define USART0_RX_vect USART_RX_vect
#endif

ISR(USART0_UDRE_vect)
{
	if (uart_tx_isempty(&uart_tx)) {
		UCSR0B &= ~(1 << UDRIE0);
		return;
	}
	UDR0 = uart_tx_read(&uart_tx);
}
ISR(USART0_RX_vect)
{
	uint8_t b = UDR0;
	if (!uart_rx_isfull(&uart_rx))
		uart_rx_write(&uart_rx, b);
}
int uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
		uart_putchar('\r', stream);
	D(GPIOR2 = c;)
	while (uart_tx_isfull(&uart_tx)) {
		UCSR0B |= (1 << UDRIE0); // make sure we don't stall
	//	if (cr_current)	cr_yield(1);
		sleep_cpu();
	}
	uart_tx_write(&uart_tx, c);
	UCSR0B |= (1 << UDRIE0);
	return 0;
}

FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);
