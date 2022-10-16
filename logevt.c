#include <stdio.h>      /* char I/O, perror */
#include <time.h>       /* clock_gettime */
#include <stdint.h>     /* uint32_t, etc. */
#include <string.h>     /* strcpy */
#include "logevt.h"     /* enums and external function prototypes */

#define ARRAY_SIZE(arr)  (sizeof(arr)/sizeof(arr[0]))

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * define the logrec, typedef to buf_t
 */
typedef struct logrec {
	evtid_t id;
	uint32_t val;
	struct timespec tstamp;
} buf_t;

#define LOG_QDEPTH 100

/**
 * struct qlog - ringbuffer context for logger
 * 
 * This is a hacky cut-and-paste of the test code (struct sq). Only difference
 * is the name and the statically allocated bufs array size.
 * 
 * Probably should refactor both ringbuffer structures into a generic one and
 * then have convert bufs to a pointer, which can point to a custom statically
 * allocated array.
 */
typedef struct qlog {
	buf_t bufs[LOG_QDEPTH];
	buf_t *enq;
	buf_t *deq;
	int32_t count;

	pthread_mutex_t *lockp;
	buf_t *first;
	buf_t *last;
	int32_t max;
	void (*cb)(const buf_t *);
} qlog_t;

/**
 * instantiate the event logger ringbuffer, same pattern as simple queue
 */
static qlog_t logevt = {
	.enq = logevt.bufs,
	.deq = logevt.bufs,
	.count = 0,

	.lockp = &log_mutex,
	.first = logevt.bufs,
	.last = &logevt.bufs[ARRAY_SIZE(logevt.bufs)-1],
	.max = ARRAY_SIZE(logevt.bufs),
};

/**
 * evt_enq - enqueue a log element
 * @id: the event id enum defined in logevt.h
 * @val: value to write to bufs element
 * 
 * write a logger record using the event id enum and value
 * use gettime for timestamp and write that also.
 * Other than that, same logic as ringbuffer q_enq
 */
void evt_enq(evtid_t id, uint32_t val)
{
	struct timespec ts;

	/* get clock before mutex because expensive, resulting in
	 * calling pthread stalling
	 */
	clock_gettime(CLOCK_MONOTONIC, &ts);
		
	pthread_mutex_lock(logevt.lockp);
	
	/* if enq (newest) is about to overwrite the deq (oldest) location
	 * then move deq to next oldest before overwriting, if last element
	 * then move deq to first element.
	 */
	if (logevt.count == logevt.max && logevt.enq == logevt.deq) {
		if (logevt.deq == logevt.last)
			logevt.deq = logevt.first;
		else
			logevt.deq++;
	}

	logevt.enq->id = id;
	logevt.enq->val = val;
	logevt.enq->tstamp.tv_sec = ts.tv_sec;
	logevt.enq->tstamp.tv_nsec = ts.tv_nsec;

	if (logevt.count < logevt.max)
		logevt.count++;

	/* if last bufs, set to first
	 * otherwise increment to next bufs element
	 */
	if (logevt.enq == logevt.last)
		logevt.enq = logevt.first;
	else
		logevt.enq++;
	pthread_mutex_unlock(logevt.lockp);
}

/**
 * evt_deq - dequeue a log element
 * @recp: pointer to a current bufs element
 * 
 * read a logger record. Same logic as ringbuffer q_deq
 *
 * Return:
 *  same as q_deq, 0 for success and negative otherwise
 */
int evt_deq(buf_t *recp)
{
	if (logevt.count == 0)
		return(-1);

	pthread_mutex_lock(logevt.lockp);

	*recp = *(logevt.deq);
	logevt.count--;

	if (logevt.deq == logevt.last)
		logevt.deq = logevt.first;
	else
		logevt.deq++;

	pthread_mutex_unlock(logevt.lockp);
	
	return(0);
	
}

/**
 * print_evts - dequeue all logger elements and write to stdout
 *
 */
#define TV_FMT "%ld.%06ld"
void print_evts(void) {
	buf_t rec;
	int idx = 0;
	char evtstr[8];

	printf("dumping log\n");

	/* loop until all events are dequeued
	 * starting from oldest and ending at newest
	 */
	while (0 == evt_deq(&rec)) {
		
		/* convert record id (event type enum) to a string */
		switch(rec.id) {
		case EVT_ENQ:
			strcpy(evtstr, "enq");
			break;
		case EVT_DEQ:
			strcpy(evtstr, "deq");
			break;
		case EVT_DEQ_IDLE:
			strcpy(evtstr, "idle");
			break;
		default:
			strcpy(evtstr, "???");
			break;
		}

		printf("%d: %s val=%d time=" TV_FMT "\n",
		       idx++,
		       evtstr,
		       rec.val,
		       rec.tstamp.tv_sec,
		       rec.tstamp.tv_nsec);
	}
	printf("done\n");
}

