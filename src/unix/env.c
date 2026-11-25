#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dos/command.h"
#include "fdbox.h"
#include "lib/applet.h"
#include "lib/args.h"
#include "lib/nls.h"
#include "unix/env.h"

#if defined(__GONWARE__)
#include <gonware/gdos.h>
#include <gonware/syscall.h>
#else
#include <sys/utsname.h>
#endif

/*
This file is part of fdbox
For license - read license.txt
*/

extern char **environ;
extern struct applet commands[];

#define ENV_MAX 256

/* env [-i] [-u NAME]... [NAME=VALUE]... [command [args...]] - without a
 * command the environment so changed, with one that command in it. The
 * applets run inside this shell, so the shell's own environment is put
 * back afterwards. */
int command_env(int argc, char *argv[]) {
        char *saved[ENV_MAX];
        size_t nsaved = 0, k;
        int i, rc = EXIT_SUCCESS;

        for (k = 0; environ && environ[k] && nsaved < ENV_MAX; k++) {
                saved[nsaved] = strdup(environ[k]);
                if (saved[nsaved]) {
                        nsaved++;
                }
        }
        for (i = 1; i < argc; i++) {
                char *eq;
                if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "-") == 0) {
                        while (environ && environ[0]) {
                                char name[256];
                                size_t l = strcspn(environ[0], "=");
                                if (l >= sizeof(name)) {
                                        l = sizeof(name) - 1;
                                }
                                memcpy(name, environ[0], l);
                                name[l] = 0;
                                if (unsetenv(name) != 0) {
                                        break;
                                }
                        }
                } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
                        unsetenv(argv[++i]);
                } else if ((eq = strchr(argv[i], '=')) != NULL && eq != argv[i]) {
                        *eq = 0;
                        setenv(argv[i], eq + 1, 1);
                        *eq = '=';
                } else {
                        break;
                }
        }
        if (i < argc) {
                char line[1024];
                if (command_merge_args((size_t)(argc - i), (const char **)argv + i, line,
                                       sizeof(line))) {
                        rc = command_execute_line(line);
                } else {
                        fputs(_("env: the command is too long\n"), stderr);
                        rc = EXIT_FAILURE;
                }
        } else {
                for (k = 0; environ && environ[k]; k++) {
                        puts(environ[k]);
                }
        }
        /* the shell's environment as it was */
        while (environ && environ[0]) {
                char name[256];
                size_t l = strcspn(environ[0], "=");
                if (l >= sizeof(name)) {
                        l = sizeof(name) - 1;
                }
                memcpy(name, environ[0], l);
                name[l] = 0;
                if (unsetenv(name) != 0) {
                        break;
                }
        }
        for (k = 0; k < nsaved; k++) {
                char *eq = strchr(saved[k], '=');
                if (eq) {
                        *eq = 0;
                        setenv(saved[k], eq + 1, 1);
                }
                free(saved[k]);
        }
        return rc;
}

const char *help_env() { return _("Show the environment, or run a command with changes to it"); }

#if !defined(__GONWARE__)
/* a name in PATH, as a POSIX shell finds it */
static int which_path(const char *name, char *path, size_t size) {
        const char *p = getenv("PATH");
        if (strchr(name, '/')) {
                snprintf(path, size, "%s", name);
                return access(path, X_OK) == 0;
        }
        while (p && *p) {
                size_t n = strcspn(p, ":");
                struct stat st;
                snprintf(path, size, "%.*s/%s", (int)(n ? n : 1), n ? p : ".", name);
                if (stat(path, &st) == 0 && !S_ISDIR(st.st_mode) && access(path, X_OK) == 0) {
                        return 1;
                }
                p += n + (p[n] == ':');
        }
        return 0;
}
#endif

/* which name... - what a command name runs: an applet of this shell, or
 * the file found as the shell finds it (PATH, and on GonWare PATHEXT) */
int command_which(int argc, char *argv[]) {
        int i, rc = EXIT_SUCCESS;

        if (argc < 2) {
                fputs(_("   which NAME...\n"), stderr);
                return EXIT_FAILURE;
        }
        for (i = 1; i < argc; i++) {
                char path[260];
                if (find_applet(CASE_INSENSITVE, argv[i], commands)) {
                        printf(_("%s: shell built-in command\n"), argv[i]);
                        continue;
                }
#if defined(__GONWARE__)
                if (command_which_file(argv[i], path, sizeof(path))) {
#else
                if (which_path(argv[i], path, sizeof(path))) {
#endif
                        puts(path);
                        continue;
                }
                fprintf(stderr, _("which: no %s in PATH\n"), argv[i]);
                rc = EXIT_FAILURE;
        }
        return rc;
}

const char *help_which() { return _("Show what a command name runs"); }

/* uname [-a -s -n -r -m] - the system's name, the computer's, the release
 * and the machine (default -s) */
int command_uname(int argc, char *argv[]) {
        const char *part[4];
        int want[4] = {0, 0, 0, 0}, i, any = 0, first = 1;
        static const char opts[] = "snrm";
#if defined(__GONWARE__)
        struct gdos_info info;
        char host[64];

        memset(&info, 0, sizeof(info));
        gdos_call1(GDOS_INFO, &info);
        if (gdos_call2(GDOS_HOSTNAME, host, sizeof(host)) < 0) {
                strcpy(host, "GONWARE");
        }
        part[0] = "GonWare";
        part[1] = host;
        part[2] = info.version;
        part[3] = info.arch;
#else
        struct utsname u;
        if (uname(&u) != 0) {
                fputs(_("uname: cannot get the system's name\n"), stderr);
                return EXIT_FAILURE;
        }
        part[0] = u.sysname;
        part[1] = u.nodename;
        part[2] = u.release;
        part[3] = u.machine;
#endif
        for (i = 1; i < argc; i++) {
                const char *p = argv[i];
                if (*p != '-' || !p[1]) {
                        goto usage;
                }
                for (p++; *p; p++) {
                        const char *o = strchr(opts, *p);
                        if (*p == 'a') {
                                want[0] = want[1] = want[2] = want[3] = 1;
                        } else if (o && *p) {
                                want[o - opts] = 1;
                        } else {
                                goto usage;
                        }
                        any = 1;
                }
        }
        if (!any) {
                want[0] = 1;
        }
        for (i = 0; i < 4; i++) {
                if (want[i]) {
                        printf("%s%s", first ? "" : " ", part[i]);
                        first = 0;
                }
        }
        putchar('\n');
        return EXIT_SUCCESS;
usage:
        fputs(_("   uname [-a] [-s] [-n] [-r] [-m]\n"), stderr);
        return EXIT_FAILURE;
}

const char *help_uname() { return _("Show the system's name, release and machine"); }
