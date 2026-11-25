#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "dos/copymove.h"
#include "fdbox.h"
#include "lib/args.h"
#include "lib/nls.h"
#include "lib/strextra.h"

/*
This file is part of fdbox
For license - read license.txt
*/

#if defined(__TURBOC__)
#include "lib/tc202/dos-glob.h"
#include "lib/tc202/stdbool.h"
#include "lib/tc202/stdextra.h"
#endif

#if defined(__WATCOMC__)
#include "lib/tc202/dos-glob.h"
#endif

#if defined(_POSIX_C_SOURCE) || defined(__DJGPP__) || defined(__APPLE__)
#include <glob.h>
#include <stdbool.h>
#include <unistd.h>
#include <utime.h>
#endif

#ifdef __WIN32__
#include "lib/win32/dirent.h"
#include "lib/win32/win32-glob.h"
#include <stdbool.h>
#include <unistd.h>
#include <utime.h>
#endif

struct copy_config {
        /*        bool verify; */
        bool ask_overwrite;
        bool copy_attributes;
        bool move_files;
        const char *source_file;
        const char *dest_file;
        struct command_config global;
};

static void copy_move_config_init(struct copy_config *config);
static bool copy_move_parse_config(int argc, char *argv[], struct copy_config *config);
static void copy_print_extended_help();
static void move_print_extended_help();
static bool copy_move_is_dir(const char *path);
static char *copy_move_append_path(const char *directory, const char *file_name);
static int copy_single_file(const char *from, const char *to, struct copy_config *config);
static int move_single_file(const char *from, const char *to, struct copy_config *config);

/* the files a name or pattern stands for; 0 when none */
static size_t copy_move_glob(const char *pattern, glob_t *g) {
        memset(g, 0, sizeof(*g));
        if (glob(pattern, GLOB_DOOFFS, NULL, g) != 0) {
                return 0;
        }
        return g->gl_pathc;
}

/* COPY A+B[+...] [TO]: the files one after another into TO; without TO
 * (or with TO the first of them) the others appended to the first */
static int copy_concat(int nsrc, char *src[], const char *to) {
        char parts[1024], *part, *next;
        FILE *out;
        size_t i, n, k = 0;
        int rc = EXIT_SUCCESS;
        char buffer[4096];
        bool append;

        parts[0] = 0;
        for (i = 0; i < (size_t)nsrc; i++) {
                if (strlen(parts) + strlen(src[i]) + 2 > sizeof(parts)) {
                        fputs(_("Error: the command is too long\n"), stderr);
                        return EXIT_FAILURE;
                }
                strcat(parts, src[i]);
                strcat(parts, "+");
        }
        part = parts;
        while (*part == '+') {
                part++;
        }
        next = strchr(part, '+');
        *next = 0;
        if (*part == 0) {
                fputs(_("Error: no source file provided\n"), stderr);
                return EXIT_FAILURE;
        }
        if (to == NULL) {
                to = part;
        }
        append = strcmp(to, part) == 0;
        out = fopen(to, append ? "ab" : "wb");
        if (out == NULL) {
                fprintf(stderr, _("Cannot open %s\n"), to);
                return EXIT_FAILURE;
        }
        for (;;) {
                glob_t g;
                if (k++ == 0 && append) {
                        goto next_part; /* the file appended to */
                }
                if (copy_move_glob(part, &g) == 0) {
                        fprintf(stderr, _("File not found - %s\n"), part);
                        rc = EXIT_FAILURE;
                }
                for (i = 0; i < g.gl_pathc; i++) {
                        FILE *in = fopen(g.gl_pathv[i], "rb");
                        if (in == NULL) {
                                fprintf(stderr, _("Cannot open %s\n"), g.gl_pathv[i]);
                                rc = EXIT_FAILURE;
                                continue;
                        }
                        while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
                                if (fwrite(buffer, 1, n, out) != n) {
                                        rc = EXIT_FAILURE;
                                        break;
                                }
                        }
                        fclose(in);
                }
                globfree(&g);
        next_part:
                /* the next part; empty ones ("A + B" gives "A++B") skipped */
                part = next + 1;
                while (*part == '+') {
                        part++;
                }
                if (*part == 0) {
                        break;
                }
                next = strchr(part, '+');
                *next = 0;
        }
        if (fclose(out) != 0) {
                rc = EXIT_FAILURE;
        }
        return rc;
}

/* COPY/MOVE source... [to]: each source a name or pattern; several files go
 * into a directory; without "to" into the current directory */
static int command_move_copy(int argc, char *argv[], struct copy_config *config) {
        int rc = EXIT_SUCCESS;
        size_t nsrc, i, j, total = 0;
        const char *dest;
        char **files;
        bool concat = false;

        copy_move_config_init(config);
        copy_move_parse_config(argc, argv, config);
        /* copy_move_print_config(config); */

        if (config->global.show_help) {
                if (config->move_files) {
                        move_print_extended_help();
                } else {
                        copy_print_extended_help();
                }
                return EXIT_SUCCESS;
        }
        if (config->global.files.count == 0) {
                fputs(_("Error: no source file provided\n"), stderr);
                return EXIT_FAILURE;
        }
        files = (char **)config->global.files.file;
        nsrc = config->global.files.count == 1 ? 1 : config->global.files.count - 1;
        dest = config->global.files.count == 1 ? NULL : files[nsrc];
        for (i = 0; i < nsrc; i++) {
                concat |= strchr(files[i], '+') != NULL;
        }
        if (dest != NULL && strcmp(dest, "+") != 0 && strchr(dest, '+') != NULL) {
                concat = true; /* "COPY A +B": all sources */
                nsrc++;
                dest = NULL;
        }
        if (concat && !config->move_files) {
                return copy_concat((int)nsrc, files, dest);
        }
        if (dest == NULL) {
                dest = ".";
        }
        for (i = 0; i < nsrc; i++) {
                glob_t g;
                total += copy_move_glob(files[i], &g);
                globfree(&g);
        }
        if (total > 1 && !copy_move_is_dir(dest)) {
                fprintf(stderr, _("Error: %s is not a directory, several files cannot go there\n"),
                        dest);
                return EXIT_FAILURE;
        }
        for (i = 0; i < nsrc; i++) {
                glob_t g;
                if (copy_move_glob(files[i], &g) == 0) {
                        fprintf(stderr, _("File not found - %s\n"), files[i]);
                        rc = EXIT_FAILURE;
                }
                for (j = 0; j < g.gl_pathc; j++) {
                        const char *from_file = g.gl_pathv[j];
                        const char *target_name = dest;
                        bool free_memory = false;
                        int r;

                        if (copy_move_is_dir(dest)) {
                                target_name =
                                    copy_move_append_path(dest, file_base_name(from_file));
                                free_memory = true;
                        }
                        if (config->move_files) {
                                r = move_single_file(from_file, target_name, config);
                        } else {
                                r = copy_single_file(from_file, target_name, config);
                        }
                        if (free_memory) {
                                free((char *)target_name);
                        }
                        if (r != EXIT_SUCCESS) {
                                rc = EXIT_FAILURE;
                        }
                }
                globfree(&g);
        }
        return rc;
}

int command_copy(int argc, char *argv[]) {
        struct copy_config config;
        config.move_files = false;
        return command_move_copy(argc, argv, &config);
}

int command_move(int argc, char *argv[]) {
        struct copy_config config;
        config.move_files = true;
        return command_move_copy(argc, argv, &config);
}

const char *help_copy() { return _("Copies one or more files to another location"); }

const char *help_move() { return _("Moves one or more files to another location"); }

static void copy_move_config_init(struct copy_config *config) {
        /*config->verify = false;*/
        config->ask_overwrite = false;
        config->copy_attributes = false;
        config->source_file = NULL;
        config->dest_file = NULL;
        command_config_init(&config->global);
        /*
         *      This should be defined by the upper calling function. This is not
         *      data that arrives from the user
         *      config->move_files = false;
         */
}

/*
 * MSDOS + freecom -
 * This is the "copy" command, but we use it also for move/rename
 *
 * /a ascii
 * /b binary
 * /v verify
 * /y - no confirmation, always overrites
 * /-y prompts to confirm every overwrite
 *
 * /a - We do not support ascii (we always copy in binary mode)
 * /v - we do not verify - let the OS do this for us.
 * /-y WIP - parser is not supporting this yet.
 */

static bool copy_move_parse_config(int argc, char *argv[], struct copy_config *config) {
        int c1, c2;
        do {
                c1 = command_config_parse(argc, argv, &config->global);
                c2 = tolower(c1);
                switch (c2) {
                case 'a':
                        if (config->move_files)
                                return false;
                        config->copy_attributes = true;
                        break;
                case 'y':
                        config->ask_overwrite = true;
                        break;
                case ARG_PROCESSED:
                        break;
                case ARG_DONE:
                        break;
                default:
                        return false;
                }
        } while (c1 >= 0);

        config->source_file = config->global.files.file[0];
        config->dest_file = config->global.files.file[1];

        return true;
}

static void copy_move_print_config(struct copy_config *config) {
        printf("ask_overwrite=%d\n", config->ask_overwrite);
        printf("copy_attributes=%d\n", config->copy_attributes);
        printf("source_file=%s\n", config->source_file);
        printf("dest_file=%s\n", config->dest_file);
        command_config_print(&config->global);
}

static void copy_print_extended_help() {
        printf("%s\n", help_copy());
        fputs(_("   copy [from...] [to] /a /y /v\n"), stdout);
        fputs(_("   copy a+b[+...] [to]  joins files (without to: onto a)\n"), stdout);

        fputs(_("   /a Copy attributes\n"), stdout);
        fputs(_("   /y Ask for confirmation on overwrite (WIP)\n"), stdout);
        fputs(_("   /v Verbose copy\n"), stdout);
        fputs(_("Examples:\n"), stdout);
        fputs(_("> copy c:\\autoexec.bat c:\\autoexec.old \n"), stdout);
        fputs(_("> copy *.exe backup\\ \n"), stdout);
}

static void move_print_extended_help() {
        printf("%s\n", help_move());
        fputs(_("   move [from] [to] /a /y /v\n"), stdout);

        fputs(_("   /y Ask for confirmation on overwrite (WIP)\n"), stdout);
        fputs(_("   /v Verbose copy\n"), stdout);
        fputs(_("Examples:\n"), stdout);
        fputs(_("> move c:\\autoexec.bat c:\\autoexec.old \n"), stdout);
        fputs(_("> move *.exe backup\\ \n"), stdout);
}

static bool copy_move_is_dir(const char *path) {
        struct stat path_stat;
        /* a path that is not there is no directory: path_stat is not filled then */
        return stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode);
}

static char *copy_move_append_path(const char *directory, const char *file_name) {
        int l1 = strlen(directory);
        int l2 = strlen(file_name);
        char *full_file_name;

        /* does the directory end with / or  \\ ? */
        full_file_name = malloc(l1 + l2 + 2); /* a delimiter, the NUL */
        strcpy(full_file_name, directory);
        if (directory[l1 - 1] == '/' || directory[l1 - 1] == '\\') {
                strcat(full_file_name, file_name);
        } else {
                /* it does not, we need to add it manually */
                strcat(full_file_name, DIRECTORY_DELIMITER);
                strcat(full_file_name, file_name);
        }

        return full_file_name;
}

static int copy_single_file(const char *from, const char *to, struct copy_config *config) {
        FILE *source;
        FILE *dest;
        char buffer[4096];
        size_t total_read, total_written, total_size;
        int last_displayed = 101;

        source = fopen(from, "rb");
        if (source == NULL) {
                fprintf(stderr, _("Failed reading from %s\n"), from);
                return errno;
        }
        dest = fopen(to, "wb");
        if (dest == NULL) {
                fprintf(stderr, _("Failed opening %s for writing\n"), to);
                fclose(source);
                return errno;
        }

        fseek(source, 0L, SEEK_END);
        total_size = ftell(source);
        fseek(source, 0L, SEEK_SET);
        /* TODO should I use:
         *         rewind(source);
         */

        total_read = 0;
        total_written = 0;
        do {
                size_t actually_read, actually_written;
                int current;

                actually_read = fread(buffer, 1, sizeof(buffer), source);
                total_read += actually_read;
                if (actually_read == 0) {
                        break;
                }
                actually_written = fwrite(buffer, 1, actually_read, dest);
                total_written += actually_written;
                if (actually_written == 0) {
                        break;
                }

                current = ((total_written * 100) / total_size);
                if (config->global.verbose && current != last_displayed) {
                        printf("\r%02d%% - %s\r", current, to);
                        fflush(stdout);
                        last_displayed = current;
                }
        } while (true);

        fclose(source);
        fclose(dest);
        if (total_read != total_written) {
                fprintf(stderr, _("Failed writing to %s (wrote %lld/%lld bytes)\n"), to,
                        (long long)total_written, (long long)total_read);
                return errno;
        }

        if (config->copy_attributes) {
#if defined(__MSDOS__) || defined(__WATCOMC__)
                fputs(_("Warning: copying attributes on MSDOS is not supported yet\n"), stderr);
#else
                struct stat source_attr;
                struct utimbuf new_times;
                int err;

                if (stat(from, &source_attr) != 0) {
                        fprintf(stderr, _("Error: failed reading the times of %s\n"), from);
                        return errno;
                }
                new_times.actime = source_attr.st_atime;
                new_times.modtime = source_attr.st_mtime;
                err = utime(to, &new_times);
                if (err) {
                        fprintf(stderr, _("Error: failed setting time on %s\n"), to);
                        return errno;
                }
                err = chmod(to, source_attr.st_mode);
                if (err) {
                        fprintf(stderr, _("Error: failed setting ownership on %s\n"), to);
                        return errno;
                }
#endif
        }

        /* TODO - clear all line */
        if (config->global.verbose) {
                if (total_size != 0) {
                        int per = (int)((total_written * 100) / total_size);
                        printf("\r%02d%% - %s -> %s %s\n", per, from, to,
                               config->copy_attributes ? "*" : "");
                } else {
                        printf("\r%s -> %s %s\n", from, to, config->copy_attributes ? "*" : "");
                }
        }
        return EXIT_SUCCESS;
}

static int move_single_file(const char *from, const char *to, struct copy_config *config) {
        int r = rename(from, to);

        if (r == 0) {
                if (config->global.verbose) {
                        printf("\r %s -> %s\n", from, to);
                }
                return EXIT_SUCCESS;
        }

        /* OK, we cannot move - lets copy+delete */
        config->copy_attributes = true;
        r = copy_single_file(from, to, config);
        if (r != 0) {
                fprintf(stderr, _("Error: failed moving %s->%s\n"), from, to);
        }
        r = remove(from);
        if (r != 0) {
                fprintf(stderr, _("Error: failed deleting original file: %s\n"), from);
                return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}
