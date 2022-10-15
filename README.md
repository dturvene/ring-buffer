# ringbuffer

Todo:

* critical section lock

## Abstract
This project demonstrates an efficient, portable ringbuffer design suitable for
embedded projects, the Linux kernel and user-space code.  The code is written
in `C`.  The reason I developed this project is there are many implementations
of a ringbuffer (see [Research](#research)) but none met my 
[Requirements](#requirements).

A [ringbuffer](https://en.wikipedia.org/wiki/Circular_buffer) is a type of 
[queue](https://en.wikipedia.org/wiki/Queue_(abstract_data_type) (or FIFO) that
adds data in the order it is received. Once full, the ringbuffer overwrites
the oldest data with the newest data.  In otherwords, a ringbuffer is a queue
that wraps around to the first element after the last element is filled.

For instance, a ringbuffer has `N`
elements.  The first input will be enqueued at element 1, the second at element
2, up to the Nth at element N.  Once the ringbuffer is full (all N elements
have valid data), the next (newest) datum will be enqueued at the oldest
element, which is element 1.  Conversely, the oldest valid element is dequeued
first, followed but subsequent elements until all *valid* elements are
dequeued.  The name `ringbuffer` is fitting because the buffer dequeue operations
chase the buffer enqueue operations from the first to the Nth element in a
loop.

## Case Study
An real-world case study is an
[ADC](https://en.wikipedia.org/wiki/Analog-to-digital_converter) that
samples an analog signal and converts it to a digital value.  The digital value
is written/enqueued to a ringbuffer.  At some point after this the digital
values will be dequeued from the ringbuffer for processing.

A good example of an ADC is the 
[TI ADC12](https://www.ti.com/lit/ug/slau406f/slau406f.pdf)
which can sample an analog stream at 200K samples per second.

## Requirements
The requirements for the ringbuffer design are as follows:

* static data structure, without the need for advanced memory allocation
* detection when no valid elements
* when buffers are exhausted the oldest are overwritten with incoming data,
  this allows recent data to be captured while discarding possibly stale data
* simple to understand and maintain, portable to resource limited environments
* mutual exclusion for critical sections
* written in `C` for Linux kernel driver work

## Research
The following ringbuffer implementations were among those studied as a basis
for this project. Note that none meet the [Requirements](#requirements).

* https://www.kernel.org/doc/html/latest/trace/ring-buffer-design.html
* https://embedjournal.com/implementing-circular-buffer-embedded-c/
* https://stackoverflow.com/questions/32109705/what-is-the-difference-between-a-ring-buffer-and-a-circular-linked-list
* https://www.baeldung.com/java-ring-buffer

## High Level Design
There are three small components to the design:

* a data structure describing the queue/ringbuffer and its current state
* an enqueue function to add a `newest` value to an element and move to the
  next available element
* a dequeue function to retrieve an `oldest` value from an element (and
  optionally mark the element as *INVALID*) and move to the next available
  element
  
The enqueue and dequeue functions both increment one position in the queue.
When a function writes or reads the last queue element it sets the next
available element to the first element in the queue.
  
### ringbuffer data structure
The ringbuffer is a standard queue; the enqueue and dequeue functions have
logic to manage the queue as a ring.

The data structure has four major components:

* a statically allocated array of buffers to implement the queue
* a field pointing to the array element to enqueue the new data, called `enq`
* a field pointing to the array element to dequeue the oldest data, called `deq`
* a counter for how many valid elements are in the ringbuffer, called `count`.

The counter is necessary because the `enq` and `deq` field locations are
ambiguous. Examples are: when both `enq` and `deq` are pointing to the same
element it could mean the array is full (`counter` is array size) or empty
(`counter` is 0); when `deq` is point to element N and `enq` is pointing to
element N+1 the array could have one valid element (`counter` is 1) or all but
one valid element.

All other queue calculations can be derived from those four but I prefer to do
the calculation once and store it in a field in the structure:

* first: the first element in the queue; used to identify the first element to
  start the queue and the location to wrap to when the last element is
  processed
* last: the last element in the queue; used to check when to wrap and as a
  comparison when dumping the entire array
* max: the size of the queue; used to check the counter
* cb: a callback function for debugging; currently just stdout/console print of
  the queue

## Code
The code and documentation reside on my github account under
[ring-buffer source code](https://github.com/dturvene/ring-buffer)

The code is structured using the 
[Linux kernel coding style guide](https://www.kernel.org/doc/html/latest/process/coding-style.html)
and documented using the 
[linux kernel documentation guide](https://www.kernel.org/doc/html/latest/doc-guide/kernel-doc.html)


### ringbuffer data structure
<!--
<script src="https://gist.github.com/dturvene/3b4bcf59146784b56dd2763586ed0aae.js"></script>
-->
[ringbuffer data structures](https://gist.github.com/dturvene/3b4bcf59146784b56dd2763586ed0aae)

### enqueue
<!--
<script src="https://gist.github.com/dturvene/b1ca7791a0c9167e15e9a7f344edf9a8.js"></script>
-->
[enqueue gist](https://gist.github.com/dturvene/b1ca7791a0c9167e15e9a7f344edf9a8)

### dequeue
<!--
<script src="https://gist.github.com/dturvene/779137c4caea8999963c3b7fb851b639.js"></script>
-->
[dequeue gist](https://gist.github.com/dturvene/779137c4caea8999963c3b7fb851b639)

### Unit Testing
<!--
<script src="https://gist.github.com/dturvene/15dc6274e0c81b1da7467ca2621a6197.js"></script>
-->
[unit test framework gist](https://gist.github.com/dturvene/15dc6274e0c81b1da7467ca2621a6197)

## Summary

### Enhancements/Alternatives

