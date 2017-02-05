/*
 * arduidiot.h
 *
 *  Created on: Jul 14, 2014
 *      Author: michel
 */

#ifndef ARDUIDIOT_H_
#define ARDUIDIOT_H_

/*
 * This file allow access to pins with a single number, instead of a
 * pair of port/bit; it uses the arduino pin numbers, and it's main
 * advantages is that it's all preprocessor based, therefore if you
 * use constants as parameter for the macros, they will resolve as
 * a single instruction in most cases
 */

#include <stdint.h>
#include <avr/io.h>


#ifndef _CONCAT2
#define _CONCAT2(_np, _nn) _np##_nn
#define _CONCAT(_np, _nn) _CONCAT2(_np,_nn)
#endif

#ifdef ARDUIDIO_FULL

#define M2560
#define M644
#define M168
typedef struct ardupin_t {
	uint32_t port : 4, pin : 3, analog : 1, adc : 4, pwm : 1, ardupin;
} ardupin_t, *ardupin_p;

#else

typedef struct ardupin_t {
	uint8_t port : 4, pin : 3;
} ardupin_t, *ardupin_p;

#if defined(__AVR_ATmega2560__)
#define M2560
#define _PORT_BASE A
#endif
#if  defined(__AVR_ATmega644__) || defined(__AVR_ATmega644P__)
#define M644
#define _PORT_BASE B
#endif
#if  defined(__AVR_ATmega168__) || defined(__AVR_ATmega168P__) || defined(__AVR_ATmega168PA__) || \
		defined(__AVR_ATmega328P__)
#define M168
#define _PORT_BASE B
#endif
#endif

#ifndef _PORT_BASE
#error Invalid AVR part, make sure to add mapping/alias for this one
#endif


enum {
#ifdef M2560
	_AVR_PORTA,
#endif
	_AVR_PORTB, _AVR_PORTC, _AVR_PORTD, _AVR_PORTE, _AVR_PORTF,
	_AVR_PORTG, 
	// PORTH and others are way afterward, at 0x100 IO space
	_AVR_PORTH, _AVR_PORTI, _AVR_PORTJ, _AVR_PORTK, _AVR_PORTL,
};

#ifdef ARDUIDIO_FULL
#define MPIN(_n, _p, _b) \
	[ _n] = { .ardupin =  _n, .port = _CONCAT(_AVR_PORT, _p), .pin =  _b }
#define MADCB(_n, _p, _b) \
	[ _n] = { .ardupin =  _n, .port = _CONCAT(_AVR_PORT, _p), .pin =  _b, .analog = 1, .adc = _b }
#define MADCL(_n, _p, _b, _a) \
	[ _n] = { .ardupin =  _n, .port = _CONCAT(_AVR_PORT, _p), .pin =  _b, .analog = 1, .adc = _a }
#else
#define MPIN(_n, _p, _b) \
	[ _n] = { .port = _CONCAT(_AVR_PORT, _p), .pin =  _b }
#define MADCB(_n, _p, _b) MPIN(_n, _p, _b)
#define MADCL(_n, _p, _b, _a) MPIN(_n, _p, _b)
#endif

#define MPORT(_B, _P) \
	MPIN( _B + 0, _P, 0), \
	MPIN( _B + 1, _P, 1), \
	MPIN( _B + 2, _P, 2), \
	MPIN( _B + 3, _P, 3), \
	MPIN( _B + 4, _P, 4), \
	MPIN( _B + 5, _P, 5), \
	MPIN( _B + 6, _P, 6), \
	MPIN( _B + 7, _P, 7)

#define MADC(_B, _P) \
	MADCB( _B + 0, _P, 0), \
	MADCB( _B + 1, _P, 1), \
	MADCB( _B + 2, _P, 2), \
	MADCB( _B + 3, _P, 3), \
	MADCB( _B + 4, _P, 4), \
	MADCB( _B + 5, _P, 5), \
	MADCB( _B + 6, _P, 6), \
	MADCB( _B + 7, _P, 7)

#ifndef ARDUIDIO_FULL
static const ardupin_t _pins[] = {
#endif

#if defined(ARDUIDIO_FULL) && defined(M168)
static const ardupin_t arduidiot_168[] = {
#endif
#ifdef M168
	MPORT( 0, D),
	MPORT( 8, B),
	MADC( 16, C), /* technically, 2 adc mode than there is.. */
#endif
#if defined(ARDUIDIO_FULL) && defined(M644)
};
static const ardupin_t arduidiot_644[] = {
#endif
#ifdef M644
#error WHAT?
	MPORT( 0, B),
	MPORT( 8, D),
	MPORT(16, C),
	
	MADCB(24, A, 7),
	MADCB(25, A, 6),
	MADCB(26, A, 5),
	MADCB(27, A, 4),
	MADCB(28, A, 3),
	MADCB(29, A, 2),
	MADCB(30, A, 1),
	MADCB(31, A, 0),
#endif
#if defined(ARDUIDIO_FULL) && defined(M2560)
};
static const ardupin_t arduidiot_2560[] = {
#endif
#ifdef M2560
	MPIN( 0, E, 0),
	MPIN( 1, E, 1),
	MPIN( 2, E, 4),
	MPIN( 3, E, 5),
	MPIN( 4, G, 5),
	MPIN( 5, E, 3),
	MPIN( 6, H, 3),
	MPIN( 7, H, 4),
	MPIN( 8, H, 5),
	MPIN( 9, H, 6),

	MPIN(10, B, 4),
	MPIN(11, B, 5),
	MPIN(12, B, 6),
	MPIN(13, B, 7),
	MPIN(14, J, 1),
	MPIN(15, J, 0),
	MPIN(16, H, 1),
	MPIN(17, H, 0),
	MPIN(18, D, 3),
	MPIN(19, D, 2),

	MPIN(20, D, 1),
	MPIN(21, D, 0),
	MPORT(22, A),	

	MPIN(30, C, 7),
	MPIN(31, C, 6),
	MPIN(32, C, 5),
	MPIN(33, C, 4),
	MPIN(34, C, 3),
	MPIN(35, C, 2),
	MPIN(36, C, 1),
	MPIN(37, C, 0),
	MPIN(38, D, 7),
	MPIN(39, G, 2),

	MPIN(40, G, 1),
	MPIN(41, G, 0),
	MPIN(42, L, 7),
	MPIN(43, L, 6),
	MPIN(44, L, 5),
	MPIN(45, L, 4),
	MPIN(46, L, 3),
	MPIN(47, L, 2),
	MPIN(48, L, 1),
	MPIN(49, L, 0),

	MPIN(50, B, 3),
	MPIN(51, B, 2),
	MPIN(52, B, 1),
	MPIN(53, B, 0),
	MADCB(54, F, 0),
	MADCB(55, F, 1),
	MADCB(56, F, 2),
	MADCB(57, F, 3),
	MADCB(58, F, 4),
	MADCB(59, F, 5),

	MADCB(60, F, 6),
	MADCB(61, F, 7),
	MADCL(62, K, 0, 8),
	MADCL(63, K, 1, 9),
	MADCL(64, K, 2, 10),
	MADCL(65, K, 3, 11),
	MADCL(66, K, 4, 12),
	MADCL(67, K, 5, 13),
	MADCL(68, K, 6, 14),
	MADCL(69, K, 7, 15),

	MPIN(70, G, 4),
	MPIN(71, G, 3),
	MPIN(72, J, 2),
	MPIN(73, J, 3),
	MPIN(74, J, 7),
	MPIN(75, J, 4),
	MPIN(76, J, 5),
	MPIN(77, J, 6),
	MPIN(78, E, 2),
	MPIN(79, E, 6),

	MPIN(80, E, 7),
	MPIN(81, D, 4),
	MPIN(82, D, 5),
	MPIN(83, D, 6),
	MPIN(84, H, 2),
	MPIN(85, H, 7),
#endif
};

/* The whole point here is that these macros are all 'constant' to feed the compiler, and therefore
 * will work just as well as the old PORTX = xxx and compile with as many instructions.
 * In fact the table up there isn't even compiled in */
#define _AVR_IOPORT_SIZE (((void*)&PORTC)-((void*)&PORTB))

#ifdef M2560
#define __PORT(__p) (*(volatile uint8_t *)(((void*)( \
				(_pins[__p].port >= _AVR_PORTH) ? \
					((&_CONCAT(PORT,H)) + ((_pins[__p].port - _AVR_PORTH) * _AVR_IOPORT_SIZE)) : \
					((&_CONCAT(PORT,_PORT_BASE)) + ((_pins[__p].port) * _AVR_IOPORT_SIZE)) \
				))))
#define __PIN(__p) (*(volatile uint8_t *)(((void*)(\
				(_pins[__p].port >= _AVR_PORTH) ? \
					((&_CONCAT(PIN,H)) + ((_pins[__p].port - _AVR_PORTH) * _AVR_IOPORT_SIZE)) : \
					((&_CONCAT(PIN,_PORT_BASE)) + ((_pins[__p].port) * _AVR_IOPORT_SIZE)) \
				))))
#define __DDR(__p) (*(volatile uint8_t *)(((void*)(\
				(_pins[__p].port >= _AVR_PORTH) ? \
					((&_CONCAT(DDR,H)) + ((_pins[__p].port - _AVR_PORTH) * _AVR_IOPORT_SIZE)) : \
					((&_CONCAT(DDR,_PORT_BASE)) + ((_pins[__p].port) * _AVR_IOPORT_SIZE)) \
				))))
#else
#define __PORT(__p) (*(volatile uint8_t *)(((void*)( \
					((&_CONCAT(PORT,_PORT_BASE)) + ((_pins[__p].port) * _AVR_IOPORT_SIZE)) \
				))))
#define __PIN(__p) (*(volatile uint8_t *)(((void*)(\
					((&_CONCAT(PIN,_PORT_BASE)) + ((_pins[__p].port) * _AVR_IOPORT_SIZE)) \
				))))
#define __DDR(__p) (*(volatile uint8_t *)(((void*)(\
					((&_CONCAT(DDR,_PORT_BASE)) + ((_pins[__p].port) * _AVR_IOPORT_SIZE)) \
				))))
#endif

#define pin_set(__p) \
		__PORT(__p) |= (1 << ((_pins[__p].pin)))
#define pin_clr(__p) \
		__PORT(__p) &= ~(1 << ((_pins[__p].pin)))
#define pin_set_to(__p, __v) \
		__PORT(__p) = (__PORT(__p) & ~(1 << ((_pins[__p].pin)))) | ((!!(__v)) << ((_pins[__p].pin)))
#define pin_get(__p) \
		(!!(__PIN(__p) & (1 << ((_pins[__p].pin)))))
#define pin_output(__p) \
		__DDR(__p) |= (1 << ((_pins[__p].pin)))
#define pin_input(__p) \
		__DDR(__p) &= ~(1 << ((_pins[__p]).pin))
#define pin_toggle(__p) \
		__PIN(__p) |= (1 << ((_pins[__p]).pin))


#endif /* ARDUIDIOT_H_ */
