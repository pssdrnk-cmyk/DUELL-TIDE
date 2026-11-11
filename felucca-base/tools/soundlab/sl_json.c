/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab: a small JSON reader and writer. */
#include "sl_lab.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

typedef struct { const char *p; int err; } P;

static void ws(P *p) { while (*p->p && isspace((unsigned char)*p->p)) p->p++; }
static sl_jv *mk(int t) { sl_jv *v = (sl_jv *)calloc(1, sizeof *v); v->type = t; return v; }
static sl_jv *val(P *p);

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *d = (char *)malloc(n); memcpy(d, s, n); return d; }

static char *pstr(P *p) {
    size_t cap = 32, n = 0; char *s = (char *)malloc(cap);
    p->p++;
    while (*p->p && *p->p != '"') {
        char c = *p->p++;
        if (c == '\\' && *p->p) {
            c = *p->p++;
            if (c == 'n') c = '\n'; else if (c == 't') c = '\t'; else if (c == 'r') c = '\r';
            else if (c == 'u') {
                unsigned u = 0; int k;
                for (k = 0; k < 4 && isxdigit((unsigned char)*p->p); k++) {
                    char h = *p->p++;
                    u = u * 16 + (unsigned)(isdigit((unsigned char)h) ? h - '0' : (tolower((unsigned char)h) - 'a' + 10));
                }
                c = u < 128 ? (char)u : '?';
            }
        }
        if (n + 2 > cap) { cap *= 2; s = (char *)realloc(s, cap); }
        s[n++] = c;
    }
    if (*p->p == '"') p->p++; else p->err = 1;
    s[n] = 0; return s;
}

static void push(sl_jv *a, const char *key, sl_jv *v) {
    a->kid = (sl_jv **)realloc(a->kid, sizeof(sl_jv *) * (size_t)(a->n + 1));
    a->key = (char **)realloc(a->key, sizeof(char *) * (size_t)(a->n + 1));
    a->kid[a->n] = v; a->key[a->n] = key ? dupstr(key) : NULL; a->n++;
}

static sl_jv *val(P *p) {
    ws(p);
    if (p->err) return NULL;
    if (*p->p == '{') {
        sl_jv *o = mk(SLJ_OBJ); p->p++; ws(p);
        if (*p->p == '}') { p->p++; return o; }
        for (;;) {
            char *k; sl_jv *v; ws(p);
            if (*p->p != '"') { p->err = 1; return o; }
            k = pstr(p); ws(p);
            if (*p->p != ':') { free(k); p->err = 1; return o; }
            p->p++; v = val(p); if (!v) { free(k); return o; }
            push(o, k, v); free(k); ws(p);
            if (*p->p == ',') { p->p++; continue; }
            if (*p->p == '}') { p->p++; return o; }
            p->err = 1; return o;
        }
    }
    if (*p->p == '[') {
        sl_jv *a = mk(SLJ_ARR); p->p++; ws(p);
        if (*p->p == ']') { p->p++; return a; }
        for (;;) {
            sl_jv *v = val(p); if (!v) return a;
            push(a, NULL, v); ws(p);
            if (*p->p == ',') { p->p++; continue; }
            if (*p->p == ']') { p->p++; return a; }
            p->err = 1; return a;
        }
    }
    if (*p->p == '"') { sl_jv *s = mk(SLJ_STR); s->str = pstr(p); return s; }
    if (!strncmp(p->p, "true", 4)) { sl_jv *b = mk(SLJ_BOOL); b->num = 1; p->p += 4; return b; }
    if (!strncmp(p->p, "false", 5)) { sl_jv *b = mk(SLJ_BOOL); p->p += 5; return b; }
    if (!strncmp(p->p, "null", 4)) { p->p += 4; return mk(SLJ_NULL); }
    if (*p->p == '-' || isdigit((unsigned char)*p->p)) {
        char *e; sl_jv *n = mk(SLJ_NUM); n->num = strtod(p->p, &e);
        if (e == p->p) { p->err = 1; free(n); return NULL; }
        p->p = e; return n;
    }
    p->err = 1; return NULL;
}

sl_jv *sl_json_parse(const char *text) {
    P p; sl_jv *v; p.p = text; p.err = 0;
    v = val(&p);
    if (p.err && v) { sl_json_free(v); return NULL; }
    return v;
}

sl_jv *sl_json_load(const char *fn) {
    FILE *f = fopen(fn, "rb"); long n; char *b; sl_jv *v;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (char *)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f); b[n] = 0;
    v = sl_json_parse(b); free(b); return v;
}

const sl_jv *sl_json_get(const sl_jv *o, const char *key) {
    int i;
    if (!o || o->type != SLJ_OBJ) return NULL;
    for (i = 0; i < o->n; i++) if (o->key[i] && !strcmp(o->key[i], key)) return o->kid[i];
    return NULL;
}
const sl_jv *sl_json_at(const sl_jv *a, int i) { return (a && a->type == SLJ_ARR && i >= 0 && i < a->n) ? a->kid[i] : NULL; }
double sl_json_num(const sl_jv *v, double d) { return (v && (v->type == SLJ_NUM || v->type == SLJ_BOOL)) ? v->num : d; }
const char *sl_json_str(const sl_jv *v, const char *d) { return (v && v->type == SLJ_STR) ? v->str : d; }
void sl_json_free(sl_jv *v) {
    int i;
    if (!v) return;
    for (i = 0; i < v->n; i++) { sl_json_free(v->kid[i]); free(v->key[i]); }
    free(v->kid); free(v->key); free(v->str); free(v);
}

/* ---- writer ---- */
static void put(sl_jw *w, const char *s) {
    size_t n = strlen(s);
    if (w->len + n + 1 > w->cap) { w->cap = (w->cap + n + 1) * 2; w->buf = (char *)realloc(w->buf, w->cap); }
    memcpy(w->buf + w->len, s, n + 1); w->len += n;
}
void jw_init(sl_jw *w) { memset(w, 0, sizeof *w); w->cap = 1024; w->buf = (char *)malloc(w->cap); w->buf[0] = 0; w->st[0] = 1; }
static void indent(sl_jw *w) { int i; put(w, "\n"); for (i = 0; i < w->depth; i++) put(w, "  "); }
static void sep(sl_jw *w, const char *key) {
    if (w->depth > 0) {
        if (!(w->st[w->depth] & 1)) put(w, ",");
        w->st[w->depth] &= (unsigned char)~1;
        indent(w);
    }
    if (key) { put(w, "\""); put(w, key); put(w, "\": "); }
}
void jw_open(sl_jw *w, const char *key, int is_array) {
    sep(w, key); put(w, is_array ? "[" : "{");
    w->depth++; w->st[w->depth] = (unsigned char)(1 | (is_array ? 2 : 0));
}
void jw_close(sl_jw *w) {
    int arr = (w->st[w->depth] & 2) != 0, empty = (w->st[w->depth] & 1) != 0;
    w->depth--;
    if (!empty) indent(w);
    put(w, arr ? "]" : "}");
}
void jw_num(sl_jw *w, const char *key, double v) {
    char b[40]; sep(w, key);
    if (!isfinite(v)) { put(w, "null"); return; }
    snprintf(b, sizeof b, "%.7g", v); put(w, b);
}
void jw_int(sl_jw *w, const char *key, long v) { char b[40]; sep(w, key); snprintf(b, sizeof b, "%ld", v); put(w, b); }
void jw_str(sl_jw *w, const char *key, const char *s) {
    size_t i; char c[4]; sep(w, key); put(w, "\"");
    for (i = 0; s && s[i]; i++) {
        if (s[i] == '"' || s[i] == '\\') { c[0] = '\\'; c[1] = s[i]; c[2] = 0; }
        else if ((unsigned char)s[i] < 32) { c[0] = ' '; c[1] = 0; }
        else { c[0] = s[i]; c[1] = 0; }
        put(w, c);
    }
    put(w, "\"");
}
void jw_bool(sl_jw *w, const char *key, int b) { sep(w, key); put(w, b ? "true" : "false"); }
void jw_null(sl_jw *w, const char *key) { sep(w, key); put(w, "null"); }
int jw_save(sl_jw *w, const char *fn) {
    FILE *f;
    while (w->depth > 0) jw_close(w);
    f = fopen(fn, "wb");
    if (!f) { free(w->buf); w->buf = NULL; return -1; }
    fwrite(w->buf, 1, w->len, f); fputc('\n', f); fclose(f);
    free(w->buf); w->buf = NULL; return 0;
}
