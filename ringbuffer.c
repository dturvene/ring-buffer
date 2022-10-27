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
#include <stdatomic.h>  /* atomic_ operations */
#include "logevt.h"     /* event logging */

/* commandline args */
char *cmd_arguments = "\n"					\
	" -t id: test id to run\n"				\
	" -m: use mutex (default spinlock)"			\
	" -c cnt: cnt events to enq (default 10000)"		\
	" -d: set debug_flag\n"					\
	" -h: this help\n"					\
	;

static uint32_t debug_flag = 0;
static uint32_t testid = 0;
static uint32_t mutex_flag = 0;
static uint32_t cnt_events = 10000;

/* if set then use internal logger to save test events and later dump them */
#define DEBUG_LOGGING 1

#define ARRAY_SIZE(arr)  (sizeof(arr)/sizeof(arr[0]))
#define INVALID_EL (0xffffffff)

/* 
 * Producer sends this element immediately before exitting
 * Consumer deq this element and exits
 * This must be large!
 */
#define END_EL (0xdeadbeef)

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
 * nap - small sleep
 * @ms: number of millisecs to sleep
 *
 * useful for very small delays, causes thread switch.
 * See man:nanosleep for signal problems, maybe convert to 
 */
inline static void nap(uint32_t ms)
{
	struct timespec t;

	t.tv_sec = (ms/1000);
	t.tv_nsec = (ms%1000)*1e6;
	nanosleep(&t, NULL);
}

/** 
 * buf_debug - helper function to display a queue bufs array element
 * @buf - pointer to bufs element
 */
inline static void buf_debug(const buf_t *buf) {
	printf("%d ", *buf);
}

/* Fixed size of the array used for queuing */
#define QDEPTH 4096

/**
 * struct sq - simple queue
 * @bufs: fix array of buffers
 * @enq: pointer to the bufs element to fill for the newest value,
 *       the element will either be invalid or, if valid, the
 *       oldest filled.
 * @deq: pointer to the bufs element to drain next,
 *       always the oldest element.
 * @count: the number of bufs elements containing data
 * @first: pointer to the first bufs element
 * @last: pointer to the last bufs element
 * @max: the number of total bufs in the array.
 * @cb: debug callback
 *
 * This is the main simple queue structure.  It is instantiated
 * once for each queue.  The bufs array fills and empties going higher. If the
 * bufs array fills to the last element then it loops back to the first element
 * and starts to overwrite the oldest elements.
 *
 * This structure does not include concurrency mechanisms.  See 
 */
typedef struct sq {
	buf_t bufs[QDEPTH];
	buf_t *enq;
	buf_t *deq;
	buf_t count;

	buf_t *first;
	buf_t *last;
	buf_t max;
	void (*cb)(const buf_t *);
} sq_t;

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

#define BARRIER 0

#if BARRIER
/**
 * barrier - pthread barrier to start pthreads at roughly the same time
 *
 * This is created and used when the BARRIER define is set, otherwise all
 * uses are removed from code.
 */
pthread_barrier_t barrier;
#endif

static pthread_mutex_t sq_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 
 * lock_t - type for the spinlock bit array
 * C11 spec says to use an atomic for atomic lock value
 */
typedef atomic_uint lock_t;

/**
 * lockholder - bit array marking the thread holding the spinlock
 *
 * This will be 0 if no thread holds lock, otherwise ONE of the defined lock
 * bits: LOCK_C for consumer and LOCK_P for producer.  Using a bitarray allows
 * for better metrics in the lock function 
 */
lock_t lockholder = 0;
#define LOCK_C 0x01
#define LOCK_P 0x02

/*
 * lock_held_c, lock_held_p: metrics when trying to lock, indicating thread
 * currently holding the lock. Each is a counter incremented in the lock
 * function when lock acquire fails.
 */
static atomic_int lock_held_c, lock_held_p;

/**
 * lock - try to acquire lock atomically, spin until achieved
 * @bitarrayp: pointer to lock bitarray
 * @desired: bit value used to update lock
 *
 * Loop, testing for all lock bits cleared.  The current value is returned in
 * expected, which can then be used to updates metrics about which thread
 * currently holds the lock.
 * When the lock is cleared, atomically update it to the desired holder.
 * 
 * Uses the gcc 7.5+ implementation of atomic_compare_exchange_weak 
 * defined in
 * https://en.cppreference.com/w/c/atomic/atomic_compare_exchange
 * 
 */
void
lock(lock_t *bitarrayp, uint32_t desired)
{
	uint32_t expected = 0; /* lock is not held */

	/* the value in expected is updated if it does not match
	 * the value in bitarrayp.  If the comparison fails then compare
	 * the returned value with the lock bits and update the appropriate
	 * counter.
	 */
	do {
		if (expected & LOCK_P) lock_held_p++;
		if (expected & LOCK_C) lock_held_c++;
		expected = 0;
#if 1
	} while(!atomic_compare_exchange_weak(bitarrayp, &expected, desired));
#else
	/* Try different memory models from
	   /usr/lib/gcc/x86_64-linux-gnu/7/include/stdatomic.h 
	   memory model: SUC, FAIL
	   __ATOMIC_SEQ_CST
	   __ATOMIC_RELEASE
	   __ATOMIC_ACQUIRE
	   __ATOMIC_ACQ_REL: invalid for call
	 */
} while(!atomic_compare_exchange_weak_explicit(bitarrayp,
					       &expected,
					       desired,
					       __ATOMIC_ACQUIRE,
					       __ATOMIC_ACQUIRE
		));
#endif
}

/**
 * release - clear the bitarray, making the lock available to be acquired.
 * @bitarrayp: pointer to lock bitarray
 *
 * The current thread will have its bit set in the lock variable and be
 * the holder of the lock.  This call effectively releases the lock.
 * Note that *bitarrayp is of type lock_t, which is a C11 atomic.
 */
void
release(lock_t *bitarrayp)
{
	*bitarrayp = 0;
}

/**
 * q_enq: enqueue a new value into the oldest ringbuffer element
 * @sqp: the simple queue context structure
 * @val: value to enter into current bufs element
 *
 * Logic:
 * - update element value and wrap or increment enq pointer
 * - if all bufs are being used then move the deq pointer to the
 *   current oldest (one more than the newest!), 
 *   if bufs still available then increment buf count
 */
void
q_enq(sq_t* sqp, buf_t val)
{
	/* command line argument to determine if mutex lock or spinlock */
	if (mutex_flag) {
		pthread_mutex_lock(&sq_mutex);
	} else {
		lock(&lockholder, LOCK_P);
	}

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


#if DEBUG_LOGGING
	/* log event before releasing lock.  This makes the critical section
	 * longer but logs the event accurately; outside of the critical
	 * section will result in out-of-sequence events being logged.
	 */
	evt_enq(EVT_ENQ, val);
#endif

	/* command line argument to determine if mutex lock or spinlock */
	if (mutex_flag) {
		pthread_mutex_unlock(&sq_mutex);
	} else {
		release(&lockholder);
	}
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
int
q_deq(sq_t* sqp, buf_t* valp)
{
	/* if no valid entries, return error */
	if (sqp->count == 0)
		return(-1);
	
	/* command line argument to determine if mutex lock or spinlock */
	if (mutex_flag) {
		pthread_mutex_lock(&sq_mutex);
	} else {
		lock(&lockholder, LOCK_C);
	}
	
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

#if DEBUG_LOGGING
	/* log event before releasing lock.  This makes the critical section
	 * longer but logs the event accurately; outside of the critical
	 * section will result in out-of-sequence events being logged.
	 */
	evt_enq(EVT_DEQ, *valp);
#endif

	/* command line argument to determine if mutex lock or spinlock */
	if (mutex_flag) {
		pthread_mutex_unlock(&sq_mutex);
	} else {
		release(&lockholder);
	}

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
 * NOTES: 
 * I experimented with allowing the thread to relax (short sleep) after 
 * enq but that just seemed to unnecessarily slow down operations.
 * I experimented with a pthread barrier wait so producer/consumer start at roughly
 * the same time but this appears to be unnecesasry.
 *
 * Return: NULL
 */
void*
q_producer_ut(void *arg) {
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */
	void (*fnenq)(sq_t*, buf_t) = q_enq;

#if BARRIER
	pthread_barrier_wait(&barrier);
#endif
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

/*
 * q_producer_empty - a minimal test of the producer/consumer
 *
 * See q_producer for doc.
 */
void
*q_producer_empty(void *arg) {
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */

#if BARRIER
	pthread_barrier_wait(&barrier);
#endif
	printf("%s: starting\n", __FUNCTION__);

	/* single enq to start consumer */
	q_enq(&rb_test, 1);

	/* end of enqueue */
	q_enq(&rb_test, END_EL);

	return (NULL);	     
}


/**
 * q_producer_stress2 - a relatively short stress test of the ringbuffer
 *
 * See q_producer for doc.
 */
void
*q_producer_stress2(void *arg) {
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */

#if BARRIER
	pthread_barrier_wait(&barrier);
#endif
	printf("%s: starting\n", __FUNCTION__);

	/* stress enq loop */
	for (int j=0; j<20; j++) {
		for (int i=1; i<QDEPTH; i++)
			q_enq(&rb_test, base_idx+i);
		base_idx += 100;
	}

	/* make inner loop bigger */
	for (int j=0; j<20; j++) {
		for (int i=1; i<128; i++)
			q_enq(&rb_test, base_idx+i);
		// nap(1);
		base_idx += 100;
	}

	/* end of enqueue */
	q_enq(&rb_test, END_EL);

	return (NULL);	     
}

/*
 * q_producer_stress3 - a long stress test of the ringbuffer
 *
 * See q_producer for doc.
 */
void
*q_producer_stress3(void *arg) {
	int base_idx = 0;  /* a unique number to differentiate q_enq entries */

	fprintf(stderr, "%s: send %u\n", __FUNCTION__, cnt_events);

	/* stress enq loop */
	for (int i=0; i<cnt_events; i++) {
		q_enq(&rb_test, base_idx+i);
	}

	/* end of enqueue */
	q_enq(&rb_test, END_EL);

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
 * When the consumer thread starts before the producer it busy-waits
 * until the first value is written by the producer.
 * 
 * NOTE: I experimented with allowing the thread to relax (short sleep) after 
 * deq but that just seemed to unnecessarily slow down operations.
 * NOTES: 
 * I experimented with allowing the thread to relax (short sleep) after 
 * enq but that just seemed to unnecessarily slow down operations.
 * I experimented with a pthread barrier wait so producer/consumer start at roughly
 * the same time but this appears to be unnecesasry.
 *
 */
void*
q_consumer(void *arg) {
	int done = 0;
	buf_t val;
	int idx = 0;
	int idlecnt = 0;
	int (*fndeq)(sq_t*, buf_t*) = q_deq; /* use a fn pointer for easy management */

#if BARRIER
	pthread_barrier_wait(&barrier);
#endif
	printf("%s: starting\n", __FUNCTION__);

	/* race condition busy-wait until producer enqueues something 
	 */
	while (fndeq(&rb_test, &val)) {
		idlecnt++;
	}

#if DEBUG_LOGGING	
	/* log how many idle loops before producer starts writing to queue */
	evt_enq(EVT_DEQ_IDLE, idlecnt);
#endif

	idlecnt = 0;
		
	/* loop until the producer sends the END element */
	while (!done) {

		if (0 == fndeq(&rb_test, &val)) {
			if (val == END_EL)
				done = 1;
			else {
#if DEBUG_LOGGING				
				/* log how many idle loops before a new element
				 * is written by producer 
				 */
				if (idlecnt > 0)
					evt_enq(EVT_DEQ_IDLE, idlecnt);
#endif
				idlecnt = 0;
			}
		} else {
			idlecnt++;
		}
	}

	fprintf(stderr, "%s: exiting\n", __FUNCTION__);
	return (NULL);
}

int main(int argc, char *argv[])
{
	int opt;
	pthread_t producer, consumer;
	void* (*fn_producer)(void *arg);
	void* (*fn_consumer)(void *arg);


	/* handle commandline options (see usage) */
	while((opt = getopt(argc, argv, "t:c:dmh")) != -1) {
		switch(opt) {
		case 't':
			testid = strtol(optarg, NULL, 0);
			break;
		case 'd':
			debug_flag = 1;
			break;
		case 'm':
			mutex_flag = 1;
			break;
		case 'c':
			cnt_events = strtol(optarg, NULL, 0);
			break;
		case 'h':
		default:			
			fprintf(stderr, "Usage: %s %s\n", argv[0], cmd_arguments);
			exit(0);
		}
	}

	switch(testid) {
	case 1:	fn_producer = q_producer_empty;	break;
	case 2:	fn_producer = q_producer_stress2; break;
	case 3:	fn_producer = q_producer_stress3; break;
	default: fn_producer = q_producer_ut; break;
	}
	/* queue consumer is generic for all tests */
	fn_consumer = q_consumer;


#if BARRIER
	/* multithread, separate producer and consumer threads 
	   use a barrier to start them at the same time
	 */
	if (0 != pthread_barrier_init(&barrier, NULL, 2))
		die("pthread_barrier_init");
#endif

	ts_start();
	if (0 != pthread_create(&producer, NULL, fn_producer, NULL))
		die("pthread_create");

	if (0 != pthread_create(&consumer, NULL, fn_consumer, NULL))
		die("pthread_create");

	/* wait for threads to exit */
	pthread_join(producer, NULL);
	pthread_join(consumer, NULL);
	ts_end();

	/* dump all event log records to stdout */
	print_evts();

	fprintf(stderr, "elapsed time from thread create after thread join: %s\n", ts_delta());
	fprintf(stderr, "lock_held_c=%d lock_held_p=%d\n", lock_held_c, lock_held_p);
}
