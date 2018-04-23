#include "handlers.h"
#include "darknet.h"
#include "detect.h"
#include "mongoose.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#define IMAGE_BUFFER_LENGTH 1024
// https://github.com/pjreddie/darknet/blob/master/examples/darknet.c#L418
#define YOLO_THRESHOLD .25

static const struct mg_str s_post_method = MG_MK_STR("POST");

void health_handler(struct mg_connection *c, int ev, void *ev_data,
                    void *user_data) {
    mg_printf(c, "%s",
              "HTTP/1.0 200 OK\r\n"
              "Content-Length: 0\r\n\r\n");
    c->flags |= MG_F_SEND_AND_CLOSE;
}

void detect_handler(struct mg_connection *c, int ev, void *ev_data,
                    void *user_data) {
    static const struct mg_str tiny_prefix = MG_MK_STR("/api/tiny");
    static const struct mg_str yolo_prefix = MG_MK_STR("/api/yolo");
    static const struct mg_str yolo_9000_prefix = MG_MK_STR("/api/yolo9000");
    struct context *ctx = (struct context *)c->user_data;
    struct http_message *hm = (struct http_message *)ev_data;
    struct mg_http_multipart_part *mp = (struct mg_http_multipart_part *)ev_data;

    switch (ev) {
        case MG_EV_HTTP_REQUEST: {
            if (!is_equal(&hm->method, &s_post_method)) {
                mg_printf(c, "%s",
                          "HTTP/1.1 405 Method not allowed\r\n"
                          "Content-Length: 0\r\n\r\n");
                c->flags |= MG_F_SEND_AND_CLOSE;
                return;
            }
            break;
        }
        case MG_EV_HTTP_MULTIPART_REQUEST: {
            int len = IMAGE_BUFFER_LENGTH;
            char **labels;
            network *net;
            struct mg_str *hv = mg_get_http_header(hm, "Content-Length");
            if (hv != NULL) {
                len = atoi(hv->p);
            }

            if (is_equal(&hm->uri, &yolo_9000_prefix)) {
                labels = ((struct net_context *)user_data)->labels_imagenet;
                net = ((struct net_context *)user_data)->yolo_9000;
            } else if (has_prefix(&hm->uri, &tiny_prefix)) {
                labels = ((struct net_context *)user_data)->labels_coco;
                net = ((struct net_context *)user_data)->tiny;
            } else if (has_prefix(&hm->uri, &yolo_prefix)) {
                labels = ((struct net_context *)user_data)->labels_coco;
                net = ((struct net_context *)user_data)->yolo;
            } else {
                mg_printf(c, "%s",
                          "HTTP/1.1 400 Bad Request\r\n"
                          "Content-Length: 0\r\n\r\n");
                c->flags |= MG_F_SEND_AND_CLOSE;
                return;
            }

            if (ctx == NULL) {
                ctx = calloc(1, sizeof(struct context));
                ctx->buffer = calloc(1, len);
                ctx->labels = labels;
                ctx->len = 0;
                ctx->net = net;
                ctx->threshold = YOLO_THRESHOLD;

                if (ctx->buffer == NULL) {
                    mg_printf(c, "%s",
                              "HTTP/1.1 500 Failed to load file\r\n"
                              "Content-Length: 0\r\n\r\n");
                    c->flags |= MG_F_SEND_AND_CLOSE;
                    return;
                }
                c->user_data = (void *)ctx;
            }

            hv = mg_get_http_header(hm, "Expect");
            if (hv != NULL && mg_vcasecmp(hv, "100-continue") == 0) {
                mg_printf(c, "%s", "HTTP/1.1 100 Continue\r\n\r\n");
            }

            break;
        }
        case MG_EV_HTTP_PART_DATA: {
            memcpy(ctx->buffer + ctx->len, mp->data.p, mp->data.len);
            ctx->len += mp->data.len;
            break;
        }
        case MG_EV_HTTP_PART_END: {
            if (ctx != NULL) {
                int x, y, ch;
                char *json = analyze_image(ctx->buffer, ctx->len, ctx->net, ctx->labels, ctx->threshold);
                if (json != NULL) {
                    mg_printf(c,
                              "HTTP/1.1 200 OK\r\n"
                              "Connection: close\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: %d\r\n\r\n"
                              "%s",
                              (int)strlen(json), json);
                    free(json);
                } else {
                    mg_printf(c, "HTTP/1.1 500 Failed to analyze file\r\n"
                                 "Content-Length: 0\r\n\r\n");
                }
                c->flags |= MG_F_SEND_AND_CLOSE;
                free(ctx->buffer);
                free(ctx);
                c->user_data = NULL;
            }
            break;
        }
    }
}
