#include "mongoose.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

char *make_str(const char *fmt, ...) {
    char *p;
    char *np;
    int n, size = 64;
    va_list ap;

    if ((p = malloc(size)) == NULL) {
        return NULL;
    };

    while (1) {
        va_start(ap, fmt);
        n = vsnprintf(p, size, fmt, ap);
        va_end(ap);
        if (n < 0) {
            free(p);
            return NULL;
        }
        if (n < size) {
            return p;
        }
        size = n + 1;
        if ((np = realloc(p, size)) == NULL) {
            free(p);
            return NULL;
        }
        p = np;
    }
}

int has_prefix(const struct mg_str *uri, const struct mg_str *prefix) {
    return uri->len >= prefix->len && memcmp(uri->p, prefix->p, prefix->len) == 0;
}

int is_equal(const struct mg_str *s1, const struct mg_str *s2) {
    return s1->len == s2->len && memcmp(s1->p, s2->p, s2->len) == 0;
}

void free_string_array(char **ca, const int n) {
    for (int i = 0; i < n; i++) {
        free(ca[i]);
    }
    free(ca);
}
