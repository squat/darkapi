#include "darknet.h"
#include "handlers.h"
#include "mongoose.h"
#include <signal.h>
#include <stdio.h>

static const char *s_signal = "";

static void signal_handler(int s) {
    switch (s) {
        case SIGINT:
            s_signal = "SIGINT";
            break;
        case SIGTERM:
            s_signal = "SIGTERM";
            break;
    }
}

static void ev_handler(struct mg_connection *c, int ev, void *ev_data,
                       void *user_data) {
    switch (ev) {
        case MG_EV_HTTP_REQUEST:
            mg_printf(c, "%s",
                      "HTTP/1.0 404 Not Found\r\n"
                      "Content-Length: 0\r\n\r\n");
            c->flags |= MG_F_SEND_AND_CLOSE;
    }
}

int main(int argc, char *argv[]) {
    struct mg_mgr mgr;
    struct mg_connection *c;
    struct net_context ctx;
    int i;
    const char *port = "8080";

    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-D") == 0) {
            mgr.hexdump_file = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            port = argv[++i];
        }
    }

    /* Initialise networks */
    ctx.tiny = load_network("vendor/darknet/cfg/yolov2-tiny.cfg",
                            "vendor/darknet/yolov2-tiny.weights", 0);

    ctx.yolo = load_network("vendor/darknet/cfg/yolov3.cfg",
                            "vendor/darknet/yolov3.weights", 0);
    ctx.yolo_9000 = load_network("vendor/darknet/cfg/yolo9000.cfg",
                            "vendor/darknet/yolo9000.weights", 0);
    /* Initialise labels */
    list *options = read_data_cfg("vendor/darknet/cfg/coco.data");
    char *labels_list = option_find_str(options, "names", "failed to load names");
    ctx.labels_coco = get_labels(labels_list);

    options = read_data_cfg("vendor/darknet/cfg/combine9k.data");
    labels_list = option_find_str(options, "names", "failed to load names");
    ctx.labels_imagenet = get_labels(labels_list);

    /* Open listening socket */
    mg_mgr_init(&mgr, NULL);
    c = mg_bind(&mgr, port, ev_handler, NULL);
    mg_register_http_endpoint(c, "/api/health", health_handler, NULL);
    mg_register_http_endpoint(c, "/api/yolo9000", detect_handler, (void *)&ctx);
    mg_register_http_endpoint(c, "/api/tiny", detect_handler, (void *)&ctx);
    mg_register_http_endpoint(c, "/api/yolo", detect_handler, (void *)&ctx);
    mg_set_protocol_http_websocket(c);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Run event loop until signal is received */
    printf("Starting %s on port %s\n", argv[0], port);
    while (*s_signal == '\0') {
        mg_mgr_poll(&mgr, 1000);
    }

    /* Cleanup */
    free(ctx.yolo);
    free(ctx.labels_coco);
    free_list(options);
    mg_mgr_free(&mgr);

    printf("Caught %s\n", s_signal);
    printf("Exiting gracefully...\n");

    return 0;
}
