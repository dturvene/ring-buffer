/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2022 Dahetral Systems
 * Author: David Turvene (dturvene@gmail.com)
 *
 * Use a simple static queue to implement a ringbuffer.
 *
 * Inline doc base on
 * https://www.kernel.org/doc/html/v5.1/doc-guide/kernel-doc.html
 * 
 */

#include <stdlib.h>     /* atoi, malloc, strtol, strtoll, strtoul, exit, EXIT_FAILURE */
#include <stdio.h>      /* char I/O */
#include <unistd.h>     /* getpid, common typdefs, e.g. ssize_t, includes getopt.h */
#include <string.h>     /* strlen, strsignal,, memset, bzero_explicit */
#include <stdint.h>     /* uint32_t, etc. */

/* commandline args */
char *arguments = "\n"				\
	" -d: set debug_flag\n"			\
	" -h: this help\n"			\
	;

static uint32_t debug_flag = 0;

#define ARRAY_SIZE(arr)  (sizeof(arr)/sizeof(arr[0]))
#define INVALID_ENTRY (-1)

typedef uint32_t buf_t;
inline static void buf_cb(const buf_t *buf) {
	printf("%d ", *buf);
}

#define QDEPTH 12

/**
 * struct sq - simple queue
 * @bufs: fix array of buffers
 * @cb: display callback
 * @first: pointer to the first bufs entry
 * @last: pointer to the last bufs entry
 * @enq: pointer to the bufs entry to fill for the newest entry,
 *       the entry will either be invalid or, if valid, be the
 *       oldest filled.
 * @deq: pointer to the bufs entry to drain next,
 *       always the oldest entry.
 * @count: the number of bufs entries containing data
 * @max: the number of total bufs in the array.
 *
 * This is the main simple queue structure.  It is instantiated
 * once for each queue.  The bufs array fills and empties going higher. If the
 * array fills to the last entry then it loops back to the first entry and
 * overwrites the oldest entries.
 */
typedef struct sq {
	buf_t bufs[QDEPTH];
	void (*cb)(const buf_t *);
	buf_t *first;
	buf_t *last;
	buf_t *enq;
	buf_t *deq;
	buf_t count;
	buf_t max;
} sq_t;

/**
 * rb_test - test ring buffer using sq_t
 *
 * Initial values are defined at compile time.
 * 
 * NOTE: better to create on local stack if ringbuffer is only accessible to a
 * function/subfunction.
 */
static sq_t rb_test = {
	.bufs[0]=INVALID_ENTRY, .bufs[1]=INVALID_ENTRY, .bufs[4]=INVALID_ENTRY, /* static init some of the bufs */
	.cb = buf_cb,
	.first = rb_test.bufs,
	.last = &rb_test.bufs[ARRAY_SIZE(rb_test.bufs)-1],
	.enq = rb_test.bufs,
	.deq = rb_test.bufs,
	.count = 0,
	.max = ARRAY_SIZE(rb_test.bufs),
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
 * @val: value to enter into current bufs entry
 *
 * Logic:
 * - If overwriting an oldest valid entry then move the deq pointer to next
 * oldest entry.
 * - update value
 * - if bufs not full (max) then update count
 * - move to next bufs entry
 */
void q_enq(sq_t* sqp, buf_t val)
{
	if (debug_flag)
		printf("q_enq enq=%p val=%u deq=%p val=%u\n", sqp->enq, *(sqp->enq), sqp->deq, *(sqp->deq));

	/* if enq (newest) is about to overwrite the deq (oldest) location
	 * then move deq to next oldest before overwriting.
	 */
	if (sqp->count == sqp->max && sqp->enq == sqp->deq) {
		if (debug_flag)
			printf("enq overwriting deq val=%d with %d\n", *(sqp->deq), val);
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
}

/**
 * q_deq: dequeue the oldest ringbuffer entry
 * @sqp: the simple queue context structure
 * @valp: return the value in the current deq entry
 *
 * Logic:
 * - If 
 * - If overwriting an oldest valid entry then move the deq pointer to next
 * oldest entry.
 * - update value
 * - if bufs not full (max) then update count
 * - move to next bufs entry
 * Return:
 *   0 for success, negative otherwise
 */
int q_deq(sq_t* sqp, buf_t *valp)
{
	if (debug_flag)
		printf("q_deq ep=%p val=%u dp=%p val=%u\n", sqp->enq, *(sqp->enq), sqp->deq, *(sqp->deq));

	/* if no valid entries, return error */
	if (sqp->count == 0)
		return(-1);

	/* fill value with bufs entry data */
	*valp = *(sqp->deq);

	/* set bufs entry to an invalid dec count because bufs entry can be reused now */
	*(sqp->deq) = INVALID_ENTRY;
	sqp->count--;

	if (sqp->deq == sqp->last)
		sqp->deq = sqp->first;
	else
		sqp->deq++;

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
 * ut_q - unit test function to exercise ring buffer
 */
int ut_q()
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

int main(int argc, char *argv[])
{
	int opt;

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

	ut_q();
}
