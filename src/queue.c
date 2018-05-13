#include "queue.h"
#include <stdbool.h>
#include <stdlib.h>

struct queue *new_queue(int cap, queue_data_free f) {
    struct queue *q = (struct queue *)malloc(sizeof(struct queue));
    pthread_cond_init(&q->c, NULL);
    q->cap = cap;
    q->f = f;
    q->head = NULL;
    pthread_mutex_init(&q->m, NULL);
    q->n = 0;
    q->tail = NULL;
    return q;
};

void queue_stop(struct queue *q) {
    pthread_mutex_lock(&q->m);
    q->n = -1;
    pthread_cond_broadcast(&q->c);
    pthread_mutex_unlock(&q->m);
};

int enqueue(struct queue *q, void *d) {
    pthread_mutex_lock(&q->m);
    while (q->cap && q->n >= q->cap) {
        pthread_cond_wait(&q->c, &q->m);
    }
    if (q->n < 0) {
        pthread_mutex_unlock(&q->m);
        return q->n;
    }
    struct queue_item *qi = (struct queue_item *)malloc(sizeof(struct queue_item));
    qi->data = d;
    qi->f = q->f;
    qi->next = NULL;
    if (q->head) {
        q->tail->next = qi;
        q->tail = qi;
    } else {
        q->head = qi;
        q->tail = qi;
    }
    q->n++;
    pthread_cond_broadcast(&q->c);
    pthread_mutex_unlock(&q->m);
    return q->n;
};

struct queue_item *dequeue(struct queue *q) {
    pthread_mutex_lock(&q->m);
    if (q->n < 0) {
        pthread_mutex_unlock(&q->m);
        return NULL;
    }
    while (q->n <= 0) {
        pthread_cond_wait(&q->c, &q->m);
    }
    struct queue_item *qi = q->head;
    q->head = q->head->next;
    q->n--;
    pthread_cond_broadcast(&q->c);
    pthread_mutex_unlock(&q->m);
    return qi;
};

struct queue_item *free_queue_item(struct queue_item *qi, bool free_data) {
    if (!qi) {
        return NULL;
    }
    struct queue_item *next = qi->next;
    if (free_data && qi->f) {
        qi->f(qi->data);
    }
    free(qi);
    return next;
};

void free_queue(struct queue *q, bool free_data) {
    struct queue_item *qi = q->head;
    while ((qi = free_queue_item(qi, free_data))) {
    }
    pthread_cond_destroy(&q->c);
    pthread_mutex_destroy(&q->m);
    free(q);
};
