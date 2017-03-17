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
