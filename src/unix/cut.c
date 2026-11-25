#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fdbox.h"
#include "lib/args.h"
#include "lib/nls.h"
#include "unix/cut.h"

/*
This file is part of fdbox
For license - read license.txt
*/

/* cut -b LIST | -c LIST | -f LIST [-d DELIM] [-s] [file...] - parts of
 * each line: bytes, characters (UTF-8) or fields (TAB by default). LIST
 * is N, N-M, N- or -M, separated by commas. -s leaves out lines without
 * the delimiter. */

#define CUT_RANGES 32

struct cut_list {
        long from[CUT_RANGES], to[CUT_RANGES];
        int n;
};

static int cut_parse(const char *s, struct cut_list *l) {
        l->n = 0;
        while (*s) {
                char *end;
                long a = 1, b;
                if (l->n == CUT_RANGES) {
                        return 0;
                }
                if (*s != '-') {
                        a = strtol(s, &end, 10);
                        if (end == s || a < 1) {
                                return 0;
                        }
                        s = end;
                }
                b = a;
                if (*s == '-') {
                        s++;
                        b = 0x7fffffffL;
                        if (*s >= '0' && *s <= '9') {
                                b = strtol(s, &end, 10);
                                s = end;
                        }
                }
                if (b < a || (*s && *s != ',')) {
                        return 0;
                }
                l->from[l->n] = a;
                l->to[l->n++] = b;
                if (*s == ',') {
                        s++;
                }
        }
        return l->n > 0;
}

static int cut_in(const struct cut_list *l, long k) {
        int i;
        for (i = 0; i < l->n; i++) {
                if (k >= l->from[i] && k <= l->to[i]) {
                        return 1;
                }
        }
        return 0;
}

struct cut_opts {
        struct cut_list list;
        char mode, delim;
        int only;
};

static void cut_line(const struct cut_opts *o, const char *s, size_t n) {
        size_t i, start = 0;
        long k = 1;
        int first = 1;

        if (o->mode == 'f') {
                if (!memchr(s, o->delim, n)) {
                        if (!o->only) {
                                fwrite(s, 1, n, stdout);
                                putchar('\n');
                        }
                        return;
                }
                for (i = 0; i <= n; i++) {
                        if (i == n || s[i] == o->delim) {
                                if (cut_in(&o->list, k)) {
                                        if (!first) {
                                                putchar(o->delim);
                                        }
                                        fwrite(s + start, 1, i - start, stdout);
                                        first = 0;
                                }
                                k++;
                                start = i + 1;
                        }
                }
        } else {
                for (i = 0; i < n; k++) {
                        size_t len = 1;
                        if (o->mode == 'c') {
                                while (i + len < n && ((unsigned char)s[i + len] & 0xc0) == 0x80) {
                                        len++;
                                }
                        }
                        if (cut_in(&o->list, k)) {
                                fwrite(s + i, 1, len, stdout);
                        }
                        i += len;
                }
        }
        putchar('\n');
}

static void cut_stream(const struct cut_opts *o, FILE *f) {
        char line[1024];

        while (fgets(line, sizeof(line), f) != NULL) {
                size_t n = strlen(line);
                if (n && line[n - 1] == '\n') {
                        n--;
                }
                if (n && line[n - 1] == '\r') {
                        n--;
                }
                cut_line(o, line, n);
        }
}

int command_cut(int argc, char *argv[]) {
        struct cut_opts o;
        const char *files[ARG_MAX_FILES];
        const char *list = NULL;
        int nfiles = 0, i, rc = EXIT_SUCCESS;

        o.mode = 0;
        o.delim = '\t';
        o.only = 0;
        for (i = 1; i < argc; i++) {
                char *a = argv[i];
                if (a[0] == '-' && a[1] && !a[2] && strchr("bcfd", a[1])) {
                        const char *v = i + 1 < argc ? argv[++i] : NULL;
                        if (!v) {
                                goto usage;
                        }
                        if (a[1] == 'd') {
                                o.delim = v[0];
                        } else {
                                o.mode = a[1];
                                list = v;
                        }
                } else if (a[0] == '-' && a[1] && strchr("bcfd", a[1])) {
                        if (a[1] == 'd') {
                                o.delim = a[2];
                        } else {
                                o.mode = a[1];
                                list = a + 2;
                        }
                } else if (strcmp(a, "-s") == 0) {
                        o.only = 1;
                } else if (nfiles < ARG_MAX_FILES) {
                        files[nfiles++] = a;
                }
        }
        if (!o.mode || !cut_parse(list, &o.list)) {
                goto usage;
        }
        if (!nfiles) {
                cut_stream(&o, stdin);
        }
        for (i = 0; i < nfiles; i++) {
                FILE *f = fopen(files[i], "r");
                if (!f) {
                        fprintf(stderr, _("cut: cannot open %s\n"), files[i]);
                        rc = EXIT_FAILURE;
                        continue;
                }
                cut_stream(&o, f);
                fclose(f);
        }
        return rc;
usage:
        fputs(_("   cut -b LIST | -c LIST | -f LIST [-d DELIMITER] [-s] [file...]\n"), stderr);
        return EXIT_FAILURE;
}

const char *help_cut() { return _("Print parts of each line (bytes, characters, fields)"); }
