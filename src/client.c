#include "json.h"
#include <curl/curl.h>
#include <cv.h>
#include <highgui.h>
#include <stdio.h>

static CvFont font;

struct response {
    char *buf;
    size_t size;
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

struct detection {
    char *label;
    double x, y, h, w;
};

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
    char err [json_error_max];

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
    cvRectangle(img, { org.x - thickness, org.y + thickness }, { org.x + size.width + thickness, org.y - size.height - baseline - thickness }, background, CV_FILLED, 8, 0);
    cvPutText(img, label, { org.x, org.y - baseline }, font, color);
}

void draw_detections(IplImage *img, struct detection *ds, uint dnum) {
    int top, bottom, left, right, width, height;
    for (uint i = 0; i < dnum; i++) {
        height = img->height;
        width = img->width;
        top = (ds[i].y - ds[i].h / 2.) * height;
        bottom = (ds[i].y + ds[i].h / 2.) * height;
        left = (ds[i].x - ds[i].w / 2.) * width;
        right = (ds[i].x + ds[i].w / 2.) * width;
        cvRectangle(img, { left, top }, { right, bottom }, { 255, 0, 0 }, 3, 8, 0);
        draw_label(img, ds[i].label, { left, top }, &font, { 0, 0, 0 }, { 255, 0, 0 }, 3);
        free(ds[i].label);
    }
}

int main(int argc, char *argv[]) {
    const char *url;
    IplImage *frame;
    CvCapture *cap;
    char c;
    CvMat *img;
    CURL *curl;
    struct response res;
    uint dnum;
    struct detection *ds;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    cap = cvCaptureFromCAM(0);
    cvNamedWindow("Darkapi Demo", CV_WINDOW_AUTOSIZE);

    if (argc < 2) {
        printf("error: a Darkapi URL is required\n");
        exit(1);
    }

    url = argv[1];

    cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1, 1, 0, 2, CV_AA);
    while (1) {
        frame = cvQueryFrame(cap);
        img = cvEncodeImage(".jpg", frame, 0);
        res = post(img, url, curl);
        ds = parse(res.buf, res.size, &dnum);
        draw_detections(frame, ds, dnum);
        cvShowImage("Darkapi Demo", frame);
        c = cvWaitKey(30);
        free(ds);
        free(res.buf);
        cvReleaseMat(&img);

        if (c == 27) {
            break;
        }
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    cvReleaseCapture(&cap);
    cvDestroyAllWindows();
}
