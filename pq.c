/*
 * Pthread queues -- priority queues and then some.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "pq.h"

/******************************************************************************/
/*!
 * Allocate a queue.
 * @param   aQueue      [out] Pointer to queue handle.
 * @param   aAttributes [in] Queue attributes.
 * @return  0           Success; *aQueue was assigned a handle.
 * @return  EINVAL      Invalid argument.
 * @return  ENOMEM      Out of memory.
 * @return  Otherwise status code of failed pthread call.
 */
pq_status_t pq_create(struct pq_queue **aQueue, const struct pq_attr *aAttributes) {
    if ((aQueue == NULL) || (aAttributes == NULL)) {
        return EINVAL;
    }

    struct pq_queue *const q = malloc(sizeof *q);
    if (q == NULL) {
        return ENOMEM;
    }

    pq_status_t sc = pthread_mutexattr_init(&q->attr);
    if (sc != 0) {
        return pq_cleanup(q, 1, sc);
    }

    sc = pthread_mutexattr_settype(&q->attr, PTHREAD_MUTEX_RECURSIVE);
    if (sc != 0) {
        return pq_cleanup(q, 2, sc);
    }

    sc = pthread_mutex_init(&q->mtx, &q->attr);
    if (sc != 0) {
        return pq_cleanup(q, 2, sc);
    }

    sc = pthread_cond_init(&q->ready_to_send, NULL);
    if (sc != 0) {
        return pq_cleanup(q, 3, sc);
    }

    sc = pthread_cond_init(&q->ready_to_recv, NULL);
    if (sc != 0) {
        return pq_cleanup(q, 4, sc);
    }

    q->fill = 0;
    q->head = 0;
    q->tail = 0;
    q->waiting_to_send = 0;
    q->waiting_to_recv = 0;
    q->maxmsg = aAttributes->maxmsg;
    q->msgsize = aAttributes->msgsize;
    q->order = aAttributes->order;
    q->maxprio = aAttributes->maxprio;
    q->message = calloc(q->maxmsg, sizeof *q->message);
    if (q->message == NULL) {
        return pq_cleanup(q, 5, ENOMEM);
    }
    for (msgindex_t i = 0; i < q->maxmsg; ++i) {
        q->message[i].msg = malloc(q->msgsize);
        if (q->message[i].msg == NULL) {
            return pq_cleanup(q, 6, ENOMEM);
        }
    }
    *aQueue = q;
    return 0;
}

/******************************************************************************/
/*!
 * Destroy a queue, deallocating all resources.
 * @param   aQueue    [in] Queue handle.
 * @return  0         Success.
 * @return  EINVAL    Invalid argument.
 */
pq_status_t pq_destroy(struct pq_queue *aQueue) {
    if (aQueue == NULL) {
        return EINVAL;
    }
    return pq_cleanup(aQueue, ~0, 0);
}

/******************************************************************************/
/*!
 * Try to send a message to a queue. Does not block.
 * @param   aQueue       [in] Queue handle.
 * @param   aMessage     [in] Message to send.
 * @return  0            Success.
 * @return  EAGAIN       Queue is full.
 * @return  EINVAL       Invalid argument.
 * @return  EMSGSIZE     Message too big for queue.
 */
pq_status_t pq_send_nonbl(struct pq_queue *aQueue, const struct pq_msg *aMessage) {
    if ((aQueue == NULL) || (aMessage == NULL)) {
        return EINVAL;
    }
    if ((aMessage->prio > aQueue->maxprio) || (aMessage->msg == NULL)) {
        return EINVAL;
    }
    if (aMessage->size > aQueue->msgsize) {
        return EMSGSIZE;
    }

    pq_status_t sc = pthread_mutex_lock(&aQueue->mtx);
    pq_unlock_and_return_if_unsuccessful(sc);
    if (aQueue->fill == aQueue->maxmsg) {
        sc = pthread_mutex_unlock(&aQueue->mtx);
        return (sc != 0) ? sc : EAGAIN;
    }
    pq_insert(aQueue, aMessage);
    if (aQueue->waiting_to_recv > 0) {
        sc = pthread_cond_signal(&aQueue->ready_to_recv);
        pq_unlock_and_return_if_unsuccessful(sc);
    }
    sc = pthread_mutex_unlock(&aQueue->mtx);
    return sc;
}

/******************************************************************************/
/*!
 * Try to receive a message from a queue. Does not block.
 * @param   aQueue    [in] Queue handle.
 * @param   aMessage  [out] Message retrieved from queue's head.
 * @return  0 on success.
 * @return  EAGAIN       Queue is empty.
 * @return  EINVAL       Invalid argument.
 * @return  Error code otherwise.
 *
 * The buffer pointed to by aMessage->msg must be large enough to hold all data,
 * which is at most aQueue->maxmsg bytes.
 */
pq_status_t pq_recv_nonbl(struct pq_queue *aQueue, struct pq_msg *aMessage) {
    if ((aQueue == NULL) || (aMessage == NULL) || (aMessage->msg == NULL)) {
        return EINVAL;
    }

    pq_status_t sc = pthread_mutex_lock(&aQueue->mtx);
    pq_unlock_and_return_if_unsuccessful(sc);
    if (aQueue->fill == 0) {
        sc = pthread_mutex_unlock(&aQueue->mtx);
        return (sc != 0) ? sc : EAGAIN;
    }
    pq_remove(aQueue, aMessage);
    if (aQueue->waiting_to_send > 0) {
        sc = pthread_cond_signal(&aQueue->ready_to_send);
        pq_unlock_and_return_if_unsuccessful(sc);
    }
    sc = pthread_mutex_unlock(&aQueue->mtx);
    return sc;
}

/******************************************************************************/
/*!
 * Send message, with timeout.
 * @param   aQueue      [in] Queue handle.
 * @param   aMessage    [in] Message to send.
 * @param   aTimeout    How long to wait on a full queue until timeout.
 * @return  0           Success.
 * @return  EINVAL      Invalid argument.
 * @return  EAGAIN      Queue is full and PQ_TIMEOUT_ZERO was specified.
 * @return  ETIMEDOUT   Queue is full after timeout expired.
 * @return  Error code otherwise.
 */
pq_status_t pq_send_timed(struct pq_queue *aQueue, const struct pq_msg *aMessage, pq_time_t aTimeout) {
    if (aTimeout == PQ_TIMEOUT_ZERO) {
        return pq_send_nonbl(aQueue, aMessage);
    }
    if ((aQueue == NULL) || (aMessage == NULL)) {
        return EINVAL;
    }
    if ((aMessage->prio > aQueue->maxprio) || (aMessage->msg == NULL)) {
        return EINVAL;
    }

    pq_status_t sc = pthread_mutex_lock(&aQueue->mtx);
    pq_unlock_and_return_if_unsuccessful(sc);

    while (aQueue->fill == aQueue->maxmsg) {
        ++aQueue->waiting_to_send;
        if (aTimeout == PQ_TIMEOUT_INF) {
            sc = pthread_cond_wait(&aQueue->ready_to_send, &aQueue->mtx);
        }
        else {
            sc = pq_cond_timedwait(&aQueue->ready_to_send, &aQueue->mtx, aTimeout);
        }
        --aQueue->waiting_to_send;
        pq_unlock_and_return_if_unsuccessful(sc);
    }

    pq_insert(aQueue, aMessage);

    if (aQueue->waiting_to_recv > 0) {
        sc = pthread_cond_signal(&aQueue->ready_to_recv);
        pq_unlock_and_return_if_unsuccessful(sc);
    }

    sc = pthread_mutex_unlock(&aQueue->mtx);
    return sc;
}

/******************************************************************************/
/*!
 * Version of pthread_cond_wait() that computes deadline and waits.
 * @param   aCond       [in] Condition variable.
 * @param   aMutex      [in] Associated mutex.
 * @param   aTimeout    How long to wait.
 * @return  0           Success.
 * @return  ETIMEDOUT   Operation timed out.
 * @return  Error code otherwise.
 */
pq_status_t pq_cond_timedwait(pthread_cond_t *aCond, pthread_mutex_t *aMutex, pq_time_t aTimeout) {
    struct timespec ts;
    pq_status_t sc = clock_gettime(CLOCK_REALTIME, &ts);
    if (sc == 0) {
        pq_add_time(&ts, aTimeout);
        sc = pthread_cond_timedwait(aCond, aMutex, &ts);
    }
    return sc;
}

/******************************************************************************/
/*!
 * Receive message, with timeout.
 * @param   aQueue      [in] Queue handle.
 * @param   aMessage    [out] Message removed from queue.
 * @param   aTimeout    How long to wait on an empty queue until timeout.
 * @return  0           Success.
 * @return  EINVAL      Invalid argument.
 * @return  EAGAIN      Queue is empty and PQ_TIMEOUT_ZERO was specified.
 * @return  ETIMEDOUT   Queue is empty after timeout expired.
 * @return  Error code otherwise.
 */
pq_status_t pq_recv_timed(struct pq_queue *aQueue, struct pq_msg *aMessage, pq_time_t aTimeout) {
    if (aTimeout == PQ_TIMEOUT_ZERO) {
        return pq_recv_nonbl(aQueue, aMessage);
    }
    if ((aQueue == NULL) || (aMessage == NULL) || (aMessage->msg == NULL)) {
        return EINVAL;
    }

    pq_status_t sc = pthread_mutex_lock(&aQueue->mtx);
    pq_unlock_and_return_if_unsuccessful(sc);

    while (aQueue->fill == 0) {
        ++aQueue->waiting_to_recv;
        if (aTimeout == PQ_TIMEOUT_INF) {
            sc = pthread_cond_wait(&aQueue->ready_to_recv, &aQueue->mtx);
        }
        else {
            sc = pq_cond_timedwait(&aQueue->ready_to_recv, &aQueue->mtx, aTimeout);
        }
        --aQueue->waiting_to_recv;
        pq_unlock_and_return_if_unsuccessful(sc);
    }

    pq_remove(aQueue, aMessage);

    if (aQueue->waiting_to_send > 0) {
        sc = pthread_cond_signal(&aQueue->ready_to_send);
        pq_unlock_and_return_if_unsuccessful(sc);
    }

    sc = pthread_mutex_unlock(&aQueue->mtx);
    return sc;
}

/******************************************************************************/
/*!
 * Remove message depending on order.
 * @param   aQueue    [in] Queue handle.
 * @param   aMessage  [out] Message with highest priority.
 */
void pq_remove(struct pq_queue *aQueue, struct pq_msg *aMessage) {
    switch (aQueue->order) {
    case PQ_ATTR_PRIFO:
        pq_remove_prifo(aQueue, aMessage);
        break;
    case PQ_ATTR_PRIOQ:
        pq_remove_prioq(aQueue, aMessage);
        break;
    case PQ_ATTR_FIFO:
        pq_remove_fifo(aQueue, aMessage);
        break;
    case PQ_ATTR_LIFO:
        pq_remove_lifo(aQueue, aMessage);
        break;
    default:
        break;
    }
}

/******************************************************************************/
/*!
 * Insert message depending on order.
 * @param   aQueue    [in] Queue handle.
 * @param   aMessage  [in] Message with highest priority.
 */
void pq_insert(struct pq_queue *aQueue, const struct pq_msg *aMessage) {
    switch (aQueue->order) {
    case PQ_ATTR_PRIFO:
        pq_insert_prifo(aQueue, aMessage);
        break;
    case PQ_ATTR_PRIOQ:
        pq_insert_prioq(aQueue, aMessage);
        break;
    case PQ_ATTR_FIFO:
        pq_insert_fifo(aQueue, aMessage);
        break;
    case PQ_ATTR_LIFO:
        pq_insert_lifo(aQueue, aMessage);
        break;
    default:
        break;
    }
}

/******************************************************************************/
/*!
 * Remove message from message heap.
 * @param   aQueue    [in] Queue handle.
 * @param   aMessage  [out] Message with highest priority.
 * @note    Assumes queue is not empty.
 * @note    Assumes mutex held by caller.
 * @note    Complexity: O(log N).
 */
void pq_remove_prioq(struct pq_queue *aQueue, struct pq_msg *const aMessage) {
    assert(aQueue->fill > 0);
    struct pq_msg *const message = aQueue->message;

    aMessage->size = message[0].size;
    aMessage->prio = message[0].prio;
    memcpy(aMessage->msg, message[0].msg, message[0].size);

    const msgindex_t last = --aQueue->fill;
    if (last == 0) {
        return;
    }
    pq_swap(message, 0, last);
    message[last].prio = 0;
    /* Restore heap order. */
    msgindex_t i = 0;
    while (((2 * i) + 1) < last) {
        const msgindex_t l = (2 * i) + 1;
        const msgindex_t r = (2 * i) + 2;
        const msgindex_t j = (message[l].prio > message[r].prio) ? l : r;
        if (message[i].prio >= message[j].prio) {
            break;
        }
        pq_swap(message, i, j);
        i = j;
    }
}

/******************************************************************************/
/*!
 * Insert message into message heap.
 * @param   aQueue      [in] Queue handle.
 * @param   aMessage    [in] Message to insert.
 * @note    Assumes queue is not full.
 * @note    Assumes mutex held by caller.
 * @note    Complexity: O(log N).
 */
void pq_insert_prioq(struct pq_queue *aQueue, const struct pq_msg *aMessage) {
    assert(aQueue->fill < aQueue->maxmsg);
    struct pq_msg *const message = aQueue->message;

    msgindex_t i = aQueue->fill;
    message[i].size = aMessage->size;
    message[i].prio = aMessage->prio;
    memcpy(message[i].msg, aMessage->msg, aMessage->size);
    ++aQueue->fill;
    while ((i > 0) && (message[(i - 1) / 2].prio < message[i].prio)) {
        const msgindex_t j = (i - 1) / 2;
        pq_swap(message, i, j);
        i = j;
    }
}

/******************************************************************************/
/*!
 * Insert message into message fifo.
 * @param   aQueue      [in] Queue handle.
 * @param   aMessage    [in] Message to insert.
 * @note    Assumes queue is not full.
 * @note    Assumes mutex held by caller.
 * @note    Complexity: O(1).
 */
void pq_insert_fifo(struct pq_queue *aQueue, const struct pq_msg *aMessage) {
    assert(aQueue->fill < aQueue->maxmsg);
    struct pq_msg *const message = aQueue->message;

    const msgindex_t i = aQueue->tail++;
    message[i].size = aMessage->size;
    message[i].prio = aMessage->prio;
    memcpy(message[i].msg, aMessage->msg, aMessage->size);
    if (aQueue->tail == aQueue->maxmsg) {
        aQueue->tail = 0;
    }
    ++aQueue->fill;
}

/******************************************************************************/
/*!
 * Insert message into message array in FIFO order.
 * @param   aQueue      [in] Queue handle.
 * @param   aMessage    [in] Message to insert.
 * @note    Assumes queue is not full.
 * @note    Assumes mutex held by caller.
 * @note    Complexity: O(N).
 *
 * Find index where to insert into array.
 * That's the index of the first message with greater or equal priority.
 * This message and all higher-ups need to rotate up one index to make
 * room for the new message.
 */
void pq_insert_prifo(struct pq_queue *aQueue, const struct pq_msg *aMessage) {
    assert(aQueue->fill < aQueue->maxmsg);
    struct pq_msg *const message = aQueue->message;

    /* Find index where to insert. */
    msgindex_t insert;
    for (insert = 0; insert < aQueue->fill; ++insert) {
        if (message[insert].prio >= aMessage->prio) {
            break;
        }
    }

    /* Rotation required? */
    const msgindex_t rotate = aQueue->fill - insert;
    if (rotate != 0) {
        void   *const tmp = message[insert + rotate].msg;
        struct pq_msg *const src = &message[insert];
        struct pq_msg *const dst = &message[insert + 1];
        memmove(dst, src, rotate * sizeof *src);
        message[insert].msg = tmp;
    }

    /* Insert message. */
    message[insert].prio = aMessage->prio;
    message[insert].size = aMessage->size;
    memcpy(message[insert].msg, aMessage->msg, aMessage->size);
    ++aQueue->fill;
}

/******************************************************************************/
/*!
 * Insert a message to queue in LIFO order (push onto stack).
 * @param   aQueue    [in] Queue handle.
 * @param   aMessage  [out] Message to push.
 * @note    Assumes stack is not empty.
 * @note    Assumes mutex held by caller.
 * @note    Complexity: O(1).
 */
void pq_insert_lifo(struct pq_queue *aQueue, const struct pq_msg *aMessage) {
    assert(aQueue->fill < aQueue->maxmsg);
    struct pq_msg *const message = aQueue->message;

    const msgindex_t i = aQueue->fill++;
    message[i].prio = aMessage->prio;
    message[i].size = aMessage->size;
    memcpy(message[i].msg, aMessage->msg, aMessage->size);
}

/******************************************************************************/
/*!
 * Remove message from message priority queue, plus FIFO.
 * @param   aQueue    [in] Queue handle.
 * @param   aMessage  [out] Message with highest priority.
 * @note    Assumes queue is not empty.
 * @note    Assumes mutex held by caller.
 * @note    Complexity: O(1).
 */
void pq_remove_prifo(struct pq_queue *aQueue, struct pq_msg *const aMessage) {
    assert(aQueue->fill > 0);
    struct pq_msg *const message = aQueue->message;

    const msgindex_t i = --aQueue->fill;
    aMessage->size = message[i].size;
    aMessage->prio = message[i].prio;
    memcpy(aMessage->msg, message[i].msg, aMessage->size);
}

/******************************************************************************/
/*!
 * Remove message from message array.
 * @param   aQueue    [in] Queue handle.
 * @param   aMessage  [out] Message with highest priority.
 * @note    Assumes queue is not empty.
 * @note    Assumes mutex held by caller.
 * @note    Complexity: O(1).
 */
void pq_remove_fifo(struct pq_queue *aQueue, struct pq_msg *const aMessage) {
    assert(aQueue->fill > 0);
    struct pq_msg *const message = aQueue->message;

    const msgindex_t i = aQueue->head;
    ++aQueue->head;
    if (aQueue->head == aQueue->maxmsg) {
        aQueue->head = 0;
    }
    aMessage->size = message[i].size;
    aMessage->prio = message[i].prio;
    memcpy(aMessage->msg, message[i].msg, aMessage->size);
    --aQueue->fill;
}

/******************************************************************************/
/*!
 * Remove a message from queue in LIFO order (pop stack).
 * @param   aQueue    [in] Queue handle.
 * @param   aMessage  [out] Message on top of stack.
 * @note    Assumes stack is not empty.
 * @note    Assumes mutex held by caller.
 * @note    Complexity: O(1).
 */
void pq_remove_lifo(struct pq_queue *aQueue, struct pq_msg *const aMessage) {
    assert(aQueue->fill > 0);
    struct pq_msg *const message = aQueue->message;

    const msgindex_t i = --aQueue->fill;
    aMessage->size = message[i].size;
    aMessage->prio = message[i].prio;
    memcpy(aMessage->msg, message[i].msg, aMessage->size);
}


/******************************************************************************/
/*!
 * Swap two messages in the message store.
 * @param   aMessage  [inout] Message store.
 * @param   aFirst    Index of first message.
 * @param   aSecond   Index of second message.
 */
void pq_swap(struct pq_msg *aMessage, msgindex_t aFirst, msgindex_t aSecond) {
    const struct pq_msg tmp = aMessage[aFirst];
    aMessage[aFirst] = aMessage[aSecond];
    aMessage[aSecond] = tmp;
}

/******************************************************************************/
/*!
 * Increment time stored in a struct timespec.
 * @param   aTime       [inout] Time value.
 * @param   aIncrement  How much time to add.
 */
void pq_add_time(struct timespec *aTime, pq_time_t aIncrement) {
    aTime->tv_sec += aIncrement / PQ_TIMEOUT_RESOLUTION;
    const pq_time_t inc = aIncrement % PQ_TIMEOUT_RESOLUTION;
    const long inc_nsec = inc * (1000000000L / PQ_TIMEOUT_RESOLUTION);
    aTime->tv_nsec += inc_nsec;
    if (aTime->tv_nsec >= 1000000000L) {
        aTime->tv_nsec -= 1000000000L;
        ++aTime->tv_sec;
    }
}

/******************************************************************************/
/*!
 * Deallocate resources in reverse order of allocation.
 * @param   aQueue   [in] Queue to clean up.
 * @param   aItems   Number of items to clean up.
 * @param   aStatus  Status to return.
 * @return  Returns aStatus.
 *
 * This is equivalent to a switch/case with fall throughs from top to bottom
 * but not upsetting linters, MISRA, or your coding rules forbidding goto.
 */
pq_status_t pq_cleanup(struct pq_queue *aQueue, pq_status_t aItems, pq_status_t aStatus) {
    if (aItems >= 6) {
        for (msgindex_t i = 0; i < aQueue->maxmsg; ++i) {
            if (aQueue->message[i].msg != NULL) {
                free(aQueue->message[i].msg);
            }
        }
        free(aQueue->message);
    }
    if (aItems >= 5) {
        pthread_cond_destroy(&aQueue->ready_to_recv);
    }
    if (aItems >= 4) {
        pthread_cond_destroy(&aQueue->ready_to_send);
    }
    if (aItems >= 3) {
        pthread_mutex_destroy(&aQueue->mtx);
    }
    if (aItems >= 2) {
        pthread_mutexattr_destroy(&aQueue->attr);
    }
    if (aItems >= 1) {
        free(aQueue);
    }
    return aStatus;
}

/******************************************************************************/
/*!
 * Get queue's fill level = current number of messages in queue.
 * @param   aQueue      [in] Queue handle.
 * @param   aFill       [out] Fill level.
 * @return  0           Success.
 * @return  Otherwise status code of failed pthread call.
 */
pq_status_t pq_get_fill(struct pq_queue *aQueue, msgindex_t *aFill) {
    pq_status_t sc = pthread_mutex_lock(&aQueue->mtx);
    pq_unlock_and_return_if_unsuccessful(sc);
    *aFill = aQueue->fill;
    sc = pthread_mutex_unlock(&aQueue->mtx);
    return sc;
}

/******************************************************************************/
/*!
 * Dump queue contents to stdout.
 * @param   aQueue      [in] Queue to dump.
 * @return  0           Success.
 * @return  EINVAL      Invalid argument.
 * @return  Otherwise status code of failed pthread call.
 */
pq_status_t pq_dump(struct pq_queue *aQueue) {
    if ((aQueue == NULL) || (aQueue->message == NULL)) {
        return EINVAL;
    }
    pq_status_t sc = pthread_mutex_lock(&aQueue->mtx);
    pq_unlock_and_return_if_unsuccessful(sc);
    printf("Queue handle %p ", (void *) aQueue);
    printf("(%u messages of %u bytes)\n", aQueue->maxmsg, aQueue->msgsize);
    printf("sizeof(struct pq_msg) is %zu bytes.\n", sizeof(struct pq_msg));
    printf("Fill=%u; ", aQueue->fill);
    if (aQueue->fill == 0) {
        printf("queue empty.\n");
    }
    else {
        printf("heap:\n");
        for (msgindex_t i = 0; i < aQueue->fill; ++i) {
            printf("%3u: prio %u, size %u {", i, aQueue->message[i].prio, aQueue->message[i].size);
            const uint8_t *const data = aQueue->message[i].msg;
            for (msgsize_t j = 0; j < aQueue->message[i].size; ++j) {
                printf(" %02x", data[j]);
            }
            printf(" }\n");
        }
    }
    printf("\n");
    sc = pthread_mutex_unlock(&aQueue->mtx);
    return sc;
}

/* vim: set syntax=c tabstop=4 shiftwidth=4 expandtab fileformat=unix: */
