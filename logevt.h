/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * emacs include file template
 */

#ifndef _LOGEVT_H
#define _LOGEVT_H

#include <stdint.h>     /* uint32_t, etc. */
#include <time.h>       /* timespec */
#include <pthread.h>    /* pthread_mutex */

typedef enum evtid {
	EVT_ENQ = 1,
	EVT_DEQ = 2,
	EVT_END = 3,
} evtid_t;

#define LOG_QDEPTH 100

typedef struct logrec {
	evtid_t id;
	uint32_t val;
	struct timespec tstamp;
} logrec_t;

typedef struct qlog {
	logrec_t bufs[LOG_QDEPTH];
	logrec_t *enq;
	logrec_t *deq;
	int32_t count;

	pthread_mutex_t *lockp;
	logrec_t *first;
	logrec_t *last;
	int32_t max;
	void (*cb)(const logrec_t *);
} qlog_t;

void evt_enq(evtid_t id, uint32_t val);
int evt_deq(logrec_t *recp);
void print_evts(void);

#endif /* _LOGEVT_H */


