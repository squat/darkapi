#include "darknet.h"
#include "util.h"
// https://github.com/pjreddie/darknet/blob/508381b37fe75e0e1a01bcb2941cb0b31eb0e4c9/examples/darknet.c#L440
#define YOLO_HIER_THRESHOLD .5
// https://github.com/pjreddie/darknet/blob/508381b37fe75e0e1a01bcb2941cb0b31eb0e4c9/examples/detector.c#L575
#define YOLO_NMS .45

const char *stbi_failure_reason(void);
unsigned char *stbi_load_from_memory(unsigned char const *buffer, int len,
                                     int *x, int *y, int *comp, int req_comp);

static image load_image_memory(const unsigned char *buffer, const int len, int *x, int *y, int *c) {
    image im;
    unsigned char *data = stbi_load_from_memory(buffer, len, x, y, c, 0);
    if (!data) {
        fprintf(stderr, "failed to load image: %s\n", stbi_failure_reason());
        im.data = NULL;
        return im;
    }
    im = make_image(*x, *y, *c);

    int i, j, k;
    for (k = 0; k < *c; k++) {
        for (j = 0; j < *y; j++) {
            for (i = 0; i < *x; i++) {
                // Source image is rgb rgb rgb rgb.
                int src = k + *c * i + *c * *x * j;
                // Destination image is rrrr gggg bbbb.
                int dst = i + *x * j + *x * *y * k;
                im.data[dst] = (float)data[src] / 255.;
            }
        }
    }
    free(data);
    return im;
}

/*static struct detection *get_detections(const int n, const int classes, const box *boxes, float **probabilities, const char **names, const float threshold) {*/
    /*struct detection *ds = calloc(n, sizeof(struct detection));*/
    /*int i, j, k = 0;*/

    /*for (i = 0; i < n; i++) {*/
        /*for (j = 0; j < classes; j++) {*/
            /*if (probabilities[i][j] > threshold) {*/
                /*ds[k].box = &boxes[i];*/
                /*ds[k].label = names[j];*/
                /*ds[k].prob = probabilities[i][j];*/
                /*k++;*/
            /*}*/
        /*}*/
    /*}*/
    /*return ds;*/
/*}*/

static char *detections_to_json(const detection *ds, const int n, const int classes, const char **names, const float threshold) {
    char *d = NULL;
    char *json = NULL;
    char *json2 = NULL;
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < classes; j++) {
            if (ds[i].prob[j] > threshold) {
                d = make_str("{\"label\":\"%s\",\"p\":%f,\"x\":%f,\"y\":%f,\"w\":%f,\"h\":%f}", names[j], ds[i].prob[j], ds[i].bbox.x, ds[i].bbox.y, ds[i].bbox.w, ds[i].bbox.h);
                if (json != NULL) {
                    json2 = make_str("%s,%s", json, d);
                    free(json);
                    json = json2;
                    free(d);
                    d = NULL;
                    continue;
                }
                json = d;
            }
        }
    }

    json2 = make_str("[%s]", json ? json : "");
    if (json != NULL) {
        free(json);
    }
    return json2;
}

static char *test_image(const image im, network *net, const char **labels, float threshold) {
    int j;
    image sized = letterbox_image(im, net->w, net->h);
    layer l = net->layers[net->n - 1];

    float *X = sized.data;
    double time = what_time_is_it_now();
    network_predict(net, X);
    time = what_time_is_it_now() - time;
    int nboxes = 0;
    detection *ds = get_network_boxes(net, im.w, im.h, threshold, YOLO_HIER_THRESHOLD, 0, 1, &nboxes);
    do_nms_sort(ds, nboxes, l.classes, YOLO_NMS);

    char *ds_json = detections_to_json(ds, nboxes, l.classes, labels, threshold);
    char *json = make_str("\"time\":%f,\"detections\":%s", time, ds_json);

    free(ds_json);
    free_detections(ds, nboxes);
    free_image(sized);
    return json;
}

char *analyze_image(const unsigned char *buffer, const int len, network *net, const char **labels, float threshold) {
    int x, y, c;

    image im = load_image_memory(buffer, len, &x, &y, &c);
    if (!im.data) {
        return NULL;
    }
    char *prediction_json = test_image(im, net, labels, threshold);
    char *json = make_str("{\"size\":%zu,\"x\":%d,\"y\":%d,\"c\":%d,%s}", len, x, y, c, prediction_json);
    free(prediction_json);
    free_image(im);
    return json;
}
