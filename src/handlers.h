#include "darknet.h"
#include "mongoose.h"

struct context {
    unsigned char *buffer;
    char **labels;
    size_t len;
    network *net;
    float threshold;
};

struct net_context {
    char **labels_coco;
    char **labels_imagenet;
    network *tiny;
    network *yolo;
    network *yolo_9000;
};

void health_handler(struct mg_connection *c, int ev, void *ev_data,
                    void *user_data);

void detect_handler(struct mg_connection *c, int ev, void *ev_data,
                    void *user_data);
