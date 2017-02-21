/*
 *	avr_cr.h
 *
 * Copyright 2013 Michel Pollet <buserror@gmail.com>
 *
 *	This file is part of simavr.
 *
 *	simavr is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	simavr is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __AVR_CR_H__
#define __AVR_CR_H__
/*
 * Smallest coroutine implementation for AVR. Takes
 * 23 + (24 * tasks) bytes of SRAM to run.
 *
 * Use it like:
 *
 * AVR_TASK(mytask1, 32);
 * AVR_TASK(mytask2, 48);
 * ...
 * void my_task_function() {
 *    do {
 *        AVR_YIELD(1);
 *    } while (1);
 * }
 * ...
 * main() {
 *     AVR_TASK_START(mytask1, my_task_function);
 *     AVR_TASK_START(mytask2, my_other_task_function);
 *     do {
 *          AVR_RESUME(mytask1);
 *          AVR_RESUME(mytask2);
 *     } while (1);
 * }
 * NOTE: Do *not* use "static" on the function prototype, otherwise it
 * will fail to link (compiler doesn't realize the "jmp" is referencing)
 *
 * NOTE2: This file ought to be included in your "main" code, do not include
 * it in other files. If you want coroutine in other .c files, all you need
 * is declare a prototype to AVR_YIELD(int) and call that from your functions
 */
#include <setjmp.h>
#include <stdint.h>

/*
 * Stack switching. Note that there is no security for the user
 * to go over the number of bytes allocated for stack, trial and
 * error will help here; or use simavr to check the status of the
 * stack.
 * You can easily see what is 'in use' in a stack by initializing
 * it with 0xff before starting the task, and see how far it gets
 * munched when running.
 */
static inline void _set_stack(register void * stack) __attribute__((always_inline));
static inline void _set_stack(register void * stack)
{
	asm volatile (
			"in r0, __SREG__" "\n\t"
			"cli" "\n\t"
			"out __SP_H__, %B0" "\n\t"
			"out __SREG__, r0" "\n\t"
			"out __SP_L__, %A0" "\n\t"
			: : "e" (stack) :
	);
}
typedef struct avr_cr {
#ifdef CR_MAIN
	struct avr_cr *next;
#endif
	jmp_buf jmp;
	uint8_t running : 1, sleeping : 1;
#ifdef CR_EXTRA_CONTEXT
	CR_EXTRA_CONTEXT
#endif
} avr_cr;

void cr_yield(uint8_t _sleep);
#ifdef CR_MAIN
jmp_buf	cr_caller;
avr_cr * cr_current = 0, * cr_head = 0;

void cr_yield(uint8_t _sleep);// __attribute__((noreturn)) __attribute__((naked));

void cr_yield(uint8_t _sleep)
{
	cr_current->running = !_sleep;
	if (!setjmp(cr_current->jmp))
		longjmp(cr_caller, 1);
}
#else
extern jmp_buf	cr_caller;
extern avr_cr * 	cr_current;
#endif

#define AVR_CR(_name) \
	void _name(void) __attribute__((noreturn)) __attribute__((naked));\
	void _name(void)

/*
 * Declares the runtime structure needed for a task, as
 * well as space for it's stack.
 */
#define AVR_TASK(_name, _stack_size) \
	struct { \
		avr_cr cr; \
		uint8_t stack[_stack_size]; \
	} _name

#ifdef CR_MAIN
#define _cr_add_task(_name) \
		_name.cr.next = cr_head; cr_head = &_name.cr;
#else
#define cr_add_task(_name)
#endif

/*
 * Starts a task, this has to be done just once per task
 */
#define cr_start(_name, _entry) \
	if (!setjmp(cr_caller)) { \
		_cr_add_task(_name); \
		cr_current = &_name.cr; \
		_set_stack(_name.stack + sizeof(_name.stack) - 1);\
		asm volatile ("rjmp "#_entry); \
	}

/*
 * Resume the task to the last place it did an AVR_YIELD()
 */
#define cr_resume(_name) \
	if (!setjmp(cr_caller)) { \
		cr_current = &_name.cr; \
		longjmp(cr_current->jmp, 1); \
	}

#ifdef CR_MAIN
/* optional if you won't run all the CR at every ticks */
static __attribute__ ((unused)) void AVR_TASK_RUN() {
	avr_cr * h = cr_head;
	while (h) {
		if (!h->sleeping) {
			if (!setjmp(cr_caller)) { \
				cr_current = h; \
				longjmp(cr_current->jmp, 1); \
			}
			cr_current = NULL;
		}
		h = h->next;
	}
}
#endif

#endif /* __AVR_CR_H__ */
