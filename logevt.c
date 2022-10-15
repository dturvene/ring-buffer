#include <stdio.h>      /* char I/O, perror */
#include <time.h>       /* clock_gettime */
#include <stdint.h>     /* uint32_t, etc. */
#include "logevt.h"

#define ARRAY_SIZE(arr)  (sizeof(arr)/sizeof(arr[0]))

static qlog_t logevt = {
	.enq = logevt.bufs,
	.deq = logevt.bufs,
	.count = 0,

	.first = logevt.bufs,
	.last = &logevt.bufs[ARRAY_SIZE(logevt.bufs)-1],
	.max = ARRAY_SIZE(logevt.bufs),
	.cb = NULL,
};

void evt_enq(evtid_t id, uint32_t val) {
	logevt.enq->id = id;
	logevt.enq->val = val;
	clock_gettime(CLOCK_MONOTONIC, &(logevt.enq->tstamp));

	if (logevt.count < logevt.max)
		logevt.count++;

	logevt.enq++;
}

int evt_deq(logrec_t *recp) {
	if (logevt.count == 0)
		return(-1);

	*recp = *(logevt.deq);

	logevt.count--;
	
	logevt.deq++;

	return(0);
	
}

#define TV_FMT "%ld.%06ld"
void print_evts(void) {
	logrec_t rec;
	int idx = 0;

	printf("dumping log\n");
	while (0 == evt_deq(&rec)) {
		printf("%d: %s val=%d time=" TV_FMT "\n",
		       idx++,
		       rec.id==1 ? "enq" : "deq" ,
		       rec.val,
		       rec.tstamp.tv_sec,
		       rec.tstamp.tv_nsec);
	}
	printf("done\n");
}
	
