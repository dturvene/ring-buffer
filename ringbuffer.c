/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2022 Dahetral Systems
 * Author: David Turvene (dturvene@gmail.com)
 *
 * Use a simple static queue to implement a ringbuffer.
 */

#include <stdlib.h>     /* atoi, malloc, strtol, strtoll, strtoul, exit, EXIT_FAILURE */
#include <stdio.h>      /* char I/O, perror */
#include <unistd.h>     /* getpid, usleep, 
			   common typdefs, e.g. ssize_t, includes getopt.h */ 
#include <string.h>     /* strlen, strsignal,, memset, bzero_explicit */
#include <stdint.h>     /* uint32_t, etc. */
#include <stdlib.h>     /* exit */
#include <pthread.h>    /* pthread_mutex, pthread_barrier */
#include "logevt.h"     /* event logging */

/* commandline args */
char *arguments = "\n"				\
	" -d: set debug_flag\n"			\
	" -h: this help\n"			\
	;

static uint32_t debug_flag = 0;

#define ARRAY_SIZE(arr)  (sizeof(arr)/sizeof(arr[0]))
#define INVALID_EL (0xffffffff)
#define END_EL (0xdead)

typedef uint32_t buf_t;

/**
 * die - helper function to stop immediately
 * @msg - an informational string to identify where program failed
 *
 * See man:perror
 */
inline static void die(const char* msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

/** 
 * buf_debug - helper function to display a queue bufs array element
 * @buf - pointer to bufs element
 */
inline static void buf_debug(const buf_t *buf) {
	printf("%d ", *buf);
}

#define QDEPTH 12

/**
 * struct sq - simple queue
 * @bufs: fix array of buffers
 * @enq: pointer to the bufs element to fill for the newest value,
 *       the element will either be invalid or, if valid, the
 *       oldest filled.
 * @deq: pointer to the bufs element to drain next,
 *       always the oldest element.
 * @count: the number of bufs elements containing data
 * @lockp: a mutex for critical sections
 * @first: pointer to the first bufs element
 * @last: pointer to the last bufs element
 * @max: the number of total bufs in the array.
 * @cb: debug callback
 *
 * This is the main simple queue structure.  It is instantiated
 * once for each queue.  The bufs array fills and empties going higher. If the
 * bufs array fills to the last element then it loops back to the first element
 * and starts to overwrite the oldest elements.
 */
typedef struct sq {
	buf_t bufs[QDEPTH];
	buf_t *enq;
	buf_t *deq;
	buf_t count;

	pthread_mutex_t *lockp;
	buf_t *first;
	buf_t *last;
	buf_t max;
	void (*cb)(const buf_t *);
} sq_t;

pthread_mutex_t sq_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;

/**
 * rb_test - test ring buffer using sq_t
 *
 * Initial values can be defined at compile time but can also be dynamically set
 * before the first enqueue operation.
 * 
 * NOTE: better to create on local stack if ringbuffer is only accessible to a
 * function/subfunction.
 */
static sq_t rb_test = {
	.bufs[0]=INVALID_EL, .bufs[1]=INVALID_EL, .bufs[4]=INVALID_EL,
	.enq = rb_test.bufs,
	.deq = rb_test.bufs,
	.count = 0,

	.lockp = &sq_mutex,
	.first = rb_test.bufs,
	.last = &rb_test.bufs[ARRAY_SIZE(rb_test.bufs)-1],
	.max = ARRAY_SIZE(rb_test.bufs),
	.cb = buf_debug,
};

/**
 * q_print: display the all bufs in the queue
 * @label: an informational string used to identify the queue state
 * @sqp: the simple queue context structure
 *
 * Start with first bufs element assigned to a tmp variable
 * print the element value
 * increment tmp until last bufs element
 */
void q_print(const char* label, const sq_t* sqp)
{
	const buf_t *tmp = sqp->first;
	
	printf("%s count=%d bufs=%p enq=%p deq=%p\n", label, sqp->count, sqp->bufs, sqp->enq, sqp->deq);
	do {
		sqp->cb(tmp);
	} while (tmp++ != sqp->last);
	printf("\n");
}

/**
 * q_enq: enqueue a new value into the oldest ringbuffer element
 * @sqp: the simple queue context structure
 * @val: value to enter into current bufs element
 *
 * Logic:
 * - If overwriting an oldest valid element then move the deq pointer to next
 * oldest ele,ment
 * - update element value
 * - if bufs not full (max) then update count
 * - if last element then wrap to first, otherwise move to next element
 */
void q_enq(sq_t* sqp, buf_t val)
{
	if (debug_flag)
		printf("q_enq enq=%p val=%d deq=%p val=%d\n", sqp->enq,
		       *(sqp->enq),
		       sqp->deq,
		       *(sqp->deq));

	pthread_mutex_lock(sqp->lockp);
	
	/* if enq (newest) is about to overwrite the deq (oldest) location
	 * then move deq to next oldest before overwriting, if last element
	 * then move deq to first element.
	 */
	if (sqp->count == sqp->max && sqp->enq == sqp->deq) {
		if (debug_flag)
			printf("enq overwriting deq val=%d with %d\n",
			       *(sqp->deq), val);
		if (sqp->deq == sqp->last)
			sqp->deq = sqp->first;
		else
			sqp->deq++;
	}

	*(sqp->enq) = val;

	/* if count is less than buf array size, increment
	 * otherwise all buffers are being used and enq is overwriting
	 * existing buffers.
	 */
	if (sqp->count < sqp->max)
		sqp->count++;

	/* if last bufs, set to first
	 * otherwise increment to next bufs element
	 */
	if (sqp->enq == sqp->last)
		sqp->enq = sqp->first;
	else
		sqp->enq++;

	pthread_mutex_unlock(sqp->lockp);

	/* log event for non-intrusive debugging */	
	evt_enq(EVT_ENQ, val);
}

/**
 * q_deq: dequeue the oldest ringbuffer element
 * @sqp: the simple queue context structure
 * @valp: return the value in the current deq element
 *
 * Logic:
 * - If no valid elements, return -1
 * - get value from bufs element
 * - mark queue element as invalid (for debugging) and decrement counter
 * - if last element then wrap to first, otherwise move to next element
 * 
 * Return:
 *   0 for success, negative otherwise
 */
int q_deq(sq_t* sqp, buf_t *valp)
{
	if (debug_flag)
		printf("q_deq count=%d ep=%p val=%d dp=%p val=%d\n",
		       sqp->count, sqp->enq, *(sqp->enq), sqp->deq, *(sqp->deq));

	/* if no valid entries, return error */
	if (sqp->count == 0)
		return(-1);

	pthread_mutex_lock(sqp->lockp);

	/* fill value with bufs entry data */
	*valp = *(sqp->deq);

	/* set bufs element to invalid for debugging */
	if (debug_flag)
		*(sqp->deq) = INVALID_EL;

	/* dec count because bufs element can be reused now */
	sqp->count--;

	/* if last bufs element, set to first
	 * otherwise increment to next bufs element
	 */
	if (sqp->deq == sqp->last)
		sqp->deq = sqp->first;
	else
		sqp->deq++; 

	pthread_mutex_unlock(sqp->lockp);

	/* log event for non-intrusive debugging */
	evt_enq(EVT_DEQ, *valp);

	return(0);
}

/**
 * deq_all - helper function to deq all valid bufs entries
 * @sqp - pointer to the ring buffer struct
 *
 * loop calling q_deq until it fails
 * print ring buffer showing it empty
 */
int deq_all(sq_t* sqp)
{
	buf_t val;
	
	printf("deq all: ");
	while (0 == q_deq(&rb_test, &val)) {
		printf("%d ", val);
	}
	printf("\n");
	q_print("deq empty", &rb_test);
}

void* q_producer(void *arg) {
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */

	pthread_barrier_wait(&barrier);
	printf("starting producer\n");

	/* enq bufs */
	for (int i=1; i<4; i++) {
		q_enq(&rb_test, base_idx+i);
#ifdef DELAY
		usleep(1); 	/* relax for 1us */
#endif
	}

	base_idx += 100;
	for (int i=0; i<20; i++) {
		q_enq(&rb_test, base_idx+i);
#ifdef DELAY
		usleep(1); 	/* relax for 1us */
#endif
	}

	base_idx += 100;
	for (int i=0; i<3; i++) {
		q_enq(&rb_test, base_idx+i);
#ifdef DELAY
		usleep(1); 	/* relax for 1us */
#endif
	}

	/* send an end flag value to end consumer lupe */
	q_enq(&rb_test, END_EL);
}

void* q_consumer(void *arg) {
	int done = 0;
	buf_t val;
	int idx = 0;
	int idlecnt = 0;
	
	pthread_barrier_wait(&barrier);
	printf("starting consumer\n");

	/* race condition busy-wait until producer enqueues something 
	 */
	while (q_deq(&rb_test, &val)) {
		idlecnt++;
#ifdef DELAY
		usleep(1); 	/* relax for 1us */
#endif
	}
	/* log how many idle loops before producer starts writing to queue */
	evt_enq(EVT_DEQ_IDLE, idlecnt);

	idlecnt = 0;

	/* loop until the producer sends the END element */
	while (!done) {

		if (0 == q_deq(&rb_test, &val)) {
			if (val == END_EL)
				done = 1;
			else {
				/* log how many idle loops before a new element
				 * is written by producer 
				 */
				if (idlecnt > 0)
					evt_enq(EVT_DEQ_IDLE, idlecnt);
				idlecnt = 0;
			}
		} else {
			idlecnt++;
		}
	}
}

int main(int argc, char *argv[])
{
	int opt;
	pthread_t producer, consumer;

	while((opt = getopt(argc, argv, "dh")) != -1) {
		switch(opt) {
		case 'd':
			debug_flag = 1;
			break;
		case 'h':
		default:			
			fprintf(stderr, "Usage: %s %s\n", argv[0], arguments);
			exit(0);
		}
	}

	/* multithread, separate producer and consumer threads 
	   use a barrier to start them at the same time
	 */
	if (0 != pthread_barrier_init(&barrier, NULL, 2))
		die("pthread_barrier_init");

	if (0 != pthread_create(&producer, NULL, q_producer, NULL))
		die("pthread_create");

	if (0 != pthread_create(&consumer, NULL, q_consumer, NULL))
		die("pthread_create");

	/* wait for threads to exit */
	pthread_join(producer, NULL);
	pthread_join(consumer, NULL);

	printf("threads done, printing results\n");
	print_evts();
}
