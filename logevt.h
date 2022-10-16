/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2022 Dahetral Systems
 * Author: David Turvene (dturvene@gmail.com)
 *
 * Copy the ringbuffer pattern in ringbuffer.c for an event logger in
 * the ringbuffer.c example.  evt_que calls are placed strategically in 
 * the test code while running.  THen print_evts is called to display all
 * events.
 */

#ifndef _LOGEVT_H
#define _LOGEVT_H

#include <stdint.h>     /* uint32_t, etc. */
#include <time.h>       /* timespec */
#include <pthread.h>    /* pthread_mutex */

/**
 * event types to log
 */
typedef enum evtid {
	EVT_ENQ = 1,
	EVT_DEQ = 2,
	EVT_DEQ_IDLE = 3,
	EVT_END = 4,
} evtid_t;

/* externally visible prototypes */
void evt_enq(evtid_t id, uint32_t val);
void print_evts(void);

#endif /* _LOGEVT_H */


