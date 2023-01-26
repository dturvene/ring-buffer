A Simple Ringbuffer in C
========================

230125 Update
-------------
I used this example to learn the [meson](https://mesonbuild.com/) build
system.  It, among many others, is gradually replacing
[make](https://en.wikipedia.org/wiki/Make_(software)) to build software
projects.  Meson is being incorporated into a number of projects, including my
favorite (QEMU)[https://www.qemu.org/).  It was fairly easy to learn the
grammar to replicate the project Makefile; simple, albeit, but useful.

Abstract
--------
This project demonstrates an efficient, 
portable [ringbuffer](https://en.wikipedia.org/wiki/Circular_buffer) design
suitable for
embedded projects, the device drivers and user-space code.  The code is written
in `C`.  While not “lock-less” (see the research section) the code uses a
small, custom spinlock based on C11 atomics to guard ringbuffer updates. 

The reason I developed this project is there are many implementations
of a ringbuffer (see research secion) but none met my requirements.

A [ringbuffer](https://en.wikipedia.org/wiki/Circular_buffer) is a type of 
[queue](https://en.wikipedia.org/wiki/Queue_(abstract_data_type)) (a.k.a FIFO)
that adds data in the order it is received. Once full, the ringbuffer
overwrites the oldest data with the newest data.  In otherwords, a ringbuffer
is a queue that wraps around to the first element after the last element is filled.

For instance, a ringbuffer has `N`
elements.  The first input will be enqueued at element 1, the second at element
2, up to the Nth at element N.  Once the ringbuffer is full (all N elements
have valid data), the next (newest) datum will be enqueued at the oldest
element, which is element 1.  Conversely, the oldest valid element is dequeued
first, followed but subsequent elements until all *valid* elements are
dequeued.  The name `ringbuffer` is fitting because the dequeue operations
chase the enqueue operations from the first to the Nth element in a
loop.

Case Study
----------
A real-world case study is an
[ADC](https://en.wikipedia.org/wiki/Analog-to-digital_converter) that
samples an analog signal and converts it to a digital value.  The digital value
is written/enqueued to a ringbuffer.  At some point after this the digital
values will be dequeued from the ringbuffer for processing.

A good example of an ADC is the 
[TI ADC12](https://www.ti.com/lit/ug/slau406f/slau406f.pdf)
which can sample an analog stream at 200K samples per second.
This device is too fast for many processors to manage and/or requires a huge
amount of memory to store the samples. However, the samples become stale
quickly so a ring buffer that overwrites the oldest sample values with newest
ones is a good algorithm to employ.

Requirements
------------
The requirements for the ringbuffer design are as follows:

* static data structure, without the need for advanced memory allocation
* detection when no valid elements exist (empty ringbuffer)
* when all buffers are filled the oldest are overwritten with incoming data,
  this allows recent data to be captured while discarding possibly stale data
* simple to understand and maintain, portable to resource limited environments
* mutual exclusion for concurrent data structure updates
* mutual exclusion must not cause a task/thread reschedule, disqualifying a
  mutex or semaphore
* written in `C` for Linux kernel driver work

Research
--------
The following ringbuffer implementations were among those studied as a basis
for this project. Note that none meet the requirements.

* https://embedjournal.com/implementing-circular-buffer-embedded-c/
* https://embeddedartistry.com/blog/2017/05/17/creating-a-circular-buffer-in-c-and-c/
* https://stackoverflow.com/questions/32109705/what-is-the-difference-between-a-ring-buffer-and-a-circular-linked-list
* https://www.baeldung.com/java-ring-buffer
* https://www.freedesktop.org/software/gstreamer-sdk/data/docs/2012.5/glib/glib-Asynchronous-Queues.html
* https://en.cppreference.com/w/c/atomic

I also researched lockless queues to increase the ringbuffer performance. while
appealing, these require a linked list and dynamic memory allocation (though I
suppose the memory chunks could be statically allocated and put in the linked
list during initialization.)

* https://www.kernel.org/doc/html/latest/trace/ring-buffer-design.html
* https://www.codeproject.com/Articles/153898/Yet-another-implementation-of-a-lock-free-circul
* https://github.com/stv0g/c11-queues

High Level Design
-----------------
There are three small components to the design:

* a data structure describing the queue/ringbuffer and its current state
* an enqueue function to add a `newest` value to an element and move to the
  next available element
* a dequeue function to retrieve an `oldest` value from an element
  (optionally marking the element as *INVALID*) and move to the next available
  element
  
The enqueue and dequeue functions both increment one position in the queue.
When a function writes or reads the last queue element it sets the next
available element to the first element in the queue.

The possibility of data corruption exists because the functions may
concurrently working on the same data structure. This design provides a mutual
exclusion mechanism using a simple spinlock based on C11 atomics. When one
function is modifying the data structure it acquires the lock, if the other
function tries to modify the data structure it will spin until the first
function releases the lock, at which point the waiting function will acquire
the lock.

### ringbuffer data structure
The ringbuffer is a standard queue; the enqueue and dequeue functions have
logic to manage the queue as a ring.

The data structure has four major components:

* a statically allocated array of buffers to implement the queue
* a field pointing to the array element to enqueue the new data, named `enq`
* a field pointing to the array element to dequeue the oldest data, named `deq`
* a counter for how many valid elements are in the ringbuffer, named `count`.

The counter is necessary because the `enq` and `deq` field locations are
ambiguous. When both `enq` and `deq` are pointing to the same
element it could mean the array is full (`count` is array size) or empty
(`count` is 0.)  Additionally the counter is useful to easily determine the
number of valid elements in the queue.

All other queue calculations can be derived from those four but I prefer to do
the calculation once and store it in a field in the structure:

* `first`: the first element in the queue; used to identify the first element to
  start the queue and the location to wrap to when the last element is
  processed
* `last`: the last element in the queue; used to check when to wrap and as a
  comparison when dumping the entire array
* `max`: the size of the queue; used to check the counter
* `cb`: a callback function for debugging; currently just stdout/console print of
  the queue

Code
----
The code and documentation reside on my github account under
[ring-buffer source code](https://github.com/dturvene/ring-buffer).

The code is structured using the 
[Linux kernel coding style guide](https://www.kernel.org/doc/html/latest/process/coding-style.html)
and documented using the 
[linux kernel documentation guide](https://www.kernel.org/doc/html/latest/doc-guide/kernel-doc.html)

### simple queue data structure
<!--
<script src="https://gist.github.com/dturvene/3b4bcf59146784b56dd2763586ed0aae.js"></script>
-->
[gist:simple queue data structures](https://gist.github.com/dturvene/3b4bcf59146784b56dd2763586ed0aae)

### enqueue
<!--
<script src="https://gist.github.com/dturvene/b1ca7791a0c9167e15e9a7f344edf9a8.js"></script>
-->
[gist:enqueue](https://gist.github.com/dturvene/b1ca7791a0c9167e15e9a7f344edf9a8)

### dequeue
<!--
<script src="https://gist.github.com/dturvene/779137c4caea8999963c3b7fb851b639.js"></script>
-->
[gist:dequeue](https://gist.github.com/dturvene/779137c4caea8999963c3b7fb851b639)

### spinlock
The spinlock code is a replacement for `pthread_mutex_lock` based on C11
atomics. It proved to be highly portable and good in driver code that cannot
block as long as the critical sections are fairly small.

### Unit Testing
The unit testing framework is set up by main in `ringbuffer.c` and is organized
as two [pthreads](https://en.wikipedia.org/wiki/Pthreads):

	* a producer enqueuing to the ringbuffer,
	* a consumer dequeuing values.
	
The	producer writes incrementally higher values to the ringbuffer, representing
chronologically ordering, and the consumer reads them in the same order.  When
the producer is done, it enqueues an `END_EL` value alerting the consumer to
also exit.

<!--
<script -->
<script src="https://gist.github.com/dturvene/e40b20b646780b8c41e14d81d02b67ca.js"></script>
-->
See [gist:unit test framework](https://gist.github.com/dturvene/15dc6274e0c81b1da7467ca2621a6197)	
	
Originally the enqueue and dequeue functions used stdio for tracking but this
was found to be too costly, creating a significant overhead in the threads. 

The solution to this was to create a logging ringbuffer to enqueue the producer
and consumer events while the threads run.  When the threads end and are joined
back, the logging ringbuffer is written using stdio.  The logging ringbuffer
itself is a simple, easily modified, version of the example ringbuffer
implementation. Not only does this mechanism dramatically increases the speed
test framework but *also* shows the power and portability of the ringbuffer
code.

Results
-------
It is always hard to quantify an algorithm without a great deal of testing but
preliminary unit tests illustrate the following results: 

* A number of producer functions are included in the unit test to gauge
different performance characteristics. Some producers send a loop of events,
nap, more events, nap, etc. 

* The benchmark producer sends a number of events provided on the command line
as fast as possible; the command line below shows test 3 with a count of 10M,
stdout to a tmp file. 
```
linger:763$ linger:758$ ./ringbuffer -t 3 -c 10000000 > /tmp/test3.log
q_producer_stress3: send 10000000
q_consumer: exiting
total log records = 10000
elapsed time from thread create after thread join: delta=4.355530222
lock_held_c=47503018 lock_held_p=55134829
```

* The simple spinlock performed much better than the pthread mutexes. Notice
the test run above how large the `lock_held` counts are, indicating contention
between the producer and consumer threads. One theory is the spinlock runs more
efficiently than the mutex because the lock critical section is small and the
spinlock removes the mutex context switching.

* The overall ringbuffer appears to be performant. A stream of 10M events
generated from producer to consumer using a ringbuffer with a queue depth of
4096 takes roughly 2.2 seconds without event logging. The same logic using gcc
pthread mutex for locking rises to 3.3sec. With event logging enabled the time
jumps to 4.3sec for spinlock and 9.8sec for pthread mutex. 

* A larger queue size creates more efficiency, probably because the `q_enq` and
 `q_deq` functions execute less ringbuffer wrap logic.
 
* I tested various atomic 
[memory models](https://gcc.gnu.org/onlinedocs/gcc-7.5.0/gcc/_005f_005fatomic-Builtins.html)
for the spin lock. Using 100M enq/deq calls, the `__ATOMIC_ACQUIRE` model
seemed to be slightly faster than the stronger (default) `__ATOMIC_SEQ_CST`,
roughly about 1-2% on average.  I don't know how much the faster memory model
increases the risk of data corruption.

Summary
-------
This project was fun to do and I learned a good bit.  The ringbuffer
implementation for the ADC driver worked well and is easily maintained. The
ISR reads the device and enqueues the 12bit sample value, the driver 
[kthread](https://lwn.net/Articles/65178/) dequeues the values for processing.
Some values can be seen to be overwritten but only occasionally and then, due
to the queue depth, not much of an impact.

A mark of a good project is my problem about how to efficiently debug and
affirm the ringbuffer is working. The answer: create a logging ringbuffer!

Enhancements
------------
Add code to log an event when the oldest queue element is overwritten to track
data loss.

Reduce lock contention windows. Possible avenues include: 

* reader-writer locks
* a lock for each array element
* atomic enqueue and dequeue operations for lockless

It is possible to pull the generic ringbuffer pieces into a library.  One could
change the `buf_t` typedef to a `void * payloadp` and attach a generic payload
to each element.  Then, possibly, use a callback function 
(similar to the `cb` field) to process the payload.  It becomes a little more
complex to manage the payloads.

