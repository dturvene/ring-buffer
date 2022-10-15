/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2022 Dahetral Systems
 * Author: David Turvene (dturvene@gmail.com)
 *
 * emacs include file template
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


