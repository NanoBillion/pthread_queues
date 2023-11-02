---
title: Pthread Queues
author: Jens Schweikhardt
---

# Pthread Queues

## What is it?

This is an implementation of various queues using POSIX threads, also known
as pthreads. Various queues means you can create queues with different
insert/remove order:

* FIFO (_first in, first out_): how everybody understands a queue to behave.
* Priority queue; messages have a priority and are removed highest priority
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

## Application Programming Interface (API)

Manuals for

* [pq_create.3](#pq_create)
* [pq_destroy.3](#pq_destroy)
* [pq_recv_nonbl.3](#pq_recv_nonbl)
* [pq_recv_timed.3](#pq_recv_timed)
* [pq_send_nonbl.3](#pq_send_nonbl)
* [pq_send_timed.3](#pq_send_timed)

---
### pq_create
<pre a="#pq_create">
PQ_CREATE(3)                Library Functions Manual              PQ_CREATE(3)

<b>NAME</b>
       pq_create — create a pthread queue

<b>SYNOPSIS</b>
       <b>#include &lt;pq.h&gt;</b>

       <i>pq_status_t</i>
       <i>pq_create</i>(<i>struct</i> <i>pq_queue</i> <i>**q</i>, <i>struct</i> <i>pq_attr</i> <i>*attr</i>);

<b>DESCRIPTION</b>
       The  <i>pq_create</i>() function creates a new queue. Upon success it stores a
       queue handle in the memory pointed to by <i>q</i>.  The <i>attr</i>  argument  points
       to a structure with the following members:

       <i>maxmsg      </i>Number of messages queue can receive until full.
       <i>msgsize     </i>Maximum message size in bytes.
       <i>order       </i>Insert/remove order, see below.
       <i>maxprio     </i>For priority queues, the maximum allowed priority.

       The order attribute is one of

       <b>PQ_ATTR_FIFO</b>
                   FIFO  (first  in,  first out).  How everybody understands a
                   queue to behave.  Message priorities are ignored  but  sent
                   and  received  intact.   Insert  and remove operations have
                   complexity O(1).
       <b>PQ_ATTR_PRIOQ</b>
                   Priority queue.  Messages have a priority and  are  removed
                   highest  priority  first.  If more than one message has the
                   highest priority, the order is unspecified.  Insert and re‐
                   move operations have complexity O(log N).
       <b>PQ_ATTR_PRIFO</b>
                   Priority queue plus FIFO.  Messages have a priority and are
                   removed highest priority first.  If more than  one  message
                   has  the  highest  priority,  the order is FIFO among those
                   messages.  The insert operation has complexity O(N), remove
                   O(1).
       <b>PQ_ATTR_LIFO</b>
                   LIFO (last in, first out).   How  everybody  understands  a
                   stack  to  behave.  Message priorities are ignored but sent
                   and received intact.  Insert  and  remove  operations  have
                   complexity O(1).

       Message data are copied when sent and received.  Data may come from ob‐
       jects that go out of scope or are deallocated after sending.

<b>RETURN VALUES</b>
       If successful, the function returns zero.  Otherwise an error number is
       returned to indicate the error or special condition.

<b>ERRORS</b>
       The <i>pq_create</i>() function fails if:

       [EINVAL]           The argument <i>q</i> or the argument <i>attr</i> is NULL.

       [ENOMEM]           Not enough memory.

       [EAGAIN]           The system temporarily lacks the resources to create
                          another condition variable.

<b>SEE ALSO</b>
       <i>pq_create</i>(3),   <i>pq_recv_nonbl</i>(3),  <i>pq_recv_timed</i>(3),  <i>pq_send_nonbl</i>(3),
       <i>pq_send_timed</i>(3)

FreeBSD 13.2                   October 10, 2023                   PQ_CREATE(3)
</pre>
### pq_destroy
<pre a="#pq_destroy">
PQ_DESTROY(3)               Library Functions Manual             PQ_DESTROY(3)

<b>NAME</b>
       pq_create, pq_destroy — create or destroy a POSIX/pthread queue

<b>SYNOPSIS</b>
       <b>#include &lt;pq.h&gt;</b>

       <i>pq_status_t</i>
       <i>pq_create</i>(<i>struct</i> <i>pq_queue</i> <i>**q</i>, <i>struct</i> <i>pq_attr</i> <i>*attr</i>);

       <i>pq_status_t</i>
       <i>pq_destroy</i>(<i>struct</i> <i>pq_queue</i> <i>*q</i>);

<b>DESCRIPTION</b>
       The  <i>pq_create</i>() function creates a new queue. Upon success it stores a
       queue handle in the memory pointed to by <i>q</i>.  The <i>attr</i>  argument  points
       to a structure with the following members: TODO

       The  <i>pq_destroy</i>() function destroys the queue <i>q</i>, freeing all memory al‐
       located upon its creation.

<b>RETURN VALUES</b>
       If successful, the functions return zero.  Otherwise an error number is
       returned to indicate the error or special condition.

<b>ERRORS</b>
       The <i>pq_create</i>() function fails if:

       [EINVAL]           The argument <i>q</i> or the argument <i>attr</i> is NULL.

       [ENOMEM]           Not enough memory.

       [EAGAIN]           The system temporarily lacks the resources to create
                          another condition variable.

       The <i>pq_destroy</i>() function fails if:

       [EINVAL]           The argument <i>q</i> is NULL.

<b>SEE ALSO</b>
       <i>pq_recv_nonbl</i>(3), <i>pq_recv_timed</i>(3), <i>pq_send_nonbl</i>(3), <i>pq_send_timed</i>(3)

FreeBSD 13.2                   October 10, 2023                  PQ_DESTROY(3)
</pre>
### pq_recv_nonbl
<pre a="#pq_recv_nonbl">
PQ_RECV_NONBL(3)            Library Functions Manual          PQ_RECV_NONBL(3)

<b>NAME</b>
       pq_recv_nonbl — receive a pthread queue message without blocking

<b>SYNOPSIS</b>
       <b>#include &lt;pq.h&gt;</b>

       <i>pq_status_t</i>
       <i>pq_recv_nonbl</i>(<i>struct</i> <i>pq_queue</i> <i>*q</i>, <i>struct</i> <i>pq_msg</i> <i>*m</i>);

<b>DESCRIPTION</b>
       The  <i>pq_recv_nonbl</i>()  function  attempts  to receive a message from the
       specified queue <i>q</i> and store it in the object pointed to  by  <i>m</i>  without
       blocking.

<b>RETURN VALUES</b>
       If  a  message  was  successfully  received, the function returns zero.
       Otherwise an error number is returned to indicate the error or  special
       condition.

<b>ERRORS</b>
       The <i>pq_recv_nonbl</i>() function fails if:

       [EINVAL]           The argument <i>q</i> or the argument <i>m</i> is NULL.

       [EINVAL]           The pointer <i>m‐&gt;msg</i> is NULL.

       [EAGAIN]           The queue is empty.

       In addition, all errors caused by a failed call to <i>pthread_mutex_lock</i>()
       and <i>pthread_mutex_unlock</i>() may be returned.

<b>SEE ALSO</b>
       <i>pq_create</i>(3),    <i>pq_destroy</i>(3),   <i>pq_recv_timed</i>(3),   <i>pq_send_nonbl</i>(3),
       <i>pq_send_timed</i>(3)

FreeBSD 13.2                   October 10, 2023               PQ_RECV_NONBL(3)
</pre>
### pq_recv_timed
<pre a="#pq_recv_timed">
PQ_RECV_TIMED(3)            Library Functions Manual          PQ_RECV_TIMED(3)

<b>NAME</b>
       pq_recv_timed — receive a pthread queue message with a timeout

<b>SYNOPSIS</b>
       <b>#include &lt;pq.h&gt;</b>

       <i>pq_status_t</i>
       <i>pq_recv_timed</i>(<i>struct</i> <i>pq_queue</i> <i>*q</i>, <i>struct</i> <i>pq_msg</i> <i>*m</i>, <i>pq_timeout_t</i> <i>t</i>);

<b>DESCRIPTION</b>
       The  <i>pq_recv_timed</i>()  function  attempts  to receive a message from the
       specified queue <i>q</i> and store it in the message object pointed  to  by  <i>m</i>
       with  a  timeout  given  by <i>t</i>.  A timeout value of PQ_TIMEOUT_ZERO will
       cause <i>pq_recv_timed </i>to return immediately.  A timeout value of PQ_TIME‐
       OUT_INF will cause <i>pq_recv_timed </i>to block indefinitely.

       The timeout has a resolution given by the PQ_TIMEOUT_RESOLUTION  macro,
       expressed as a fraction of a second.  By default it is 1000, giving 1ms
       resolution of real time.

<b>RETURN VALUES</b>
       If  a  message  was  successfully  received, the function returns zero.
       Otherwise an error number is returned to indicate the error or  special
       condition.

<b>ERRORS</b>
       The <i>pq_recv_timed</i>() function fails if:

       [EINVAL]           The argument <i>q</i> or the argument <i>m</i> is NULL.

       [EINVAL]           The pointer <i>m‐&gt;msg</i> is NULL.

       [EAGAIN]           The  queue  is  empty and PQ_TIMEOUT_ZERO was speci‐
                          fied.

       [ETIMEDOUT]        The queue is empty after timeout expired.

       In   addition,   all   errors   caused   by   a    failed    call    to
       <i>pthread_mutex_lock</i>(),    <i>pthread_mutex_unlock</i>(),   <i>pthread_cond_wait</i>(),
       <i>pthread_cond_signal</i>(),  <i>pthread_cond_timedwait,</i>()  and  <i>clock_gettime</i>()
       may be returned.

<b>SEE ALSO</b>
       <i>pq_create</i>(3),    <i>pq_destroy</i>(3),   <i>pq_recv_timed</i>(3),   <i>pq_send_nonbl</i>(3),
       <i>pq_send_timed</i>(3)

FreeBSD 13.2                   October 10, 2023               PQ_RECV_TIMED(3)
</pre>
### pq_send_nonbl
<pre a="#pq_send_nonbl">
PQ_SEND_NONBL(3)            Library Functions Manual          PQ_SEND_NONBL(3)

<b>NAME</b>
       pq_send_nonbl — send a pthread queue message without blocking

<b>SYNOPSIS</b>
       <b>#include &lt;pq.h&gt;</b>

       <i>pq_status_t</i>
       <i>pq_send_nonbl</i>(<i>struct</i> <i>pq_queue</i> <i>*q</i>, <i>const</i> <i>struct</i> <i>pq_msg</i> <i>*m</i>);

<b>DESCRIPTION</b>
       The <i>pq_send_nonbl</i>() function attempts to send the message pointed to by
       <i>m</i> to the specified <i>q</i> without blocking.

       A successful call copies the message data pointed to by <i>m‐&gt;msg</i> to queue
       internal  memory.  This means that the object can go out of scope or be
       deallocated.

<b>RETURN VALUES</b>
       If the message was sent successfully, the function returns zero.   Oth‐
       erwise  an  error  number  is returned to indicate the error or special
       condition.

<b>ERRORS</b>
       The <i>pq_send_nonbl</i>() function fails if:

       [EINVAL]           The argument <i>q</i> or the argument <i>m</i> is NULL.

       [EINVAL]           The pointer <i>m‐&gt;msg</i> is NULL.

       [EINVAL]           The priority <i>m‐&gt;prio</i>  exceeds  the  queue’s  maximum
                          priority attribute.

       [EMSGSIZE]         The message size <i>m‐&gt;size</i> exceeds the queue’s maximum
                          message size attribute.

       [EAGAIN]           The queue is full.

       In    addition,    all    errors   caused   by   a   failed   call   to
       <i>pthread_mutex_lock</i>(),   <i>pthread_mutex_unlock</i>(),    <i>pthread_cond_wait</i>(),
       <i>pthread_cond_signal</i>(),  <i>pthread_cond_timedwait,</i>()  and  <i>clock_gettime</i>()
       may be returned.

<b>SEE ALSO</b>
       <i>pq_create</i>(3),   <i>pq_destroy</i>(3),   <i>pq_recv_timed</i>(3),    <i>pq_recv_nonbl</i>(3),
       <i>pq_send_timed</i>(3)

FreeBSD 13.2                   October 10, 2023               PQ_SEND_NONBL(3)
</pre>
### pq_send_timed
<pre a="#pq_send_timed">
PQ_SEND_TIMED(3)            Library Functions Manual          PQ_SEND_TIMED(3)

<b>NAME</b>
       pq_send_timed — send a pthread queue message with a timeout

<b>SYNOPSIS</b>
       <b>#include &lt;pq.h&gt;</b>

       <i>pq_status_t</i>
       <i>pq_send_timed</i>(<i>struct</i>    <i>pq_queue</i>    <i>*q</i>,   <i>const</i>   <i>struct</i>   <i>pq_msg</i>   <i>*m</i>,
           <i>pq_timeout_t</i> <i>t</i>);

<b>DESCRIPTION</b>
       The <i>pq_send_timed</i>() function attempts to send the message pointed to by
       <i>m</i> to the specified <i>q</i> with a timeout given by <i>t</i>.

       A successful call copies the message data pointed to by <i>m‐&gt;msg</i> to queue
       internal memory.  This means that the object can go out of scope or  be
       deallocated.

       A  timeout  value of PQ_TIMEOUT_ZERO will cause <i>pq_send_timed </i>to return
       immediately.   A   timeout   value   of   PQ_TIMEOUT_INF   will   cause
       <i>pq_send_timed </i>to block indefinitely.

       The  timeout has a resolution given by the PQ_TIMEOUT_RESOLUTION macro,
       expressed as a fraction of a second.  By default it is 1000, giving 1ms
       resolution of real time.

<b>RETURN VALUES</b>
       If the message was sent successfully, the function returns zero.   Oth‐
       erwise  an  error  number  is returned to indicate the error or special
       condition.

<b>ERRORS</b>
       The <i>pq_send_timed </i>function fails if:

       [EINVAL]           The argument <i>q</i> or the argument <i>m</i> is NULL.

       [EINVAL]           The pointer <i>m‐&gt;msg</i> is NULL.

       [EINVAL]           The priority <i>m‐&gt;prio</i>  exceeds  the  queue’s  maximum
                          priority attribute.

       [EMSGSIZE]         The message size <i>m‐&gt;size</i> exceeds the queue’s maximum
                          message size attribute.

       [EAGAIN]           The queue is full and PQ_TIMEOUT_ZERO was specified.

       [ETIMEDOUT]        The queue is still full after the timeout expired.

       In addition, all errors caused by a failed call to <i>pthread_mutex_lock</i>()
       and <i>pthread_mutex_unlock</i>() may be returned.

<b>SEE ALSO</b>
       <i>pq_create</i>(3),    <i>pq_destroy</i>(3),   <i>pq_recv_timed</i>(3),   <i>pq_recv_nonbl</i>(3),
       <i>pq_send_timed</i>(3)

FreeBSD 13.2                   October 10, 2023               PQ_SEND_TIMED(3)
</pre>
