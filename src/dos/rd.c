#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dos/rd.h"
#include "fdbox.h"
#include "lib/args.h"
#include "lib/nls.h"

/*
This file is part of fdbox
For license - read license.txt
*/

#ifdef __MSDOS__
#include "lib/tc202/stdbool.h"
#include <dir.h>
#endif

#if defined(_POSIX_C_SOURCE) || defined(__WIN32__)
#include <stdbool.h>
#include <unistd.h>
#endif

/* RD, RMDIR: removes empty directories (no /S: deleting trees is DEL's
   and DELTREE's business) */

static void rd_print_extended_help() {
        printf("%s\n", help_rd());
        fputs(_("   rd [directories]\n"), stdout);
        fputs(_("   The directories must be empty.\n"), stdout);
}

int command_rd(int argc, char *argv[]) {
        struct command_config config;
        size_t i;
        int c, status = EXIT_SUCCESS;

        command_config_init(&config);
        do {
                c = command_config_parse(argc, argv, &config);
                if (c != ARG_PROCESSED && c != ARG_DONE && c >= 0) {
                        fputs(_("Failed parsing command line args\n"), stdout);
                        return EXIT_FAILURE;
                }
        } while (c >= 0);
        if (config.show_help) {
                rd_print_extended_help();
                return EXIT_SUCCESS;
        }
        if (config.files.count == 0) {
                fputs(_("Required parameter missing\n"), stdout);
                return EXIT_FAILURE;
        }
        for (i = 0; i < config.files.count; i++) {
                const char *name = config.files.file[i];
                if (rmdir(name) != 0) {
                        printf(_("%s cannot be removed (not empty, in use or not found)\n"), name);
                        status = EXIT_FAILURE;
                } else if (config.verbose) {
                        printf(_("Removed %s\n"), name);
                }
        }
        return status;
}

const char *help_rd() { return _("Remove one or more empty directories"); }
