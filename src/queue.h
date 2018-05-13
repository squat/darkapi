#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*queue_data_free)(void *);

struct queue_item {
    void *data;
    queue_data_free f;
    struct queue_item *next;
};

struct queue {
    pthread_cond_t c;
    int cap;
    queue_data_free f;
    struct queue_item *head;
    pthread_mutex_t m;
    int n;
    struct queue_item *tail;
};

struct queue *new_queue(int cap, queue_data_free f);

void queue_stop(struct queue *q);

int enqueue(struct queue *q, void *d);

struct queue_item *dequeue(struct queue *q);

struct queue_item *free_queue_item(struct queue_item *qi, bool free_data);

void free_queue(struct queue *q, bool free_data);

#ifdef __cplusplus
}
#endif
