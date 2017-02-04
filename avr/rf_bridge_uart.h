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

#ifndef _RF_BRIDGE_UART_H_
#define _RF_BRIDGE_UART_H_

#include <stdio.h>
#include "fifo_declare.h"

/*
 * declare UART fifos, this is used as buffering helpers to make sure the
 * priority is always to the pulse transceiver interrupt
 */

DECLARE_FIFO(uint8_t, uart_tx, 128);
DECLARE_FIFO(uint8_t, uart_rx, 32);	/* doesn't need as much */

DEFINE_FIFO(uint8_t, uart_tx);
DEFINE_FIFO(uint8_t, uart_rx);

extern uart_tx_t uart_tx;
extern uart_rx_t uart_rx;

int uart_putchar(char c, FILE *stream);

#endif /* _RF_BRIDGE_UART_H_ */
