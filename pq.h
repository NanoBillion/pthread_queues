#ifndef PQ_H
#define PQ_H

#include <stdint.h>
#include <pthread.h>

/* Timeout resolution (frequency); 1L=1s, 1000L=1ms, 1000000L=1us. */
#define PQ_TIMEOUT_RESOLUTION 1000L

/* No timeout, i.e. non-blocking call desired. */
#define PQ_TIMEOUT_ZERO ((pq_time_t)0u)

/* Maximum timeout, i.e. blocking call desired. */
#define PQ_TIMEOUT_INF  (~(pq_time_t)0u)

/* Return messages of the same priority in FIFO order. */
#define PQ_ATTR_PRIFO 0

/* Return messages of the same priority in heap order. */
#define PQ_ATTR_PRIOQ 1

/* Return messages in FIFO order. Queue with head and tail. Ignore prio. */
#define PQ_ATTR_FIFO  2

/* Return messages in LIFO order. A stack. Ignore prio. */
#define PQ_ATTR_LIFO  3

/* Maximum value that fits in a msgprio_t. */
#define PQ_MAXPRIO 65535u

/* Avoid some repetitive code in case of errors. */
#define pq_unlock_and_return_if_unsuccessful(aStatus) \
    do { \
        if ((aStatus) != 0) { \
            pthread_mutex_unlock(&aQueue->mtx); \
            return (aStatus); \
        } \
    } while (0)

/* Type returned by pq_* functions. */
typedef int pq_status_t;

/* Type for timeout. */
typedef uint32_t pq_time_t;

/* Type for message index variables. */
typedef uint16_t msgindex_t;

/* Type for message size variables. */
typedef uint16_t msgsize_t;

/* Type for message priority variables. */
typedef uint16_t msgprio_t;

/* Type for thread count variables. */
typedef uint16_t thrcount_t;

/* Type for message order attribute. */
typedef uint16_t msgorder_t;

/* Queue attributes. */
struct pq_attr {
    /* Max number of messages queue can hold. */
    msgindex_t maxmsg;
    /* Max size of message in bytes. */
    msgsize_t msgsize;
    /* How to return messages of the same priority. */
    msgorder_t order;
    /* Maximum priority. */
    msgprio_t maxprio;
};

/* Element type of queue's message array. */
struct pq_msg {
    void   *msg;
    msgsize_t size;
    msgprio_t prio;
};

/* Priority queue descriptor. */
struct pq_queue {
    /* Max number of messages queue can hold. */
    msgindex_t maxmsg;
    /* Max size of message in bytes. */
    msgsize_t msgsize;
    /* How to insert and remove messages. */
    msgorder_t order;
    /* Maximum priority. */
    msgprio_t maxprio;
    /* Array of messages. */
    struct pq_msg *message;
    /* Number of messages in queue. */
    msgindex_t fill;
    /* Index of head element. */
    msgindex_t head;
    /* Index of tail element. */
    msgindex_t tail;
    /* Mutex to protect queue state. */
    pthread_mutex_t mtx;
    /* Mutex attribute. */
    pthread_mutexattr_t attr;
    /* Number of threads waiting to send to a full queue. */
    thrcount_t waiting_to_send;
    /* Condition indicating queue no longer full. */
    pthread_cond_t ready_to_send;
    /* Number of threads waiting to recv from an empty queue. */
    thrcount_t waiting_to_recv;
    /* Condition indicating queue no longer empty. */
    pthread_cond_t ready_to_recv;
};

/* Public functions. */
pq_status_t pq_create(struct pq_queue **aQueue, const struct pq_attr *aAttributes);
pq_status_t pq_destroy(struct pq_queue *aQueue);

pq_status_t pq_recv_nonbl(struct pq_queue *aQueue, struct pq_msg *aMessage);
pq_status_t pq_recv_timed(struct pq_queue *aQueue, struct pq_msg *aMessage, pq_time_t aTimeout);

pq_status_t pq_send_nonbl(struct pq_queue *aQueue, const struct pq_msg *aMessage);
pq_status_t pq_send_timed(struct pq_queue *aQueue, const struct pq_msg *aMessage, pq_time_t aTimeout);

/* Helper/debug functions. */
pq_status_t pq_dump(struct pq_queue *aQueue);
pq_status_t pq_get_fill(struct pq_queue *aQueue, msgindex_t *aFill);

/* Private functions. */
pq_status_t pq_cleanup(struct pq_queue *aQueue, pq_status_t aItems, pq_status_t aStatus);
void    pq_insert(struct pq_queue *aQueue, const struct pq_msg *aMessage);
void    pq_insert_prioq(struct pq_queue *aQueue, const struct pq_msg *aMessage);
void    pq_insert_fifo(struct pq_queue *aQueue, const struct pq_msg *aMessage);
void    pq_insert_prifo(struct pq_queue *aQueue, const struct pq_msg *aMessage);
void    pq_insert_lifo(struct pq_queue *aQueue, const struct pq_msg *aMessage);
void    pq_remove(struct pq_queue *aQueue, struct pq_msg *aMessage);
void    pq_remove_prioq(struct pq_queue *aQueue, struct pq_msg *const aMessage);
void    pq_remove_fifo(struct pq_queue *aQueue, struct pq_msg *const aMessage);
void    pq_remove_lifo(struct pq_queue *aQueue, struct pq_msg *const aMessage);
void    pq_remove_prifo(struct pq_queue *aQueue, struct pq_msg *const aMessage);
void    pq_add_time(struct timespec *aTime, pq_time_t aIncrement);
void    pq_swap(struct pq_msg *aMessage, msgindex_t aFirst, msgindex_t aSecond);
pq_status_t pq_cond_timedwait(pthread_cond_t *aCond, pthread_mutex_t *aMutex, pq_time_t aTimeout);

#endif /* PQ_H */

/* vim: set syntax=c tabstop=4 shiftwidth=4 expandtab fileformat=unix: */
