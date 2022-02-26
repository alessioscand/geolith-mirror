/*
 * Gngeo JG
 *
 * Copyright (c) 2001-2015 Mathieu Peponas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef _TIMER_H_
#define _TIMER_H_

typedef struct timer_struct {
    double time;        // when
    uint32_t odo_debut;
    uint32_t nb_cycle;
    int param;
    uint32_t del_it;
    void (*func) (int param);
    struct timer_struct *next;
} timer_struct;

extern double timer_count;
//extern uint32_t timer_count;

timer_struct *insert_timer(double duration, int param, void (*func) (int));
//timer_struct *insert_timer(uint32_t duration, int param, void (*func) (int));
void del_timer(timer_struct * ts);
void my_timer(void);
double timer_get_time(void);
//uint32_t timer_get_time(void);
void free_all_timer(void);

#endif
