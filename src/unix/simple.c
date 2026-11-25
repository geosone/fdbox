#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fdbox.h"
#include "lib/nls.h"
#include "unix/simple.h"

/*
This file is part of fdbox
For license - read license.txt
*/

/* The smallest unix commands, as BusyBox has them: true, false, yes, sleep,
 * seq, rev, tee. */

int command_true(int argc, char *argv[]) {
        (void)argc;
        (void)argv;
        return EXIT_SUCCESS;
}

const char *help_true() { return _("Do nothing, successfully"); }

int command_false(int argc, char *argv[]) {
        (void)argc;
        (void)argv;
        return EXIT_FAILURE;
}

const char *help_false() { return _("Do nothing, unsuccessfully"); }

/* yes [text...] - the text (default y) a line, until writing fails. Where a
 * pipe is a file (DOS, GonWare) that is when the disk is full: there use it
 * only towards a program reading the console. */
int command_yes(int argc, char *argv[]) {
        char line[512];
        int i;

        line[0] = 0;
        for (i = 1; i < argc; i++) {
                if (strlen(line) + strlen(argv[i]) + 2 > sizeof(line)) {
                        break;
                }
                if (i > 1) {
                        strcat(line, " ");
                }
                strcat(line, argv[i]);
        }
        if (argc < 2) {
                strcpy(line, "y");
        }
        strcat(line, "\n");
        while (fputs(line, stdout) != EOF && !ferror(stdout)) {
        }
        return EXIT_FAILURE;
}

const char *help_yes() { return _("Print a text (default y) again and again"); }

/* sleep N[s|m|h]... - wait for the sum, a number may have a fraction */
int command_sleep(int argc, char *argv[]) {
        double total = 0;
        struct timespec ts;
        int i;

        if (argc < 2) {
                fputs(_("   sleep NUMBER[s|m|h]...\n"), stderr);
                return EXIT_FAILURE;
        }
        for (i = 1; i < argc; i++) {
                char *end;
                double v = strtod(argv[i], &end);
                if (end == argv[i] || v < 0) {
                        fprintf(stderr, _("sleep: invalid time %s\n"), argv[i]);
                        return EXIT_FAILURE;
                }
                if (*end == 'm') {
                        v *= 60;
                } else if (*end == 'h') {
                        v *= 3600;
                } else if (*end && *end != 's') {
                        fprintf(stderr, _("sleep: invalid time %s\n"), argv[i]);
                        return EXIT_FAILURE;
                }
                total += v;
        }
        ts.tv_sec = (time_t)total;
        ts.tv_nsec = (long)((total - (double)ts.tv_sec) * 1e9);
        while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
        }
        return EXIT_SUCCESS;
}

const char *help_sleep() { return _("Wait a number of seconds (or m, h)"); }

/* seq [-s SEP] [FIRST [STEP]] LAST - whole numbers */
int command_seq(int argc, char *argv[]) {
        const char *sep = "\n";
        long v[3], first = 1, step = 1, last, n;
        int i, nv = 0;

        for (i = 1; i < argc; i++) {
                char *end;
                if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
                        sep = argv[++i];
                        continue;
                }
                if (nv == 3) {
                        nv = 4;
                        break;
                }
                v[nv] = strtol(argv[i], &end, 10);
                if (end == argv[i] || *end) {
                        fprintf(stderr, _("seq: not a whole number: %s\n"), argv[i]);
                        return EXIT_FAILURE;
                }
                nv++;
        }
        if (nv < 1 || nv > 3) {
                fputs(_("   seq [-s SEPARATOR] [FIRST [STEP]] LAST\n"), stderr);
                return EXIT_FAILURE;
        }
        last = v[nv - 1];
        if (nv > 1) {
                first = v[0];
        }
        if (nv == 3) {
                step = v[1];
        }
        if (step == 0) {
                fputs(_("seq: the step must not be 0\n"), stderr);
                return EXIT_FAILURE;
        }
        for (n = first; step > 0 ? n <= last : n >= last; n += step) {
                printf("%s%ld", n == first ? "" : sep, n);
        }
        if (step > 0 ? first <= last : first >= last) {
                putchar('\n');
        }
        return EXIT_SUCCESS;
}

const char *help_seq() { return _("Print a sequence of numbers"); }

/* reverse one line in place, keeping UTF-8 characters whole */
static void rev_line(char *s, size_t n) {
        size_t i, j;
        char t;

        for (i = 0, j = n; i + 1 < j; i++, j--) {
                t = s[i];
                s[i] = s[j - 1];
                s[j - 1] = t;
        }
        /* each character's bytes are now backwards: a run of continuation
         * bytes followed by its lead byte; turn those round again */
        for (i = 0; i < n; i++) {
                if (((unsigned char)s[i] & 0xc0) == 0x80) {
                        size_t k = i;
                        while (k < n && ((unsigned char)s[k] & 0xc0) == 0x80) {
                                k++;
                        }
                        if (k < n) {
                                size_t a = i, b = k;
                                for (; a < b; a++, b--) {
                                        t = s[a];
                                        s[a] = s[b];
                                        s[b] = t;
                                }
                        }
                        i = k;
                }
        }
}

static void rev_stream(FILE *f) {
        char line[1024];

        while (fgets(line, sizeof(line), f) != NULL) {
                size_t n = strlen(line);
                int nl = n && line[n - 1] == '\n';
                if (nl) {
                        n--;
                }
                if (n && line[n - 1] == '\r') {
                        n--;
                }
                rev_line(line, n);
                fwrite(line, 1, n, stdout);
                if (nl) {
                        putchar('\n');
                }
        }
}

/* rev [file...] - every line backwards */
int command_rev(int argc, char *argv[]) {
        int i, rc = EXIT_SUCCESS;

        if (argc < 2) {
                rev_stream(stdin);
                return rc;
        }
        for (i = 1; i < argc; i++) {
                FILE *f = fopen(argv[i], "r");
                if (!f) {
                        fprintf(stderr, _("rev: cannot open %s\n"), argv[i]);
                        rc = EXIT_FAILURE;
                        continue;
                }
                rev_stream(f);
                fclose(f);
        }
        return rc;
}

const char *help_rev() { return _("Print every line backwards"); }

#define TEE_MAX 16

/* tee [-a] [file...] - the input to the output and into each file */
int command_tee(int argc, char *argv[]) {
        FILE *out[TEE_MAX];
        const char *mode = "w";
        char buf[1024];
        size_t n, nout = 0, k;
        int i, rc = EXIT_SUCCESS;

        for (i = 1; i < argc; i++) {
                if (strcmp(argv[i], "-a") == 0) {
                        mode = "a";
                }
        }
        for (i = 1; i < argc; i++) {
                if (strcmp(argv[i], "-a") == 0) {
                        continue;
                }
                if (nout == TEE_MAX) {
                        fprintf(stderr, _("tee: too many files, %s left out\n"), argv[i]);
                        rc = EXIT_FAILURE;
                        continue;
                }
                out[nout] = fopen(argv[i], mode);
                if (!out[nout]) {
                        fprintf(stderr, _("tee: cannot open %s\n"), argv[i]);
                        rc = EXIT_FAILURE;
                        continue;
                }
                nout++;
        }
        while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
                fwrite(buf, 1, n, stdout);
                for (k = 0; k < nout; k++) {
                        if (fwrite(buf, 1, n, out[k]) != n) {
                                rc = EXIT_FAILURE;
                        }
                }
        }
        for (k = 0; k < nout; k++) {
                if (fclose(out[k]) != 0) {
                        rc = EXIT_FAILURE;
                }
        }
        return rc;
}

const char *help_tee() { return _("Copy the input to the output and into files (-a appends)"); }
