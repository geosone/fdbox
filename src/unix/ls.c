#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "fdbox.h"
#include "lib/nls.h"
#include "unix/ls.h"

#if defined(__GONWARE__)
#include <strings.h>
#define name_cmp strcasecmp
#else
#define name_cmp strcmp
#endif

/*
This file is part of fdbox
For license - read license.txt
*/

/* ls [-l -a -1] [file or directory...] - names of a directory, sorted, in
 * columns on a terminal and one a line otherwise; -l long form (type and
 * the nine permission bits as the system reports them, size, time), -a
 * names starting with a dot too. The same directory reading as DIR, but
 * the output of ls. */

#define LS_MAX 1024
#define LS_WIDTH 80

struct ls_opts {
        int lng, all, one;
};

static int ls_cmp(const void *a, const void *b) {
        return name_cmp(*(char *const *)a, *(char *const *)b);
}

static void ls_long(const char *path, const char *name) {
        struct stat st;
        char mode[11], when[32];
        struct tm *tm;

        if (stat(path, &st) != 0) {
                printf("?????????? %10s %12s %s\n", "?", "?", name);
                return;
        }
        mode[0] = S_ISDIR(st.st_mode) ? 'd' : '-';
        mode[1] = st.st_mode & S_IRUSR ? 'r' : '-';
        mode[2] = st.st_mode & S_IWUSR ? 'w' : '-';
        mode[3] = st.st_mode & S_IXUSR ? 'x' : '-';
        mode[4] = st.st_mode & S_IRGRP ? 'r' : '-';
        mode[5] = st.st_mode & S_IWGRP ? 'w' : '-';
        mode[6] = st.st_mode & S_IXGRP ? 'x' : '-';
        mode[7] = st.st_mode & S_IROTH ? 'r' : '-';
        mode[8] = st.st_mode & S_IWOTH ? 'w' : '-';
        mode[9] = st.st_mode & S_IXOTH ? 'x' : '-';
        mode[10] = 0;
        tm = localtime(&st.st_mtime);
        if (!tm || !strftime(when, sizeof(when), "%Y-%m-%d %H:%M", tm)) {
                strcpy(when, "?");
        }
        printf("%s %10lld %s %s\n", mode, (long long)st.st_size, when, name);
}

static void ls_names(const struct ls_opts *o, const char *dir, char **names, size_t n) {
        size_t i, width = 0, cols, rows, r, c;

        qsort(names, n, sizeof(names[0]), ls_cmp);
        if (o->lng) {
                for (i = 0; i < n; i++) {
                        char path[512];
                        if (dir) {
                                size_t l = strlen(dir);
                                snprintf(path, sizeof(path), "%s%s%s", dir,
                                         l && strchr("\\/:", dir[l - 1]) ? "" : DIRECTORY_DELIMITER,
                                         names[i]);
                        } else {
                                snprintf(path, sizeof(path), "%s", names[i]);
                        }
                        ls_long(path, names[i]);
                }
                return;
        }
        if (o->one || !isatty(1)) {
                for (i = 0; i < n; i++) {
                        puts(names[i]);
                }
                return;
        }
        for (i = 0; i < n; i++) {
                if (strlen(names[i]) > width) {
                        width = strlen(names[i]);
                }
        }
        width += 2;
        cols = LS_WIDTH / width ? LS_WIDTH / width : 1;
        rows = (n + cols - 1) / cols;
        for (r = 0; r < rows; r++) {
                for (c = 0; c < cols; c++) {
                        i = c * rows + r;
                        if (i >= n) {
                                break;
                        }
                        if (c + 1 < cols && i + rows < n) {
                                printf("%-*s", (int)width, names[i]);
                        } else {
                                fputs(names[i], stdout);
                        }
                }
                putchar('\n');
        }
}

static int ls_dir(const struct ls_opts *o, const char *dir) {
        DIR *d = opendir(dir);
        struct dirent *e;
        char *names[LS_MAX];
        size_t n = 0, skipped = 0, i;

        if (!d) {
                fprintf(stderr, _("ls: cannot open %s\n"), dir);
                return EXIT_FAILURE;
        }
        while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.' && !o->all) {
                        continue;
                }
                if (n == LS_MAX) {
                        skipped++;
                        continue;
                }
                names[n] = strdup(e->d_name);
                if (names[n]) {
                        n++;
                }
        }
        closedir(d);
        ls_names(o, dir, names, n);
        for (i = 0; i < n; i++) {
                free(names[i]);
        }
        if (skipped) {
                fprintf(stderr, _("ls: %lu more entries not shown\n"), (unsigned long)skipped);
        }
        return EXIT_SUCCESS;
}

int command_ls(int argc, char *argv[]) {
        struct ls_opts o = {0, 0, 0};
        char *files[LS_MAX], *dirs[LS_MAX];
        size_t nfiles = 0, ndirs = 0, i;
        int rc = EXIT_SUCCESS;

        for (i = 1; i < (size_t)argc; i++) {
                char *arg = argv[i], *p;
                struct stat st;
                if (arg[0] == '-' && arg[1]) {
                        for (p = arg + 1; *p; p++) {
                                if (*p == 'l') {
                                        o.lng = 1;
                                } else if (*p == 'a') {
                                        o.all = 1;
                                } else if (*p == '1') {
                                        o.one = 1;
                                } else {
                                        fprintf(stderr, _("ls: unknown option -%c\n"), *p);
                                        fputs(_("   ls [-l] [-a] [-1] [file or directory...]\n"),
                                              stderr);
                                        return EXIT_FAILURE;
                                }
                        }
                        continue;
                }
                if (stat(arg, &st) != 0) {
                        fprintf(stderr, _("ls: %s: not found\n"), arg);
                        rc = EXIT_FAILURE;
                } else if (S_ISDIR(st.st_mode)) {
                        if (ndirs < LS_MAX) {
                                dirs[ndirs++] = arg;
                        }
                } else if (nfiles < LS_MAX) {
                        files[nfiles++] = arg;
                }
        }
        if (!nfiles && !ndirs && rc == EXIT_SUCCESS) {
                return ls_dir(&o, ".");
        }
        if (nfiles) {
                ls_names(&o, NULL, files, nfiles);
        }
        qsort(dirs, ndirs, sizeof(dirs[0]), ls_cmp);
        for (i = 0; i < ndirs; i++) {
                if (nfiles || ndirs > 1) {
                        printf("%s%s:\n", nfiles || i ? "\n" : "", dirs[i]);
                }
                if (ls_dir(&o, dirs[i]) != EXIT_SUCCESS) {
                        rc = EXIT_FAILURE;
                }
        }
        return rc;
}

const char *help_ls() { return _("List directory contents (-l long, -a all, -1 one a line)"); }
