#include "json.h"
#include "queue.h"
#include <argp.h>
#include <curl/curl.h>
#include <cv.h>
#include <highgui.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#if !defined(VERSION)
#define VERSION "not built correctly"
#endif

bool running = true;

struct response {
    char *buf;
    size_t size;
};

struct detection {
    char *label;
    double x, y, h, w;
};

struct arguments {
    int fps;
    char *in;
    char *out;
    bool preview;
    char *url;
};

struct config {
    double fps;
    CvVideoWriter *out;
    bool preview;
    bool *running;
    struct source *src;
    int theta;
    pthread_t thread;
    char *url;
};

static size_t write_response(void *contents, size_t size, size_t n, void *userp) {
    size_t realsize = size * n;
    struct response *r = (struct response *)userp;

    r->buf = (char *)realloc(r->buf, r->size + realsize + 1);
    if (r->buf == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(r->buf[r->size]), contents, realsize);
    r->size += realsize;
    r->buf[r->size] = 0;

    return realsize;
}

struct response post(CvMat *img, const char *url, CURL *curl) {
    struct curl_httppost *post = NULL;
    struct curl_httppost *last = NULL;
    struct response data;

    data.buf = (char *)malloc(1);
    data.size = 0;
    curl_formadd(&post, &last, CURLFORM_COPYNAME, "data", CURLFORM_COPYCONTENTS, img->data.ptr, CURLFORM_CONTENTSLENGTH, img->step, CURLFORM_END);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&data);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);

    /* what URL that receives this POST */
    curl_easy_setopt(curl, CURLOPT_URL, url);

    /* Perform the request, res will get the return code */
    CURLcode res = curl_easy_perform(curl);
    /* Check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    }

    curl_formfree(post);
    return data;
}

void json_to_detection(json_value *v, struct detection *d) {
    for (int i = 0; i < v->u.object.length; i++) {
        if (strcmp(v->u.object.values[i].name, "label") == 0) {
            d->label = (char *)calloc(v->u.object.values[i].value->u.string.length + 1, 1);
            strncpy(d->label, v->u.object.values[i].value->u.string.ptr, v->u.object.values[i].value->u.string.length);
        } else if (strcmp(v->u.object.values[i].name, "x") == 0) {
            d->x = v->u.object.values[i].value->u.dbl;
        } else if (strcmp(v->u.object.values[i].name, "y") == 0) {
            d->y = v->u.object.values[i].value->u.dbl;
        } else if (strcmp(v->u.object.values[i].name, "h") == 0) {
            d->h = v->u.object.values[i].value->u.dbl;
        } else if (strcmp(v->u.object.values[i].name, "w") == 0) {
            d->w = v->u.object.values[i].value->u.dbl;
        }
    }
}

struct detection *parse(const char *json, const size_t n, uint *dnum) {
    json_value *o;
    struct detection *detections;
    json_settings settings = { 0 };
    char err[json_error_max];

    o = json_parse_ex(&settings, json, n, err);
    if (strlen(err)) {
        printf("error: %s\n", err);
        exit(1);
    }

    if (o->type != json_object || o == NULL) {
        printf("error: response should be a JSON object\n");
        exit(1);
    }
    for (int i = 0; i < o->u.object.length; i++) {
        if (strcmp(o->u.object.values[i].name, "detections")) {
            continue;
        }
        json_value *ds = o->u.object.values[i].value;
        *dnum = ds->u.object.length;
        detections = (struct detection *)malloc(ds->u.object.length * sizeof(detection));
        for (int j = 0; j < ds->u.array.length; j++) {
            json_to_detection(ds->u.array.values[j], &detections[j]);
        }
        break;
    }
    json_value_free(o);
    return detections;
}

void draw_label(IplImage *img, char *label, CvPoint org, CvFont *font, CvScalar color, CvScalar background, int thickness) {
    int baseline;
    CvSize size;
    cvGetTextSize(label, font, &size, &baseline);
    cvRectangle(img, { org.x - thickness, org.y - size.height - baseline - thickness }, { org.x + size.width + thickness, org.y + thickness }, background, CV_FILLED, 8, 0);
    cvPutText(img, label, { org.x, org.y - baseline }, font, color);
}

void draw_detections(IplImage *img, struct detection *ds, uint dnum, CvFont *font) {
    int top, bottom, left, right, width, height;
    for (uint i = 0; i < dnum; i++) {
        height = img->height;
        width = img->width;
        top = (ds[i].y - ds[i].h / 2.) * height;
        bottom = (ds[i].y + ds[i].h / 2.) * height;
        left = (ds[i].x - ds[i].w / 2.) * width;
        right = (ds[i].x + ds[i].w / 2.) * width;
        cvRectangle(img, { left, top }, { right, bottom }, { 255, 0, 0 }, 3, 8, 0);
        draw_label(img, ds[i].label, { left, top }, font, { 0, 0, 0 }, { 255, 0, 0 }, 2);
        free(ds[i].label);
    }
}

int orientation(const char *filename) {
    av_register_all();
    AVFormatContext *fc = avformat_alloc_context();
    if (!fc) {
        return -1;
    }
    if (avformat_open_input(&fc, filename, NULL, NULL) != 0) {
        return -1;
    }
    if (avformat_find_stream_info(fc, NULL) < 0) {
        return -1;
    }

    AVDictionaryEntry *e = NULL;
    int theta = 0;
    for (int i = 0; i < fc->nb_streams; i++) {
        if (fc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVDictionary *d = fc->streams[i]->metadata;
            while ((e = av_dict_get(d, "", e, AV_DICT_IGNORE_SUFFIX))) {
                if (strcmp(e->key, "rotate") == 0) {
                    theta = atoi(e->value);
                    break;
                }
            }
            break;
        }
    }
    avformat_close_input(&fc);
    return theta;
}

IplImage *rotate(IplImage *img, int theta) {
    IplImage *f = NULL;
    IplImage *t = NULL;
    switch (theta) {
        case 90:
            t = cvCreateImage(CvSize(img->height, img->width), img->depth, img->nChannels);
            f = cvCreateImage(CvSize(img->height, img->width), img->depth, img->nChannels);
            cvTranspose(img, t);
            cvFlip(t, f, 1);
            cvReleaseImage(&t);
            return f;
        case 180:
            f = cvCreateImage(CvSize(img->width, img->height), img->depth, img->nChannels);
            cvFlip(img, f, -1);
            return f;
        case 270:
            t = cvCreateImage(CvSize(img->height, img->width), img->depth, img->nChannels);
            f = cvCreateImage(CvSize(img->height, img->width), img->depth, img->nChannels);
            cvTranspose(img, t);
            cvFlip(t, f, 0);
            cvReleaseImage(&t);
            return f;
    }
    return NULL;
}

struct source {
    bool camera;
    CvCapture *cap;
    struct queue *q;
};

void release_image(void *i) {
    IplImage *img = (IplImage *)i;
    cvReleaseImage(&img);
};

struct source *new_source(bool camera, CvCapture *cap) {
    struct source *s = (struct source *)malloc(sizeof(source));
    s->camera = camera;
    s->cap = cap;
    s->q = new_queue(0, release_image);
    return s;
};

void free_source(struct source *s) {
    free_queue(s->q, true);
    free(s);
};

IplImage *source_enqueue_image(struct source *s) {
    IplImage *i = cvQueryFrame(s->cap);
    if (!i) {
        queue_stop(s->q);
        return NULL;
    }
    if (enqueue(s->q, cvCloneImage(i)) < 0) {
        return NULL;
    }
    return i;
}

IplImage *source_get_image(struct source *s) {
    if (s->camera) {
        struct queue_item *qi = dequeue(s->q);
        if (qi) {
            IplImage *i = (IplImage *)qi->data;
            free_queue_item(qi, false);
            return i;
        }
        return NULL;
    }
    return cvQueryFrame(s->cap);
};

void source_release_image(struct source *s, IplImage *i) {
    if (s->camera) {
        cvReleaseImage(&i);
    }
};

void run(struct config *c) {
    IplImage *frame, *rot;
    char esc;
    CvMat *img;
    struct response res;
    uint dnum;
    struct detection *ds;
    int wait;
    double mean;
    CURL *curl;
    CvFont font;
    uint frames = 0;
    struct timespec start, stop, rstop;
    long diff, rdiff;

    curl = curl_easy_init();
    cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1, 1, 0, 2, CV_AA);
    if (c->preview) {
        cvNamedWindow("Darkapi Demo", CV_WINDOW_AUTOSIZE);
    }
    wait = 1.0 / c->fps * 1000;
    while (*c->running) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        frame = source_get_image(c->src);
        if (!frame) {
            break;
        }
        if ((rot = rotate(frame, c->theta))) {
            frame = rot;
        }
        img = cvEncodeImage(".jpg", frame, 0);
        res = post(img, c->url, curl);
        cvReleaseMat(&img);
        ds = parse(res.buf, res.size, &dnum);
        free(res.buf);
        draw_detections(frame, ds, dnum, &font);
        free(ds);
        frames++;
        if (c->out) {
            cvWriteFrame(c->out, frame);
        }
        clock_gettime(CLOCK_MONOTONIC, &rstop);
        diff = 1000000000 * (rstop.tv_sec - start.tv_sec);
        diff += rstop.tv_nsec - start.tv_nsec;
        mean = mean * (frames-1)/frames + diff/1000000.0/frames;
        if (c->preview) {
            cvShowImage("Darkapi Demo", frame);
            esc = cvWaitKey((int)(wait - mean) <= 0 ? 1 : (wait - mean));
        }
        if (rot) {
            cvReleaseImage(&rot);
        }
        source_release_image(c->src, frame);
        if (c->preview && esc == 27) {
            queue_stop(c->src->q);
            printf("caught escape\nexiting gracefully...\n");
        }
        clock_gettime(CLOCK_MONOTONIC, &stop);
        diff = 1000000000 * (stop.tv_sec - start.tv_sec);
        diff += stop.tv_nsec - start.tv_nsec;
        printf("\rFPS: %05.2f", 1000000000.0 / diff);
        fflush(stdout);
    }
    printf("\n");

    curl_easy_cleanup(curl);
    printf("TOTAL FRAMES: %d\n", frames);
};

error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *a = (struct arguments *)state->input;
    int fps;
    switch (key) {
        case 'f':
            fps = atoi(arg);
            if (fps <= 0) {
                argp_usage(state);
            }
            a->fps = fps;
            break;
        case 'i':
            a->in = arg;
            break;
        case 'o':
            a->out = arg;
            break;
        case 'p':
            a->preview = true;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num == 0) {
                a->url = arg;
            } else {
                argp_usage(state);
            }
            break;
        case ARGP_KEY_END:
            if (state->arg_num != 1) {
                argp_usage(state);
            }
            if (!a->preview && !a->out) {
                argp_usage(state);
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
};

void *image_enqueuer(void *cfg) {
    struct config *c = (struct config *)cfg;
    struct timespec ts;
    int ms = 1.0 / c->fps * 1000;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    while (source_enqueue_image(c->src)) {
        nanosleep(&ts, NULL);
    }
    return NULL;
};

static void signal_handler(int _) {
    if (running) {
        running = false;
        printf("caught SIGINT\nexiting gracefully...\n");
        return;
    }
    exit(1);
}

const char *argp_program_version = VERSION;

int main(int argc, char *argv[]) {
    struct config c = { 0, NULL, false, &running, NULL, 0, {}, NULL };
    struct arguments a = { 0, NULL, NULL, false, NULL };
    const char doc[] = "A program to label videos using Darknet image detectors";
    const char args_doc[] = "url";
    struct argp_option options[] = {
        { "fps", 'f', "integer", 0, "Framerate for output video." },
        { "input", 'i', "FILE", 0, "Input video to label" },
        { "output", 'o', "FILE", 0, "Destination for labeled video. One of --output or --preview is required." },
        { "preview", 'p', 0, 0, "Show preview of labeled video. One of --output or --preview is required." },
        { 0 }
    };
    struct argp ap = { options, parse_opt, args_doc, doc };
    int f = 0;

    argp_parse(&ap, argc, argv, 0, 0, &a);

    c.url = a.url;
    c.preview = a.preview;
    if (a.in) {
        c.src = new_source(false, cvCaptureFromFile(a.in));
        c.theta = orientation(a.in);
        f = cvGetCaptureProperty(c.src->cap, CV_CAP_PROP_FOURCC);
    } else {
        c.src = new_source(true, cvCaptureFromCAM(0));
        f = CV_FOURCC('x', '2', '6', '4');
    }
    c.fps = a.fps == 0 ? cvGetCaptureProperty(c.src->cap, CV_CAP_PROP_FPS) : a.fps;
    if (a.out) {
        int width = cvGetCaptureProperty(c.src->cap, CV_CAP_PROP_FRAME_WIDTH);
        int height = cvGetCaptureProperty(c.src->cap, CV_CAP_PROP_FRAME_HEIGHT);
        c.out = cvCreateVideoWriter(a.out, f, c.fps, CvSize(width, height));
    }

    printf("FRAMERATE: %f\n", c.fps);
    curl_global_init(CURL_GLOBAL_ALL);

    signal(SIGINT, signal_handler);
    if (c.src->camera && pthread_create(&c.thread, NULL, image_enqueuer, &c)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    run(&c);
    queue_stop(c.src->q);
    pthread_join(c.thread, NULL);

    if (c.out) {
        cvReleaseVideoWriter(&c.out);
    }
    curl_global_cleanup();
    cvReleaseCapture(&c.src->cap);
    free_source(c.src);
    cvDestroyAllWindows();
}
