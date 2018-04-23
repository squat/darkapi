#include "mongoose.h"

char *make_str(const char *fmt, ...);

int has_prefix(const struct mg_str *uri, const struct mg_str *prefix);

int is_equal(const struct mg_str *s1, const struct mg_str *s2);
