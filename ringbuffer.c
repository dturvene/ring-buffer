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
	" -t id: test id to run\n"		\
	" -d: set debug_flag\n"			\
	" -h: this help\n"			\
	;

static uint32_t debug_flag = 0;
static uint32_t testid = 0;

#define PROD_CAN_BLOCK

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

#define QDEPTH 8

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
	.bufs[0]=INVALID_EL, .bufs[1]=INVALID_EL, .bufs[3]=INVALID_EL,
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
 * q_enq_block: enqueue a new value into the oldest ringbuffer element, 
 *   block if q_deq_block is running.
 * @sqp: the simple queue context structure
 * @val: value to enter into current bufs element
 *
 * Logic:
 * - update element value and wrap or increment enq pointer
 * - if all bufs are being used then move the deq pointer to the
 *   current oldest (one more than the newest!), 
 *   if bufs still available then increment buf count
 */
void q_enq_block(sq_t* sqp, buf_t val)
{
	pthread_mutex_lock(sqp->lockp);
	if (debug_flag)
		printf("enter enq count=%d val=%d enq=%p deq=%p\n", sqp->count, val, sqp->enq, sqp->deq);

	/* enqueue value into buf */
	*(sqp->enq) = val;

	/* compare to last buf first and then increment to next buf
	 * if match last then set to first
	 * the enq pointer after last is never used
	 */
	if (sqp->enq++ == sqp->last)
		sqp->enq = sqp->first;

	/* When the the array is full, q_enq will be overwriting the oldest buffer
	 * so after enq updates the oldest buffer with the newest datum, 
	 * move the deq pointer to the NEXT oldest.
	 * If the array is not full, then deq will still point to the oldest so just
	 * increment the buffer count.
	 */
	if (sqp->count == sqp->max) {
		if (sqp->deq == sqp->last)
			sqp->deq = sqp->first + 1;
		else
			sqp->deq = sqp->enq + 1;
	} else {
		sqp->count++;
	}
	
	if (debug_flag)
		printf("leave enq count=%d val=%d enq=%p deq=%p\n", sqp->count, val, sqp->enq, sqp->deq);

	pthread_mutex_unlock(sqp->lockp);

	/* log event for non-intrusive debugging, for performance not inside lock but
	 * events may be out-of-sync
	 */
	evt_enq(EVT_ENQ, val);
}

/**
 * q_deq_block: dequeue the oldest ringbuffer element,
 *    block if q_enq_block is running.
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
int q_deq_block(sq_t* sqp, buf_t* valp)
{
	/* if no valid entries, return error */
	if (sqp->count == 0)
		return(-1);

	pthread_mutex_lock(sqp->lockp);
	if (debug_flag)
		printf("q_deq count=%d ep=%p val=%d dp=%p val=%d\n",
		       sqp->count, sqp->enq, *(sqp->enq), sqp->deq, *(sqp->deq));

	/* fill value with bufs entry data */
	*valp = *(sqp->deq);

	/* set bufs element to invalid for debugging */
	if (debug_flag)
		*(sqp->deq) = INVALID_EL;

	/* dec count because bufs element can be reused now */
	sqp->count--;

	/* compare to last buf first and then increment to next buf
	 * if match last then set to first
	 * the enq pointer after last is never used
	 */
	if (sqp->deq++ == sqp->last)
		sqp->deq = sqp->first;

	pthread_mutex_unlock(sqp->lockp);

	/* log event for non-intrusive debugging, performance not inside lock but
	 * events may be out-of-sync
	 */
	evt_enq(EVT_DEQ, *valp);

	return(0);
}

/**
 * q_producer: pthread to call q_enq using a monotonically increasing value
 * @arg: pthread arguments passed from pthread_create (not used)
 *
 * This pthread only enqueues values to the ringbuffer.  The values increase to
 * represent a chronologically order.  When the thread exits, it enqueus an
 * END_EL value for the consumer to recognize there are no more enqueued values. 
 * 
 * The function uses a barrier wait so that producer/consumer start at roughly
 * the same time.
 * 
 * NOTE: I experimented with allowing the thread to relax (short sleep) after 
 * enq but that just seemed to unnecessarily slow down operations.
 */
void* q_producer_ut(void *arg) {
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */
#ifdef PROD_CAN_BLOCK	
	void (*fnenq)(sq_t*, buf_t) = q_enq_block;
#endif

	pthread_barrier_wait(&barrier);
	printf("%s: starting\n", __FUNCTION__);

	/* test enq works before wrapping */
	for (int i=1; i<3; i++)
		fnenq(&rb_test, base_idx+i);

	/* test one loop around the ringbuffer works */
	base_idx += 100;
	for (int i=1; i<QDEPTH; i++)
		(fnenq)(&rb_test, base_idx+i);	
	
	fnenq(&rb_test, END_EL);

	return (NULL);	     
}

void *q_producer_stress(void *arg) {
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */

	pthread_barrier_wait(&barrier);
	printf("%s: starting\n", __FUNCTION__);

	/* test enq works before wrapping */
	for (int i=1; i<3; i++)
		q_enq_block(&rb_test, base_idx+i);

	/* stress enq loop */
	for (int j=0; j<4; j++) {
		base_idx += 100;
		for (int i=1; i<QDEPTH; i++)
			q_enq_block(&rb_test, base_idx+i);	
	}
	
	q_enq_block(&rb_test, END_EL);

	return (NULL);	     
}	

/**
 * q_consumer: pthread to call q_deq
 * @arg: pthread arguments passed from pthread_create (not used)
 *
 * This pthread loops until the END_EL value is received. It trys to dequeue a
 * value. If one is available the function logs it, otherwise it increases and
 * idle counter. After a value is dequeued it will also log the idle counter.
 * 
 * The function uses a barrier wait so that producer/consumer start at roughly
 * the same time.  Occasionally the consumer will start first so it busy-waits
 * until the first value is written by the producer.
 * 
 * NOTE: I experimented with allowing the thread to relax (short sleep) after 
 * deq but that just seemed to unnecessarily slow down operations.
 */
void* q_consumer(void *arg) {
	int done = 0;
	buf_t val;
	int idx = 0;
	int idlecnt = 0;
#ifdef PROD_CAN_BLOCK	
	int (*fndeq)(sq_t*, buf_t*) = q_deq_block;
#endif	
	
	pthread_barrier_wait(&barrier);
	printf("%s: starting\n", __FUNCTION__);	

	/* race condition busy-wait until producer enqueues something 
	 */
	while (fndeq(&rb_test, &val)) {
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

		if (0 == fndeq(&rb_test, &val)) {
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

	return (NULL);
}

int main(int argc, char *argv[])
{
	int opt;
	pthread_t producer, consumer;
	void* (*fn_producer)(void *arg);
	void* (*fn_consumer)(void *arg);

	/* handle commandline options (see usage) */
	while((opt = getopt(argc, argv, "t:dh")) != -1) {
		switch(opt) {
		case 't':
			testid = strtol(optarg, NULL, 0);
			break;
		case 'd':
			debug_flag = 1;
			break;
		case 'h':
		default:			
			fprintf(stderr, "Usage: %s %s\n", argv[0], arguments);
			exit(0);
		}
	}

	switch(testid) {
	case 1:
		fn_producer = q_producer_stress;
		fn_consumer = q_consumer;
		break;
	default:
		fn_producer = q_producer_ut;
		fn_consumer = q_consumer;
		break;
	}

	/* multithread, separate producer and consumer threads 
	   use a barrier to start them at the same time
	 */
	if (0 != pthread_barrier_init(&barrier, NULL, 2))
		die("pthread_barrier_init");

	if (0 != pthread_create(&producer, NULL, fn_producer, NULL))
		die("pthread_create");

	if (0 != pthread_create(&consumer, NULL, fn_consumer, NULL))
		die("pthread_create");

	/* wait for threads to exit */
	pthread_join(producer, NULL);
	pthread_join(consumer, NULL);

	printf("threads done, printing results\n");

	/* dump all event log records to stdout */
	print_evts();
}
