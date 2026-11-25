#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "dos/md.h"
#include "fdbox.h"
#include "lib/args.h"
#include "lib/nls.h"
#include "lib/strextra.h"

/*
This file is part of fdbox
For license - read license.txt
*/

#ifdef __MSDOS__
#include "lib/tc202/dos-glob.h"
#include "lib/tc202/stdbool.h"
#include "lib/tc202/stdextra.h"
#endif

#ifdef _POSIX_C_SOURCE
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

struct mkdir_config {
        bool recursive;
        struct command_config global;
};

static void mkdir_config_init(struct mkdir_config *config);
static bool mkdir_config_parse(int argc, char *argv[], struct mkdir_config *config);
static void mkdir_config_print(const struct mkdir_config *config);
static void mkdir_print_extended_help();
static bool mkdir_create_dir(char *dir_name, const struct mkdir_config *config);

int command_md(int argc, char *argv[]) {
        size_t i;
        struct mkdir_config config;
        char unix_flags[4];

        mkdir_config_init(&config);
        /* mkdir -p: the parents too, as /r */
        command_unix_flags(&argc, argv, "p", unix_flags, sizeof(unix_flags));
        if (!mkdir_config_parse(argc, argv, &config)) {
                fputs(_("Failed parsing command line args\n"), stdout);
                return EXIT_FAILURE;
        }
        if (strchr(unix_flags, 'p')) {
                config.recursive = true;
        }
        /* mkdir_config_print(&config); */

        if (config.global.show_help) {
                mkdir_print_extended_help();
                return EXIT_SUCCESS;
        }
        for (i = 0; i < config.global.files.count; i++) {
                /* This is used only to move forward in the pointer, we will not modify the
                 * content of `dir_name`
                 */
                char *name = (char *)config.global.files.file[i];
                bool status = mkdir_create_dir(name, &config);
                if (!status) {
                        return EXIT_FAILURE;
                }
        }
        return EXIT_SUCCESS;
}

const char *help_md() { return _("Create one or more directories"); }

static void mkdir_config_init(struct mkdir_config *config) {
        config->recursive = false;
        command_config_init(&config->global);
}

static bool mkdir_config_parse(int argc, char *argv[], struct mkdir_config *config) {
        int c1, c2;
        do {
                c1 = command_config_parse(argc, argv, &config->global);
                c2 = tolower(c1);
                switch (c2) {
                case 'r':
                        config->recursive = true;
                        break;
                case ARG_PROCESSED:
                        break;
                case ARG_DONE:
                        break;
                default:
                        return false;
                }
        } while (c1 >= 0);

        return true;
}

static void mkdir_config_print(const struct mkdir_config *config) {
        printf("recursive: %s\n", str_bool(config->recursive));
        command_config_print(&config->global);
}

static void mkdir_print_extended_help() {
        printf("%s\n", help_md());

        fputs(_("   md [files] /r\n"), stdout);
        fputs(_("   /r Recursive creation (otherwise, creates only top level)\n"), stdout);
}

static int mkdir_one(const char *path) {
#if defined(__DJGPP__) || defined(_POSIX_C_SOURCE)
        return mkdir(path, 0755);
#elif defined(__TURBOC__) || defined(__WIN32__)
        return mkdir(path);
#endif
}

/* dir_name; with /r (mkdir -p) each missing directory above it first, and
 * one that is there already is no error */
static bool mkdir_create_dir(char *dir_name, const struct mkdir_config *config) {
        struct stat st;
        int r;

        if (config->recursive) {
                char *p;
                for (p = dir_name + 1; *p; p++) {
                        char c = *p;
                        /* a separator, not the one of a root ("\", "C:\") */
                        if ((c != '/' && c != '\\') || p[-1] == ':' || p[-1] == '/' ||
                            p[-1] == '\\') {
                                continue;
                        }
                        *p = 0;
                        if (stat(dir_name, &st) != 0) {
                                mkdir_one(dir_name);
                        }
                        *p = c;
                }
                r = stat(dir_name, &st) == 0 && S_ISDIR(st.st_mode) ? 0 : mkdir_one(dir_name);
        } else {
                r = mkdir_one(dir_name);
        }

        if (r != 0) {
                printf(_("Failed creating %s\n"), dir_name);
        } else if (config->global.verbose) {
                printf(_("Created %s\n"), dir_name);
        }
        return r == 0;
}
