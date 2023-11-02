#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "pq.h"
#include "unity.h"

/* Parameters of the default queues available to all tests via setUp(). */
#define Q_MAXMSG  10
#define Q_MSGSIZE 12
#define Q_MAXPRIO (Q_MAXMSG - 1)

/* Default queues, one for each attribute, indexed by PQ_ATTR_*. */
struct pq_queue *gQueue[4] = { NULL };

/* Get array element count. */
#define ELEMENTS(aArray) (sizeof(aArray) / sizeof(*aArray))

void    setUp(void), tearDown(void);
void    send_message_array(struct pq_queue *aQueue, const struct pq_msg *aArray, msgindex_t aCount);
void    recv_message_array(struct pq_queue *aQueue, struct pq_msg *aArray, msgindex_t aCount);

void    test_pq_macros(void);
void    test_pq_create(void);
void    test_pq_recv_nonbl(void);
void    test_pq_send_nonbl(void);
void    test_pq_recv_timed(void);
void   *test_pq_recv_timed_task(void *aUnused);
void    test_pq_send_timed(void);
void   *test_pq_send_timed_task(void *aUnused);
void    test_pq_add_time(void);
void    test_pq_insert_prioq(void);
void    test_pq_insert_prifo(void);
void    test_pq_remove_prioq(void);
void    test_pq_remove_prifo(void);
void    test_pq_swap(void);
void    test_pq_cond_timedwait(void);
void    test_pq_send_blocking(void);
void    test_pq_recv_blocking(void);
void   *test_pq_blocking_send_task(void *aQueue);
void   *test_pq_blocking_recv_task(void *aQueue);
void    test_pq_stress(void);
void   *test_pq_stress_send_task(void *aQueue);
void   *test_pq_stress_recv_task(void *aQueue);

void    test_sequence_same_priority(void);
void    test_sequence_incr_priority(void);
void    test_sequence_decr_priority(void);
void    test_sequence_mod3_prioq(void);
void    test_sequence_mod3_prifo(void);

/******************************************************************************/

void setUp(void) {
    /* Create each of the four queue types. */
    for (size_t i = 0; i < ELEMENTS(gQueue); ++i) {
        const struct pq_attr attr = {
            .maxmsg = Q_MAXMSG,
            .msgsize = Q_MSGSIZE,
            .order = i,
            .maxprio = Q_MAXPRIO
        };
        TEST_ASSERT_EQUAL(0, pq_create(&gQueue[i], &attr));
    }
}

void tearDown(void) {
    /* Destroy each of the four queue types created by setUp(). */
    for (size_t i = 0; i < ELEMENTS(gQueue); ++i) {
        TEST_ASSERT_EQUAL(0, pq_destroy(gQueue[i]));
    }
}

void send_message_array(struct pq_queue *aQueue, const struct pq_msg *aArray, msgindex_t aCount) {
    for (msgindex_t i = 0; i < aCount; ++i) {
        TEST_ASSERT_EQUAL(0, pq_send_nonbl(aQueue, &aArray[i]));
    }
}

void recv_message_array(struct pq_queue *aQueue, struct pq_msg *aArray, msgindex_t aCount) {
    for (msgindex_t i = 0; i < aCount; ++i) {
        TEST_ASSERT_EQUAL(0, pq_recv_nonbl(aQueue, &aArray[i]));
    }
}

/******************************************************************************/

void test_pq_macros(void) {
    TEST_ASSERT_EQUAL(0, PQ_ATTR_PRIFO);
    TEST_ASSERT_EQUAL(1, PQ_ATTR_PRIOQ);
    TEST_ASSERT_EQUAL(2, PQ_ATTR_FIFO);
    TEST_ASSERT_EQUAL(3, PQ_ATTR_LIFO);
    TEST_ASSERT_EQUAL((pq_time_t) 0u, PQ_TIMEOUT_ZERO);
    TEST_ASSERT_EQUAL(~(pq_time_t) 0u, PQ_TIMEOUT_INF);
    TEST_ASSERT_EQUAL(4, ELEMENTS(gQueue));
}

void test_pq_create(void) {
    for (msgorder_t order = 0; order < ELEMENTS(gQueue); ++order) {
        struct pq_queue *q = NULL;
        const struct pq_attr attr = {
            .maxmsg = Q_MAXMSG,
            .msgsize = Q_MSGSIZE,
            .maxprio = PQ_MAXPRIO,
            .order = order
        };
        TEST_ASSERT_EQUAL(EINVAL, pq_create(NULL, NULL));
        TEST_ASSERT_EQUAL(EINVAL, pq_create(NULL, &attr));
        TEST_ASSERT_EQUAL(EINVAL, pq_create(&q, NULL));
        TEST_ASSERT_EQUAL(0, pq_create(&q, &attr));
        TEST_ASSERT_EQUAL(Q_MAXMSG, q->maxmsg);
        TEST_ASSERT_EQUAL(Q_MSGSIZE, q->msgsize);
        TEST_ASSERT_EQUAL(order, q->order);
        TEST_ASSERT_EQUAL_UINT(PQ_MAXPRIO, q->maxprio);
        TEST_ASSERT_EQUAL(0, q->fill);
        TEST_ASSERT_EQUAL(0, q->head);
        TEST_ASSERT_EQUAL(0, q->tail);
        TEST_ASSERT_EQUAL(0, pq_destroy(q));
    }
}

void test_pq_recv_nonbl(void) {
    for (msgorder_t order = 0; order < ELEMENTS(gQueue); ++order) {
        char    data[Q_MSGSIZE];
        struct pq_msg m = {.msg = "foo",.size = 4,.prio = 2 };
        gQueue[order]->fill = 0;
        TEST_ASSERT_EQUAL(EAGAIN, pq_recv_nonbl(gQueue[order], &m));
        TEST_ASSERT_EQUAL(0, pq_send_nonbl(gQueue[order], &m));
        TEST_ASSERT_EQUAL(1, gQueue[order]->fill);
        m.msg = data;
        m.size = 0;
        m.prio = 0;
        TEST_ASSERT_EQUAL(0, pq_recv_nonbl(gQueue[order], &m));
        TEST_ASSERT_EQUAL_STRING("foo", data);
        TEST_ASSERT_EQUAL(4, m.size);
        TEST_ASSERT_EQUAL(2, m.prio);
    }
}

void test_pq_send_nonbl(void) {
    for (msgorder_t order = 0; order < ELEMENTS(gQueue); ++order) {
        struct pq_msg m = {.msg = "foo",.size = 4,.prio = 2 };
        gQueue[order]->fill = gQueue[order]->maxmsg;
        TEST_ASSERT_EQUAL(EAGAIN, pq_send_nonbl(gQueue[order], &m));
        gQueue[order]->fill = 0;
        m.prio = Q_MAXPRIO + 1u;
        TEST_ASSERT_EQUAL(EINVAL, pq_send_nonbl(gQueue[order], &m));
        m.prio = Q_MAXPRIO;
        TEST_ASSERT_EQUAL(0, pq_send_nonbl(gQueue[order], &m));
        TEST_ASSERT_EQUAL_STRING("foo", gQueue[order]->message[0].msg);
        TEST_ASSERT_EQUAL(4, gQueue[order]->message[0].size);
        TEST_ASSERT_EQUAL(Q_MAXPRIO, gQueue[order]->message[0].prio);
    }
}

void test_sequence_same_priority(void) {
    /* All messages with the same priority. Should be recv'd in FIFO order. */
    const msgorder_t order = PQ_ATTR_PRIFO; /* Works only for PRIFO. */
    char    send_data[Q_MAXMSG][Q_MSGSIZE];
    struct pq_msg send_array[Q_MAXMSG];
    for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
        const msgsize_t size = 1 + (i % Q_MSGSIZE);
        memset(send_data[i], i + 1, size);
        send_array[i].msg = send_data[i];
        send_array[i].size = size;
        send_array[i].prio = 1;
    }
    send_message_array(gQueue[order], send_array, Q_MAXMSG);
    TEST_ASSERT_EQUAL(Q_MAXMSG, gQueue[order]->fill);

    char    recv_data[Q_MAXMSG][Q_MSGSIZE];
    struct pq_msg recv_array[Q_MAXMSG];
    for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
        recv_array[i].msg = recv_data[i];
    }
    recv_message_array(gQueue[order], recv_array, Q_MAXMSG);
    TEST_ASSERT_EQUAL(0, gQueue[order]->fill);
    /* Verify order is FIFO: basically send_data == recv_data. */
    for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
        const msgsize_t size = 1 + (i % Q_MSGSIZE);
        TEST_ASSERT_EQUAL_MEMORY(send_data[i], recv_array[i].msg, size);
        TEST_ASSERT_EQUAL(size, recv_array[i].size);
        TEST_ASSERT_EQUAL(1, recv_array[i].prio);
    }
}

void test_sequence_incr_priority(void) {
    /* Messages with increasing priority from 0 to maxprio. */
    for (msgorder_t order = 0; order <= PQ_ATTR_PRIOQ; ++order) {
        char    send_data[Q_MAXMSG][Q_MSGSIZE];
        struct pq_msg send_array[Q_MAXMSG];
        for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
            memset(send_data[i], i + 1, (i + 1) % (Q_MSGSIZE + 1));
            send_array[i].msg = send_data[i];
            send_array[i].size = (i + 1) % (Q_MSGSIZE + 1);
            send_array[i].prio = i;
        }
        send_message_array(gQueue[order], send_array, Q_MAXMSG);
        TEST_ASSERT_EQUAL(Q_MAXMSG, gQueue[order]->fill);

        char    recv_data[Q_MAXMSG][Q_MSGSIZE];
        struct pq_msg recv_array[Q_MAXMSG];
        for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
            recv_array[i].msg = recv_data[i];
        }
        recv_message_array(gQueue[order], recv_array, Q_MAXMSG);
        TEST_ASSERT_EQUAL(0, gQueue[order]->fill);

        /* Verify order. Should be reversed. */
        for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
            const msgindex_t j = (Q_MAXMSG - 1) - i;
            TEST_ASSERT_EQUAL(send_array[i].prio, recv_array[j].prio);
            TEST_ASSERT_EQUAL(send_array[i].size, recv_array[j].size);
            if (recv_array[j].size != 0) {
                TEST_ASSERT_EQUAL_MEMORY(send_array[i].msg, recv_array[j].msg, recv_array[j].size);
            }
        }
    }
}

void test_sequence_decr_priority(void) {
    /* Messages with decreasing priority from maxprio to 0. */
    for (msgorder_t order = 0; order <= PQ_ATTR_PRIOQ; ++order) {
        char    send_data[Q_MAXMSG][Q_MSGSIZE];
        struct pq_msg send_array[Q_MAXMSG];
        for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
            memset(send_data[i], i + 1, (i + 1) % (Q_MSGSIZE + 1));
            send_array[i].msg = send_data[i];
            send_array[i].size = (i + 1) % (Q_MSGSIZE + 1);
            send_array[i].prio = (Q_MAXMSG - 1) - i;
        }
        send_message_array(gQueue[order], send_array, Q_MAXMSG);
        TEST_ASSERT_EQUAL(Q_MAXMSG, gQueue[order]->fill);

        char    recv_data[Q_MAXMSG][Q_MSGSIZE];
        struct pq_msg recv_array[Q_MAXMSG];
        for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
            recv_array[i].msg = recv_data[i];
        }
        recv_message_array(gQueue[order], recv_array, Q_MAXMSG);
        TEST_ASSERT_EQUAL(0, gQueue[order]->fill);

        /* Verify order. Should be unchanged. */
        for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
            const msgindex_t j = i;
            TEST_ASSERT_EQUAL(send_array[i].prio, recv_array[j].prio);
            TEST_ASSERT_EQUAL(send_array[i].size, recv_array[j].size);
            if (recv_array[j].size != 0) {
                TEST_ASSERT_EQUAL_MEMORY(send_array[i].msg, recv_array[j].msg, recv_array[j].size);
            }
        }
    }
}

void test_sequence_mod3_prioq(void) {
    msgprio_t prio[3] = { 0 };
    char    send_data[Q_MAXMSG][Q_MSGSIZE];
    struct pq_msg send_array[Q_MAXMSG];
    for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
        memset(send_data[i], i + 1, (i + 1) % (Q_MSGSIZE + 1));
        send_array[i].msg = send_data[i];
        send_array[i].size = (i + 1) % (Q_MSGSIZE + 1);
        send_array[i].prio = i % 3;
        ++prio[i % 3];
    }
    send_message_array(gQueue[PQ_ATTR_PRIOQ], send_array, Q_MAXMSG);
    TEST_ASSERT_EQUAL(Q_MAXMSG, gQueue[PQ_ATTR_PRIOQ]->fill);

    char    recv_data[Q_MAXMSG][Q_MSGSIZE];
    struct pq_msg recv_array[Q_MAXMSG];
    for (msgindex_t i = 0; i < Q_MAXMSG; ++i) {
        recv_array[i].msg = recv_data[i];
    }
    recv_message_array(gQueue[PQ_ATTR_PRIOQ], recv_array, Q_MAXMSG);
    TEST_ASSERT_EQUAL(0, gQueue[PQ_ATTR_PRIOQ]->fill);

    /* Verify priorities. Does not verify FIFO for identical priorities. */
    msgindex_t j = 0;
    for (msgindex_t i = 0; i < prio[2]; ++i) {
        TEST_ASSERT_EQUAL(2, recv_array[j].prio);
        ++j;
    }
    for (msgindex_t i = 0; i < prio[1]; ++i) {
        TEST_ASSERT_EQUAL(1, recv_array[j].prio);
        ++j;
    }
    for (msgindex_t i = 0; i < prio[0]; ++i) {
        TEST_ASSERT_EQUAL(0, recv_array[j].prio);
        ++j;
    }
}

void test_sequence_mod3_prifo(void) {
    char    send_data[10][Q_MSGSIZE] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
    };
    const struct pq_msg send_array[10] = {
        {.msg = send_data[0],.prio = 0,.size = 2},
        {.msg = send_data[1],.prio = 1,.size = 2},
        {.msg = send_data[2],.prio = 2,.size = 2},
        {.msg = send_data[3],.prio = 0,.size = 2},
        {.msg = send_data[4],.prio = 1,.size = 2},
        {.msg = send_data[5],.prio = 2,.size = 2},
        {.msg = send_data[6],.prio = 0,.size = 2},
        {.msg = send_data[7],.prio = 1,.size = 2},
        {.msg = send_data[8],.prio = 2,.size = 2},
        {.msg = send_data[9],.prio = 0,.size = 3},
    };
    send_message_array(gQueue[PQ_ATTR_PRIFO], send_array, ELEMENTS(send_array));
    TEST_ASSERT_EQUAL(ELEMENTS(send_array), gQueue[PQ_ATTR_PRIFO]->fill);

    char    recv_data[ELEMENTS(send_array)][Q_MSGSIZE];
    struct pq_msg recv_array[ELEMENTS(send_array)];
    for (msgindex_t i = 0; i < ELEMENTS(send_array); ++i) {
        recv_array[i].msg = recv_data[i];
    }
    recv_message_array(gQueue[PQ_ATTR_PRIFO], recv_array, ELEMENTS(send_array));
    TEST_ASSERT_EQUAL(0, gQueue[PQ_ATTR_PRIFO]->fill);

    /* Verify priorities and FIFO for identical priorities. */
    const struct pq_msg exp_array[ELEMENTS(send_array)] = {
        {.msg = send_data[2],.prio = 2,.size = 2},
        {.msg = send_data[5],.prio = 2,.size = 2},
        {.msg = send_data[8],.prio = 2,.size = 2},
        {.msg = send_data[1],.prio = 1,.size = 2},
        {.msg = send_data[4],.prio = 1,.size = 2},
        {.msg = send_data[7],.prio = 1,.size = 2},
        {.msg = send_data[0],.prio = 0,.size = 2},
        {.msg = send_data[3],.prio = 0,.size = 2},
        {.msg = send_data[6],.prio = 0,.size = 2},
        {.msg = send_data[9],.prio = 0,.size = 3},
    };
    for (msgindex_t i = 0; i < ELEMENTS(send_array); ++i) {
        TEST_ASSERT_EQUAL(exp_array[i].prio, recv_array[i].prio);
        TEST_ASSERT_EQUAL(exp_array[i].size, recv_array[i].size);
        TEST_ASSERT_EQUAL_STRING(exp_array[i].msg, recv_array[i].msg);
    }
}

/******************************************************************************/

void test_pq_insert_prioq(void) {
    char    a[Q_MSGSIZE];
    memset(a, '!', Q_MSGSIZE);
    char    b[Q_MSGSIZE];
    memset(b, 'x', Q_MSGSIZE);

    const struct pq_msg send_msg = {.msg = a,.prio = 0,.size = Q_MSGSIZE };
    struct pq_msg recv_msg = {.msg = b,.prio = 1,.size = 0 };

    pq_insert_prioq(gQueue[PQ_ATTR_PRIOQ], &send_msg);
    TEST_ASSERT_EQUAL(1, gQueue[PQ_ATTR_PRIOQ]->fill);
    TEST_ASSERT_EQUAL(0, pq_recv_nonbl(gQueue[PQ_ATTR_PRIOQ], &recv_msg));
    TEST_ASSERT_EACH_EQUAL_CHAR('!', b, Q_MSGSIZE);

    TEST_ASSERT_EQUAL(0, recv_msg.prio);
    TEST_ASSERT_EQUAL(Q_MSGSIZE, recv_msg.size);
    TEST_ASSERT_EQUAL(0, gQueue[PQ_ATTR_PRIOQ]->fill);
}

void test_pq_insert_prifo(void) {
    char    a[Q_MSGSIZE];
    memset(a, '!', Q_MSGSIZE);
    char    b[Q_MSGSIZE];
    memset(b, 'x', Q_MSGSIZE);

    const struct pq_msg send_msg = {.msg = a,.prio = 0,.size = Q_MSGSIZE };
    struct pq_msg recv_msg = {.msg = b,.prio = 1,.size = 0 };

    pq_insert_prifo(gQueue[PQ_ATTR_FIFO], &send_msg);
    TEST_ASSERT_EQUAL(1, gQueue[PQ_ATTR_FIFO]->fill);
    TEST_ASSERT_EQUAL(0, pq_recv_nonbl(gQueue[PQ_ATTR_FIFO], &recv_msg));
    TEST_ASSERT_EACH_EQUAL_CHAR('!', b, Q_MSGSIZE);

    TEST_ASSERT_EQUAL(0, recv_msg.prio);
    TEST_ASSERT_EQUAL(Q_MSGSIZE, recv_msg.size);
    TEST_ASSERT_EQUAL(0, gQueue[PQ_ATTR_FIFO]->fill);
}

/******************************************************************************/

void test_pq_remove_prioq(void) {
    char    a[Q_MSGSIZE];
    memset(a, '!', Q_MSGSIZE);
    char    b[Q_MSGSIZE];
    memset(b, 'x', Q_MSGSIZE);

    const struct pq_msg send_msg = {.msg = a,.prio = 0,.size = Q_MSGSIZE };
    struct pq_msg recv_msg = {.msg = b,.prio = 1,.size = 0 };

    TEST_ASSERT_EQUAL(0, gQueue[PQ_ATTR_PRIOQ]->fill);
    TEST_ASSERT_EQUAL(0, pq_send_nonbl(gQueue[PQ_ATTR_PRIOQ], &send_msg));
    TEST_ASSERT_EQUAL(1, gQueue[PQ_ATTR_PRIOQ]->fill);

    pq_remove_prioq(gQueue[PQ_ATTR_PRIOQ], &recv_msg);

    TEST_ASSERT_EACH_EQUAL_CHAR('!', b, Q_MSGSIZE);
    TEST_ASSERT_EQUAL(0, recv_msg.prio);
    TEST_ASSERT_EQUAL(Q_MSGSIZE, recv_msg.size);
    TEST_ASSERT_EQUAL(0, gQueue[PQ_ATTR_PRIOQ]->fill);
}

void test_pq_remove_prifo(void) {
    char    a[Q_MSGSIZE];
    memset(a, '!', Q_MSGSIZE);
    char    b[Q_MSGSIZE];
    memset(b, 'x', Q_MSGSIZE);

    const struct pq_msg send_msg = {.msg = a,.prio = 0,.size = Q_MSGSIZE };
    struct pq_msg recv_msg = {.msg = b,.prio = 1,.size = 0 };

    TEST_ASSERT_EQUAL(0, gQueue[PQ_ATTR_FIFO]->fill);
    TEST_ASSERT_EQUAL(0, pq_send_nonbl(gQueue[PQ_ATTR_FIFO], &send_msg));
    TEST_ASSERT_EQUAL(1, gQueue[PQ_ATTR_FIFO]->fill);

    pq_remove_prifo(gQueue[PQ_ATTR_FIFO], &recv_msg);

    TEST_ASSERT_EACH_EQUAL_CHAR('!', b, Q_MSGSIZE);
    TEST_ASSERT_EQUAL(0, recv_msg.prio);
    TEST_ASSERT_EQUAL(Q_MSGSIZE, recv_msg.size);
    TEST_ASSERT_EQUAL(0, gQueue[PQ_ATTR_FIFO]->fill);
}

/******************************************************************************/

void test_pq_add_time(void) {
    struct timespec ts = { 0, 0 };
    pq_add_time(&ts, 0);
    TEST_ASSERT_EQUAL_INT(0, ts.tv_sec);
    TEST_ASSERT_EQUAL_INT(0, ts.tv_nsec);
    pq_add_time(&ts, 1);
    TEST_ASSERT_EQUAL_INT(0, ts.tv_sec);
    TEST_ASSERT_EQUAL_INT(1000000000L / PQ_TIMEOUT_RESOLUTION, ts.tv_nsec);
    pq_add_time(&ts, 10);
    TEST_ASSERT_EQUAL_INT(0, ts.tv_sec);
    TEST_ASSERT_EQUAL_INT(11 * (1000000000L / PQ_TIMEOUT_RESOLUTION), ts.tv_nsec);
    pq_add_time(&ts, PQ_TIMEOUT_RESOLUTION);
    TEST_ASSERT_EQUAL_INT(1, ts.tv_sec);
    TEST_ASSERT_EQUAL_INT(11 * (1000000000L / PQ_TIMEOUT_RESOLUTION), ts.tv_nsec);
    pq_add_time(&ts, 42 * PQ_TIMEOUT_RESOLUTION);
    TEST_ASSERT_EQUAL_INT(43, ts.tv_sec);
    TEST_ASSERT_EQUAL_INT(11 * (1000000000L / PQ_TIMEOUT_RESOLUTION), ts.tv_nsec);

    ts.tv_sec = 0L;
    ts.tv_nsec = 999999999L;
    pq_add_time(&ts, 0);
    TEST_ASSERT_EQUAL_INT(0, ts.tv_sec);
    TEST_ASSERT_EQUAL_INT(999999999L, ts.tv_nsec);
    pq_add_time(&ts, 1);
    TEST_ASSERT_EQUAL_INT(1, ts.tv_sec);
    TEST_ASSERT_EQUAL_INT((1000000000L / PQ_TIMEOUT_RESOLUTION) - 1, ts.tv_nsec);
}

/******************************************************************************/

void test_pq_swap(void) {
    struct pq_msg msg[3] = {
        {.msg = "foo",.size = 4,.prio = 42},
        {.msg = "barf",.size = 5,.prio = 43},
        {.msg = "bazzz",.size = 6,.prio = 44},
    };
    /* Swapping identical elements should be a no-op. */
    for (msgindex_t i = 0; i < 3; ++i) {
        pq_swap(msg, i, i);
        TEST_ASSERT_EQUAL_STRING("foo", msg[0].msg);
        TEST_ASSERT_EQUAL(4, msg[0].size);
        TEST_ASSERT_EQUAL(42, msg[0].prio);
        TEST_ASSERT_EQUAL_STRING("barf", msg[1].msg);
        TEST_ASSERT_EQUAL(5, msg[1].size);
        TEST_ASSERT_EQUAL(43, msg[1].prio);
        TEST_ASSERT_EQUAL_STRING("bazzz", msg[2].msg);
        TEST_ASSERT_EQUAL(6, msg[2].size);
        TEST_ASSERT_EQUAL(44, msg[2].prio);
    }
    /* Swapping should only modify the selected elements. */
    pq_swap(msg, 0, 1);
    TEST_ASSERT_EQUAL_STRING("barf", msg[0].msg);
    TEST_ASSERT_EQUAL(5, msg[0].size);
    TEST_ASSERT_EQUAL(43, msg[0].prio);
    TEST_ASSERT_EQUAL_STRING("foo", msg[1].msg);
    TEST_ASSERT_EQUAL(4, msg[1].size);
    TEST_ASSERT_EQUAL(42, msg[1].prio);
    TEST_ASSERT_EQUAL_STRING("bazzz", msg[2].msg);
    TEST_ASSERT_EQUAL(6, msg[2].size);
    TEST_ASSERT_EQUAL(44, msg[2].prio);
}

/******************************************************************************/

void test_pq_send_timed(void) {
    pthread_t thread;
    pthread_attr_t attr;
    TEST_ASSERT_EQUAL(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL(0, pthread_create(&thread, &attr, test_pq_send_timed_task, NULL));
    TEST_ASSERT_EQUAL(0, pthread_join(thread, NULL));
}

void   *test_pq_send_timed_task(void *aUnused) {
    char    data[Q_MSGSIZE];
    const struct pq_msg z = {.msg = NULL,.size = 0,.prio = 0 };
    const struct pq_msg m = {.msg = "foo",.size = 4,.prio = 0 };
    TEST_ASSERT_EQUAL(EINVAL, pq_send_timed(NULL, &m, 0));
    for (msgorder_t order = 0; order < ELEMENTS(gQueue); ++order) {
        gQueue[order]->fill = gQueue[order]->maxmsg;
        TEST_ASSERT_EQUAL(EINVAL, pq_send_timed(gQueue[order], NULL, 0));
        TEST_ASSERT_EQUAL(EINVAL, pq_send_timed(gQueue[order], &z, 0));
        TEST_ASSERT_EQUAL(EAGAIN, pq_send_timed(gQueue[order], &m, 0));
        TEST_ASSERT_EQUAL(ETIMEDOUT, pq_send_timed(gQueue[order], &m, 1));
        TEST_ASSERT_EQUAL(ETIMEDOUT, pq_send_timed(gQueue[order], &m, 10));
        /* Fill queue. */
        gQueue[order]->fill = 0;
        for (int i = 0; i < Q_MAXMSG; ++i) {
            TEST_ASSERT_EQUAL(0, pq_send_timed(gQueue[order], &m, 0));
        }
        /* Full? */
        TEST_ASSERT_EQUAL(EAGAIN, pq_send_timed(gQueue[order], &m, 0));

        struct pq_msg r = {.msg = data,.size = 0,.prio = 0 };
        TEST_ASSERT_EQUAL(0, pq_recv_timed(gQueue[order], &r, 0));
        TEST_ASSERT_EQUAL_STRING("foo", data);
    }
    return NULL;
}

/******************************************************************************/

void test_pq_recv_timed(void) {
    pthread_t thread;
    pthread_attr_t attr;
    TEST_ASSERT_EQUAL(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL(0, pthread_create(&thread, &attr, test_pq_recv_timed_task, NULL));
    TEST_ASSERT_EQUAL(0, pthread_join(thread, NULL));
}

void   *test_pq_recv_timed_task(void *aUnused) {
    uint8_t data[1];
    struct pq_msg z = {.msg = NULL,.size = 0,.prio = 0 };
    struct pq_msg m = {.msg = data,.size = 1,.prio = 0 };
    TEST_ASSERT_EQUAL(EINVAL, pq_recv_timed(NULL, &m, 0));
    for (msgorder_t order = 0; order < ELEMENTS(gQueue); ++order) {
        TEST_ASSERT_EQUAL(EINVAL, pq_recv_timed(gQueue[order], NULL, 0));
        TEST_ASSERT_EQUAL(EINVAL, pq_recv_timed(gQueue[order], &z, 0));
        TEST_ASSERT_EQUAL(EAGAIN, pq_recv_timed(gQueue[order], &m, 0));
        TEST_ASSERT_EQUAL(ETIMEDOUT, pq_recv_timed(gQueue[order], &m, 1));
        TEST_ASSERT_EQUAL(ETIMEDOUT, pq_recv_timed(gQueue[order], &m, 10));
    }
    return NULL;
}

/******************************************************************************/

void test_pq_send_blocking(void) {
    /* Sender starts before receiver. Send should block eventually. */
    for (msgorder_t order = 0; order < ELEMENTS(gQueue); ++order) {
        pthread_t thread[2];
        pthread_attr_t attr;
        TEST_ASSERT_EQUAL(0, pthread_attr_init(&attr));
        TEST_ASSERT_EQUAL(0, pthread_create(&thread[0], &attr, test_pq_blocking_send_task, gQueue[order]));
        /* Wait for send_task to fill queue and block. */
        for (msgindex_t fill = 0; fill != Q_MAXMSG;) {
            TEST_ASSERT_EQUAL(0, pq_get_fill(gQueue[order], &fill));
        }
        TEST_ASSERT_TRUE(gQueue[order]->waiting_to_send == 1);
        TEST_ASSERT_TRUE(gQueue[order]->waiting_to_recv == 0);
        /* Now start recv_task. */
        TEST_ASSERT_EQUAL(0, pthread_create(&thread[1], &attr, test_pq_blocking_recv_task, gQueue[order]));
        TEST_ASSERT_EQUAL(0, pthread_join(thread[0], NULL));
        TEST_ASSERT_EQUAL(0, pthread_join(thread[1], NULL));
    }
}

void test_pq_recv_blocking(void) {
    /* Receiver starts before sender. Recv should block immediately. */
    for (msgorder_t order = 0; order < ELEMENTS(gQueue); ++order) {
        pthread_t thread[2];
        pthread_attr_t attr;
        TEST_ASSERT_EQUAL(0, pthread_attr_init(&attr));
        TEST_ASSERT_EQUAL(0, pthread_create(&thread[0], &attr, test_pq_blocking_recv_task, gQueue[order]));
        /* Wait for recv_task to block. */
        while (gQueue[order]->waiting_to_recv == 0) {
            ;
        }
        TEST_ASSERT_TRUE(gQueue[order]->waiting_to_send == 0);
        TEST_ASSERT_TRUE(gQueue[order]->waiting_to_recv == 1);
        /* Now start send_task. */
        TEST_ASSERT_EQUAL(0, pthread_create(&thread[1], &attr, test_pq_blocking_send_task, gQueue[order]));
        TEST_ASSERT_EQUAL(0, pthread_join(thread[0], NULL));
        TEST_ASSERT_EQUAL(0, pthread_join(thread[1], NULL));
    }
}

void   *test_pq_blocking_send_task(void *aQueue) {
    struct pq_queue *const q = aQueue;
    for (size_t i = 0; i < (2 * Q_MAXMSG); ++i) {
        const struct pq_msg m = {.msg = "foo",.size = 4,.prio = 1 };
        TEST_ASSERT_EQUAL(0, pq_send_timed(q, &m, PQ_TIMEOUT_INF));
    }
    return NULL;
}

void   *test_pq_blocking_recv_task(void *aQueue) {
    struct pq_queue *const q = aQueue;
    for (size_t i = 0; i < (2 * Q_MAXMSG); ++i) {
        char    data[Q_MSGSIZE];
        struct pq_msg m = {.msg = data,.size = 0,.prio = 0 };
        TEST_ASSERT_EQUAL(0, pq_recv_timed(q, &m, PQ_TIMEOUT_INF));
        TEST_ASSERT_EQUAL_STRING("foo", m.msg);
        TEST_ASSERT_EQUAL(4, m.size);
        TEST_ASSERT_EQUAL(1, m.prio);
    }
    return NULL;
}

/******************************************************************************/
/*
 * Create S senders sending s messages each.
 * Create R receivers receiving r messages each, where S * s == R * r.
 * If all messages where received by some receiver, all threads eventually
 * return and can be joined. If this test hangs, there's a bug.
 */
#define S  16
#define s  (5 * Q_MAXMSG)
#define R  20
#define r  (4 * Q_MAXMSG)
#if (S * s) != (R * r)
#error "message count mismatch"
#endif

void test_pq_stress(void) {
    for (msgorder_t order = 0; order < 1; ++order) {
        pthread_t thread[S + R];
        pthread_attr_t attr;
        TEST_ASSERT_EQUAL(0, pthread_attr_init(&attr));
        /* Start R recv_tasks. */
        for (int t = 0; t < R; ++t) {
            TEST_ASSERT_EQUAL(0, pthread_create(&thread[t], &attr, test_pq_stress_recv_task, gQueue[order]));
        }
        /* Start S send_tasks. */
        for (int t = R; t < (S + R); ++t) {
            TEST_ASSERT_EQUAL(0, pthread_create(&thread[t], &attr, test_pq_stress_send_task, gQueue[order]));
        }
        TEST_ASSERT_EQUAL(0, pthread_attr_destroy(&attr));
        /* Join all tasks. */
        for (int t = 0; t < (S + R); ++t) {
            TEST_ASSERT_EQUAL(0, pthread_join(thread[t], NULL));
        }
    }
}

void   *test_pq_stress_send_task(void *aQueue) {
    /* Send fixed number of messages with random intervals. */
    struct pq_queue *const q = aQueue;
    for (size_t i = 0; i < s; ++i) {
        const struct pq_msg m = {.msg = "foo",.size = 4,.prio = 1 };
        TEST_ASSERT_EQUAL(0, pq_send_timed(q, &m, PQ_TIMEOUT_INF));
        TEST_ASSERT_EQUAL(0, usleep(1000 * (1 + (rand() & 15))));
    }
    return NULL;
}

void   *test_pq_stress_recv_task(void *aQueue) {
    /* Receive fixed number of messages with random intervals. */
    struct pq_queue *const q = aQueue;
    for (size_t i = 0; i < r; ++i) {
        char    data[Q_MSGSIZE];
        struct pq_msg m = {.msg = data,.size = 0,.prio = 0 };
        TEST_ASSERT_EQUAL(0, pq_recv_timed(q, &m, PQ_TIMEOUT_INF));
        TEST_ASSERT_EQUAL_STRING("foo", m.msg);
        TEST_ASSERT_EQUAL(4, m.size);
        TEST_ASSERT_EQUAL(1, m.prio);
        TEST_ASSERT_EQUAL(0, usleep(1000 * (1 + (rand() & 15))));
    }
    return NULL;
}

/******************************************************************************/

void test_pq_cond_timedwait(void) {
    /* When not called from a task, causes EPERM. */
    for (msgorder_t order = 0; order < ELEMENTS(gQueue); ++order) {
        TEST_ASSERT_EQUAL(EPERM, pq_cond_timedwait(&gQueue[order]->ready_to_send, &gQueue[order]->mtx, 1));
    }
}

/******************************************************************************/

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pq_macros);
    RUN_TEST(test_pq_create);
    RUN_TEST(test_pq_recv_nonbl);
    RUN_TEST(test_pq_recv_timed);
    RUN_TEST(test_pq_send_nonbl);
    RUN_TEST(test_pq_send_timed);
    RUN_TEST(test_pq_insert_prifo);
    RUN_TEST(test_pq_insert_prioq);
    RUN_TEST(test_pq_remove_prifo);
    RUN_TEST(test_pq_remove_prioq);
    RUN_TEST(test_pq_swap);
    RUN_TEST(test_pq_add_time);
    RUN_TEST(test_pq_cond_timedwait);
    RUN_TEST(test_sequence_same_priority);
    RUN_TEST(test_sequence_incr_priority);
    RUN_TEST(test_sequence_decr_priority);
    RUN_TEST(test_sequence_mod3_prioq);
    RUN_TEST(test_sequence_mod3_prifo);
    RUN_TEST(test_pq_send_blocking);
    RUN_TEST(test_pq_stress);
    return UNITY_END();
}

/* vim: set syntax=c tabstop=4 shiftwidth=4 expandtab fileformat=unix: */
