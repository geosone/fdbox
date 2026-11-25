#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fdbox.h"
#include "lib/nls.h"
#include "unix/tr.h"

/*
This file is part of fdbox
For license - read license.txt
*/

/* tr [-d] [-s] SET1 [SET2] - translate, delete (-d) or squeeze (-s)
 * bytes of the input. A set has ranges (a-z), escapes (\n \t \r \\ \NNN
 * octal) and the classes [:upper:] [:lower:] [:digit:] [:alpha:]
 * [:alnum:] [:space:] [:punct:]. SET2 shorter than SET1 repeats its last
 * byte. Bytes, not characters: for ASCII text. */

static int tr_class(const char *name, unsigned char *out, int n) {
        static const struct {
                const char *name;
                int (*is)(int);
        } classes[] = {{"upper", isupper}, {"lower", islower}, {"digit", isdigit},
                       {"alpha", isalpha}, {"alnum", isalnum}, {"space", isspace},
                       {"punct", ispunct}};
        size_t i;
        int c;

        for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i++) {
                size_t l = strlen(classes[i].name);
                if (strncmp(name, classes[i].name, l) == 0 && strncmp(name + l, ":]", 2) == 0) {
                        for (c = 0; c < 128 && n < 256; c++) {
                                if (classes[i].is(c)) {
                                        out[n++] = (unsigned char)c;
                                }
                        }
                        return n;
                }
        }
        return -1;
}

/* the next byte of a set, its escape read; *s moves past it */
static int tr_char(const char **s) {
        const char *p = *s;
        int c;

        if (*p != '\\' || !p[1]) {
                *s = p + 1;
                return (unsigned char)*p;
        }
        p++;
        switch (*p) {
        case 'n':
                c = '\n';
                break;
        case 't':
                c = '\t';
                break;
        case 'r':
                c = '\r';
                break;
        default:
                if (*p >= '0' && *p <= '7') {
                        int k;
                        c = 0;
                        for (k = 0; k < 3 && *p >= '0' && *p <= '7'; k++, p++) {
                                c = c * 8 + (*p - '0');
                        }
                        *s = p;
                        return c & 0xff;
                }
                c = (unsigned char)*p;
        }
        *s = p + 1;
        return c;
}

static int tr_expand(const char *s, unsigned char *out) {
        int n = 0;

        while (*s && n < 256) {
                int a, b;
                if (strncmp(s, "[:", 2) == 0) {
                        int m = tr_class(s + 2, out, n);
                        if (m < 0) {
                                return -1;
                        }
                        n = m;
                        s = strstr(s, ":]") + 2;
                        continue;
                }
                a = tr_char(&s);
                if (s[0] == '-' && s[1]) {
                        s++;
                        b = tr_char(&s);
                        if (b < a) {
                                return -1;
                        }
                        for (; a <= b && n < 256; a++) {
                                out[n++] = (unsigned char)a;
                        }
                        continue;
                }
                out[n++] = (unsigned char)a;
        }
        return n;
}

int command_tr(int argc, char *argv[]) {
        unsigned char set1[256], set2[256], map[256], in1[256], sq[256];
        const char *a1 = NULL, *a2 = NULL;
        int del = 0, squeeze = 0, n1, n2 = 0, i, c, last = -1;

        for (i = 1; i < argc; i++) {
                if (argv[i][0] == '-' && argv[i][1] && !a1) {
                        const char *p;
                        for (p = argv[i] + 1; *p; p++) {
                                if (*p == 'd') {
                                        del = 1;
                                } else if (*p == 's') {
                                        squeeze = 1;
                                } else {
                                        goto usage;
                                }
                        }
                } else if (!a1) {
                        a1 = argv[i];
                } else if (!a2) {
                        a2 = argv[i];
                } else {
                        goto usage;
                }
        }
        if (!a1 || (!a2 && !del && !squeeze) || (del && a2 && !squeeze)) {
                goto usage;
        }
        n1 = tr_expand(a1, set1);
        if (a2) {
                n2 = tr_expand(a2, set2);
        }
        if (n1 < 0 || n2 < 0 || (a2 && !del && n2 == 0 && n1 > 0)) {
                fputs(_("tr: invalid set\n"), stderr);
                return EXIT_FAILURE;
        }
        for (i = 0; i < 256; i++) {
                map[i] = (unsigned char)i;
                in1[i] = 0;
                sq[i] = 0;
        }
        for (i = 0; i < n1; i++) {
                in1[set1[i]] = 1;
                if (a2 && !del) {
                        map[set1[i]] = set2[i < n2 ? i : n2 - 1];
                }
        }
        /* squeeze the last set given: SET2 when there is one, else SET1 */
        if (squeeze) {
                if (a2) {
                        for (i = 0; i < n2; i++) {
                                sq[set2[i]] = 1;
                        }
                } else {
                        for (i = 0; i < n1; i++) {
                                sq[set1[i]] = 1;
                        }
                }
        }
        while ((c = getchar()) != EOF) {
                if (del && in1[c]) {
                        continue;
                }
                c = map[c];
                if (squeeze && sq[c] && c == last) {
                        continue;
                }
                last = c;
                putchar(c);
        }
        return EXIT_SUCCESS;
usage:
        fputs(_("   tr [-d] [-s] SET1 [SET2]\n"), stderr);
        return EXIT_FAILURE;
}

const char *help_tr() { return _("Translate, delete or squeeze characters"); }
