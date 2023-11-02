---
title: Pthread Queues
author: Jens Schweikhardt
---

# Pthread Queues

## What is it?

This is a compact implementation (just 2 files, `pq.c`, `pq.h`)
of various queues using POSIX threads, also known as pthreads.
Various queues means you can create queues with different
insert/remove order:

* FIFO (_first in, first out_): how everybody understands a queue to behave.
* Priority queue: messages have a priority and are removed highest priority
  first. If more than one message have the highest priority, the order is
  unspecified. (If you must know, it's _binary heap order_.)
* Priority queue plus FIFO: messages have a priority and are removed highest
  priority first. If more than one message have the highest priority, the order
  is FIFO among those messages.
* LIFO (_last in, first out_): how everybody understands a stack to behave.

Each queue has a set of attributes describing

* Queue insert/remove order.
* Queue capacity: how many messages it can receive until full.
* Queue message size: how much data a single message can transport.
* Maximum priority a message can have.

## Features

* All send and receive calls can be blocking, non-blocking or specify a timeout.
* Access to queue data is locked with pthread mutexes.
* Synchronization between receiver and sender uses pthread condition variables.
* Message data are copied so data can come from objects that go out of
  scope or are deallocated after sending.
* All functions return 0 on success and error codes otherwise.
* Message memory is dynamically allocated with malloc/calloc once during queue
  creation. If you need to avoid dynamic allocation you may hack around it by
  defining the queue structs with arrays instead of pointers.
* Queue types that don't operate on priorities (FIFO and LIFO) still transport
  a message's priority which may be used as a side channel.

## How do I use Pthread Queues in my Program?

1. Add `pq.c` and `pq.h` to your source tree.
2. Add feature test macros needed to compile pthread programs to your
   C compilation command, e.g. `-D_XOPEN_SOURCE=600`.
3. Add `-lpthread` to the link command.
4. Call Pthread Queue functions from a thread created with *pthread_create*().
   Don't call them from *main*(), as *main* is not a proper pthread.

For reference, the unit tests in `test_pq.c` thoroughly exercise each
function.

## Application Programming Interface (API)

Manuals for

* [pq_create.3](#pq_create)
* [pq_destroy.3](#pq_destroy)
* [pq_recv_nonbl.3](#pq_recv_nonbl)
* [pq_recv_timed.3](#pq_recv_timed)
* [pq_send_nonbl.3](#pq_send_nonbl)
* [pq_send_timed.3](#pq_send_timed)

---
