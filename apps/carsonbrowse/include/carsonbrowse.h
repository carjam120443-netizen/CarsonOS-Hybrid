#ifndef CARSONBROWSE_H
#define CARSONBROWSE_H
#include <stddef.h>
#define CB_HOST_MAX 192
#define CB_PATH_MAX 320
#define CB_TITLE_MAX 96
#define CB_TEXT_MAX 8192
#define CB_HISTORY_MAX 16
#define CB_RESPONSE_MAX 12288
typedef struct { char scheme[8], host[CB_HOST_MAX], path[CB_PATH_MAX]; unsigned port; } cb_url;
typedef struct { unsigned status; const char *body; size_t body_length; } cb_http_response;
typedef struct { char title[CB_TITLE_MAX], text[CB_TEXT_MAX]; size_t text_length; } cb_document;
typedef struct { cb_url entries[CB_HISTORY_MAX]; size_t count, cursor; } cb_history;
int cb_url_parse(const char *input, cb_url *out);
int cb_http_parse(const char *bytes, size_t length, cb_http_response *out);
int cb_html_to_text(const char *html, size_t length, cb_document *out);
int cb_history_visit(cb_history *history, const cb_url *url);
int cb_history_back(cb_history *history, cb_url *out);
int cb_history_forward(cb_history *history, cb_url *out);
typedef int (*cb_fetch_fn)(const cb_url *, char *, size_t, size_t *, void *);
int cb_load(const char *address, cb_fetch_fn fetch, void *context, cb_document *document, unsigned *status);
#endif
