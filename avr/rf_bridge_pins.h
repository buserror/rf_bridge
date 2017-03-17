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

#ifndef _RF_BRIDGE_PINS_H_
#define _RF_BRIDGE_PINS_H_

#include "arduidiot.h"
/*
 * AVR pins in use. Using the arduidiot notation
 */

#if defined(__AVR_ATmega2560__)

enum {
	pin_Receiver = 8,
	pin_Transmitter = 7,
	pin_Antenna = 6,
	pin_LED = 13,
#ifdef DEBUG
	/* Used in the mega project with the signal analyser */
	pin_Debug0 = 2,
	pin_Debug1,
	pin_Debug2,
	pin_Debug3,
#endif
};

#elif  defined(__AVR_ATmega328P__)

enum {
	pin_Receiver = 19,		// new PCB! A3 vs A7
	pin_Transmitter = 20,	// new PCB! A4 vs A6
	pin_Antenna = 21,		// A5 aka PC5
	pin_LED = 13,
#ifdef DEBUG
	/* Used in the mega project with the signal analyser */
	pin_Debug0 = 2,
	pin_Debug1,
	pin_Debug2,
	pin_Debug3,
#endif
};
#else
#error No pin configuration defined for this part
#endif


#endif /* _RF_BRIDGE_PINS_H_ */
