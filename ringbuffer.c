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
#define INVALID_ENTRY (0xffffffff)

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
 * buf_cb - helper function to display a queue bufs array element
 * @buf - pointer to bufs array element
 */
inline static void buf_cb(const buf_t *buf) {
	printf("%d ", *buf);
}

#define QDEPTH 12

/**
 * struct sq - simple queue
 * @bufs: fix array of buffers
 * @enq: pointer to the bufs entry to fill for the newest entry,
 *       the entry will either be invalid or, if valid, be the
 *       oldest filled.
 * @deq: pointer to the bufs entry to drain next,
 *       always the oldest entry.
 * @count: the number of bufs entries containing data
 * @lockp: a mutex for critical sections
 * @first: pointer to the first bufs entry
 * @last: pointer to the last bufs entry
 * @max: the number of total bufs in the array.
 * @cb: debug callback
 *
 * This is the main simple queue structure.  It is instantiated
 * once for each queue.  The bufs array fills and empties going higher. If the
 * bufs array fills to the last entry then it loops back to the first entry and
 * overwrites the oldest entries.
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
	.bufs[0]=INVALID_ENTRY, .bufs[1]=INVALID_ENTRY, .bufs[4]=INVALID_ENTRY,
	.enq = rb_test.bufs,
	.deq = rb_test.bufs,
	.count = 0,

	.lockp = &sq_mutex,
	.first = rb_test.bufs,
	.last = &rb_test.bufs[ARRAY_SIZE(rb_test.bufs)-1],
	.max = ARRAY_SIZE(rb_test.bufs),
	.cb = buf_cb,
};

/**
 * q_print: display the all bufs in the queue
 * @label: an informational string used to identify the queue state
 * @sqp: the simple queue context structure
 *
 * Start with first bufs entry assigned to a tmp variable
 * print the entry value
 * increment tmp until last bufs entry
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
 * q_enq: enqueue a new value into the oldest ringbuffer entry
 * @sqp: the simple queue context structure
 * @val: value to enter into current bufs element
 *
 * Logic:
 * - If overwriting an oldest valid element then move the deq pointer to next
 * oldest entry
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
	 * then move deq to next oldest before overwriting.
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
	 * otherwise increment to next bufs entry
	 */
	if (sqp->enq == sqp->last)
		sqp->enq = sqp->first;
	else
		sqp->enq++;

	pthread_mutex_unlock(sqp->lockp);

	evt_enq(EVT_ENQ, val);
}

/**
 * q_deq: dequeue the oldest ringbuffer entry
 * @sqp: the simple queue context structure
 * @valp: return the value in the current deq entry
 *
 * Logic:
 * - If no entries, return -1
 * - get value from bufs element
 * - mark queue element as invalid and decrement counter
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

	/* set bufs entry to an invalid dec count because bufs entry can be reused now */
	*(sqp->deq) = INVALID_ENTRY;
	sqp->count--;

	if (sqp->deq == sqp->last)
		sqp->deq = sqp->first;
	else
		sqp->deq++;

	pthread_mutex_unlock(sqp->lockp);

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

/**
 * q_ut - single thread unit test function to exercise ring buffer
 */
int q_ut()
{
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */
	buf_t val;

	if (debug_flag)
		q_print("static init", &rb_test);

	/* dynamic init of rb
	 * bytewise clear the buffer array
	 */
	if (debug_flag) {
		memset(rb_test.bufs, 0, sizeof(rb_test.bufs));
		q_print("dynamic init", &rb_test);
	}

	/* enq three bufs and print array */
	for (int i=0; i<3; i++)
		q_enq(&rb_test, base_idx+i);
	q_print("enq 3", &rb_test);

	/* reset bufs */
	deq_all(&rb_test);

	/* enq and then deq a single entry */
	base_idx += 10;
	q_enq(&rb_test, base_idx+1);
	q_print("enq 1", &rb_test);	
	if (0 == q_deq(&rb_test, &val))
		printf("deq oldest val=%d\n", val);
	q_print("deq 1", &rb_test);	

	/* enq so bufs wraps */
	base_idx += 10;
	for (int i=0; i<24; i++)
		q_enq(&rb_test, base_idx+i);
	q_print("enq 24 wrap", &rb_test);

	/* deq the oldest three entries */
	for (int i=0; i<3; i++) {
		if (0 == q_deq(&rb_test, &val))
			printf("oldest=%d ", val);
	}
	q_print("deq 3", &rb_test);

	/* enq three */
	base_idx += 10;
	for (int i=0; i<3; i++)
		q_enq(&rb_test, base_idx+i);
	q_print("enq 3", &rb_test);

	/* enq four */
	base_idx += 10;
	for (int i=0; i<4; i++)
		q_enq(&rb_test, base_idx+i);
	q_print("enq 4", &rb_test);

	/* reset bufs */
	deq_all(&rb_test);
	
	/* test enq quickly and deq slowly */
	for (int j=0; j<8; j++) {
		base_idx += 10;
		for (int i=0; i<5; i++)
			q_enq(&rb_test, base_idx+i);
		q_print("enq 5", &rb_test);
		if (0 == q_deq(&rb_test, &val))
			printf("oldest val=%d\n", val);
		else
			printf("warning, no entries\n");
	}

	deq_all(&rb_test);
}

void* q_producer(void *arg) {
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */

	pthread_barrier_wait(&barrier);
	printf("starting producer\n");

	/* enq bufs */
	for (int i=1; i<4; i++) {
		q_enq(&rb_test, base_idx+i);
	}

	base_idx += 100;
	for (int i=0; i<20; i++) {
		q_enq(&rb_test, base_idx+i);
	}

	// usleep(1); 	/* relax for 1us */

	base_idx += 100;
	for (int i=0; i<3; i++) {
		q_enq(&rb_test, base_idx+i);
	}

	/* 1ms to allow consumer to drain and then handle end flag */
	usleep(1e3);
	
	/* send an end flag value to end consumer lupe */
	q_enq(&rb_test, 0xdead);
}

void* q_consumer(void *arg) {
	int done = 0;	
	buf_t val;
	int idx = 0;
	int idlecnt = 0;
	
	pthread_barrier_wait(&barrier);
	printf("starting consumer\n");

	/* race condition wait until producer enqueues something 
	   cannot busy-wait because will spin
	 */
	while (q_deq(&rb_test, &val)) {
		idlecnt++;
	}

	while (!done) {
		idlecnt = 0;
		if (0 == q_deq(&rb_test, &val)) {
			if (val == 0xdead)
				done = 1;
			else {
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

	/* single thread testing
	q_ut();
	*/

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
