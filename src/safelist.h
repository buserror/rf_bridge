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

#ifndef _SAFELIST_H_
#define _SAFELIST_H_

#include "list.h"


typedef struct safelist_t {
	struct list_head head;
	int	 	lock;
} safelist_t;

void safelist_init(safelist_t * l)
{
	INIT_LIST_HEAD(&l->head);
	l->lock = 0;
}

static inline void safelist_lock(safelist_t *l) {
	while (!__sync_bool_compare_and_swap(&l->lock, 0, 1))
		usleep(1);
}
static inline void safelist_unlock(safelist_t *l) {
	l->lock = 0;
}

static void safelist_add_tail(safelist_t * l, struct list_head *h)
{
	safelist_lock(l);
	list_add_tail(h, &l->head);
	safelist_unlock(l);
}

static void safelist_del(safelist_t * l, struct list_head *h)
{
	safelist_lock(l);
	list_del(h);
	safelist_unlock(l);
}


#endif
