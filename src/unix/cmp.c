#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fdbox.h"
#include "lib/nls.h"
#include "unix/cmp.h"

/*
This file is part of fdbox
For license - read license.txt
*/

/* cmp [-s] [-l] file1 [file2] - compare two files byte by byte ("-" or a
 * missing file2: the input). 0 the same, 1 they differ, 2 trouble. -s says
 * nothing, -l lists every differing byte (position, octal values). */
int command_cmp(int argc, char *argv[]) {
        const char *name[2] = {NULL, NULL};
        FILE *f[2];
        int silent = 0, list = 0, n = 0, i, rc = 0;
        long byte = 1, line = 1;

        for (i = 1; i < argc; i++) {
                if (strcmp(argv[i], "-s") == 0) {
                        silent = 1;
                } else if (strcmp(argv[i], "-l") == 0) {
                        list = 1;
                } else if (n < 2) {
                        name[n++] = argv[i];
                } else {
                        n = 3;
                }
        }
        if (n < 1 || n > 2) {
                fputs(_("   cmp [-s] [-l] FILE1 [FILE2]\n"), stderr);
                return 2;
        }
        if (n == 1) {
                name[1] = "-";
        }
        for (i = 0; i < 2; i++) {
                f[i] = strcmp(name[i], "-") == 0 ? stdin : fopen(name[i], "rb");
                if (!f[i]) {
                        if (!silent) {
                                fprintf(stderr, _("cmp: cannot open %s\n"), name[i]);
                        }
                        if (i && f[0] != stdin) {
                                fclose(f[0]);
                        }
                        return 2;
                }
        }
        for (;;) {
                int a = getc(f[0]), b = getc(f[1]);
                if (a == EOF || b == EOF) {
                        if (a != b) {
                                if (!silent) {
                                        fprintf(stderr, _("cmp: EOF on %s after byte %ld\n"),
                                                name[a == EOF ? 0 : 1], byte - 1);
                                }
                                rc = 1;
                        }
                        break;
                }
                if (a != b) {
                        rc = 1;
                        if (silent) {
                                break;
                        }
                        if (!list) {
                                printf(_("%s %s differ: byte %ld, line %ld\n"), name[0], name[1],
                                       byte, line);
                                break;
                        }
                        printf("%ld %3o %3o\n", byte, a, b);
                }
                if (a == '\n') {
                        line++;
                }
                byte++;
        }
        for (i = 0; i < 2; i++) {
                if (f[i] != stdin) {
                        fclose(f[i]);
                }
        }
        return rc;
}

const char *help_cmp() { return _("Compare two files byte by byte"); }
