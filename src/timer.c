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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "emu.h"
#include "timer.h"
#include "ym2610/ym2610.h"

double timer_count;
//uint32_t timer_count;

timer_struct *timer_list;
#define MAX_TIMER 3
timer_struct timers[MAX_TIMER];
extern int nb_interlace;
//int nb_timer=0;

double timer_get_time(void) {
//uint32_t timer_get_time(void) {
    return timer_count;
}

timer_struct *insert_timer(double duration, int param, void (*func) (int))
//timer_struct *insert_timer(uint32_t duration, int param, void (*func) (int))
{
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timers[i].del_it) {
            timers[i].time = timer_count + duration;
            timers[i].param = param;
            timers[i].func = func;
            //printf("Insert_timer %d duration=%f param=%d\n",i,duration,timers[i].param);
            timers[i].del_it = 0;
            return &timers[i];
        }
    }
    printf("YM2610: No timer free!\n");
    return NULL;                /* No timer free */
}

void free_all_timer(void) {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        timers[i].del_it=1;
    }
}

void timer_callback_2610(int param);

void del_timer(timer_struct * ts)
{
    ts->del_it = 1;
}

static double inc;
//static uint32_t inc;

void my_timer(void)
{
    static int init = 1;
    int i;

    if (init) {
        init = 0;

        if (conf.pal) {
                inc = ((double) (0.02) / nb_interlace);/* *(1<<TIMER_SH);*/
        } else {
                inc = ((double) (0.01666) / nb_interlace); /* *(1<<TIMER_SH); */
        }
        for (i = 0; i < MAX_TIMER; i++)
            timers[i].del_it = 1;
    }

    timer_count += inc;         /* 16ms par frame */

    for (i = 0; i < MAX_TIMER; i++) {
        if (timer_count >= timers[i].time && timers[i].del_it == 0) {
            //printf("Timer_expire %d duration=%f param=%d\n",i,timers[i].time,timers[i].param);
            if (timers[i].func) timers[i].func(timers[i].param);
            timers[i].del_it = 1;
        }
    }
}
