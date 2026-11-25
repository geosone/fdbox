/* This file is part of fdbox
 For license - read license.txt
*/

/*
 * TODO
 * - dir /s - display accumulative byte count
 * - dir /a - filter types
 * - dir /p - pause - WIP
 * - display fanicer byte sizes
 * - fix display of subdirectories without any glob (dir /w ../)
 * - on msdos "dir c:*" (when in d:) displays garbage ("dir c:\*" works).
 */

/*
Places I look for inspiration:
https://github.com/tronkko/dirent/tree/master/examples
https://github.com/tronkko/dirent/blob/master/include/dirent.h
https://gist.github.com/saghul/8013376
https://github.com/tproffen/DiffuseCode/blob/master/lib_f90/win32-glob.c
*/

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dos/cmd_dir.h"
#include "fdbox.h"
#include "lib/args.h"
#include "lib/nls.h"
#include "lib/strextra.h"
#include <sys/stat.h>

#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#if !defined(__GONWARE__)
#include <sys/statvfs.h>
#endif
#endif

#include "lib/readline.h"

#ifdef __WIN32__
#include "lib/win32/dirent.h"
#include "lib/win32/win32-glob.h"
#endif

#if defined(__TURBOC__)
#include "lib/tc202/dos-glob.h"
#include "lib/tc202/stdbool.h"
#include "lib/tc202/stdextra.h"
#endif

#if defined(__WATCOMC__)
#include "lib/tc202/dos-glob.h"
#include <strings.h>
#include <sys/stat.h>
#endif

/***************************************************************************
 * Types used internally
 ***************************************************************************/

/* /O: the keys in order, an upper case letter ascending (N name, E
 * extension, S size smallest first, D date oldest first, G directories
 * first), a lower case one reversed ("-" before it) */
#define SORT_KEYS 8

#define MAX_DIR_FILES 128
#define MAX_DIR_ENTRY_FILES 1024

struct dir_config {
        bool show_help;
        bool pause;
        bool wide;
        char sort_keys[SORT_KEYS];
        bool subdirs;
        bool bare;
        bool lower_case;
        struct command_config global;
};

struct file_entry {
        char *file_name;
        struct stat file_details;
};

/***************************************************************************
 * Forward declarations
 ***************************************************************************/
static void dir_config_init(struct dir_config *config);
static void dir_config_print(struct dir_config *config);
/* a directory that could not be read while globbing (an empty drive, a
 * read error): its name and errno, reported instead of an empty listing */
static int dir_glob_errno;
static char dir_glob_path[260];

#if defined(_WIN32)
#define DIR_GLOB_ERRFUNC NULL /* win32-glob.c takes no errfunc */
#else
static int dir_glob_error(const char *path, int err) {
        dir_glob_errno = err;
        snprintf(dir_glob_path, sizeof(dir_glob_path), "%s", path);
        return 1;
}
#define DIR_GLOB_ERRFUNC dir_glob_error
#endif

static void dir_display_dir(struct dir_config *config, const char *dir_name,
                            struct command_glob *files2, int depth);
static bool dir_parse_config(int argc, char *argv[], struct dir_config *config);
static void dir_format_date_time(long ff_date, long ff_time, char *time, char *date);
static void dir_print_extended_help();

static const char *dir_sort_keys;
static int dir_file_comperator(const void *a, const void *b);
static bool dir_parse_sort(const char *arg, char keys[SORT_KEYS]);

static bool found(const char *file_name, const struct file_entry files[], size_t file_count);

/***************************************************************************
 * This is public API
 ***************************************************************************/
int command_dir(int argc, char *argv[]) {
        struct dir_config config;

        /* first read configuration from command line */
        dir_config_init(&config);
        if (!dir_parse_config(argc, argv, &config)) {
                fputs(_("Failed parsing command line args\n"), stdout);
                return EXIT_FAILURE;
        }
        /*        dir_config_print(&config); */

        if (config.show_help) {
                dir_print_extended_help();
                return EXIT_SUCCESS;
        }

        if (config.global.files.overflow != 0) {
                fprintf(stderr, _("Warning: %zd/%zd files will not be displayed\n"),
                        config.global.files.overflow,
                        config.global.files.overflow + config.global.files.count);
        }

        if (config.global.files.count == 0) {
                config.global.files.file[0] = ALL_FILES_GLOB;
                config.global.files.count = 1;
        }

#if defined(__GONWARE__) || defined(_POSIX_C_SOURCE) || defined(__APPLE__)
        {
                /* as in DOS: "DIR D:" and "DIR NAME" of a directory list its files */
                static char patterns[ARG_MAX_FILES][260];
                static char where[260];
                const char *first;
                size_t i, l;

                for (i = 0; i < config.global.files.count; i++) {
                        const char *f = config.global.files.file[i];
                        struct stat st;
                        l = strlen(f);
                        if (l && strchr(":\\/", f[l - 1]))
                                snprintf(patterns[i], sizeof(patterns[i]), "%s" ALL_FILES_GLOB, f);
                        else if (!strpbrk(f, "*?") && !stat(f, &st) && S_ISDIR(st.st_mode))
                                snprintf(patterns[i], sizeof(patterns[i]),
                                         "%s" DIRECTORY_DELIMITER ALL_FILES_GLOB, f);
                        else
                                snprintf(patterns[i], sizeof(patterns[i]), "%s", f);
                        config.global.files.file[i] = patterns[i];
                }
                /* the directory shown: that of the first pattern */
                first = config.global.files.file[0];
                l = 0;
                for (i = 0; first[i]; i++)
                        if (strchr(":\\/", first[i]))
                                l = i + 1;
                if (!l) {
                        if (!getcwd(where, sizeof(where)))
                                strcpy(where, ".");
                } else if (l == 2 && first[1] == ':') {
                        /* "D:": the current directory of that drive */
                        char here[260], drive[3] = {first[0], ':', 0};
                        if (!getcwd(here, sizeof(here)) || chdir(drive) ||
                            !getcwd(where, sizeof(where)))
                                snprintf(where, sizeof(where), "%s", drive);
                        chdir(here);
                } else {
                        snprintf(where, sizeof(where), "%.*s", (int)l, first);
                        if (l > 1 && where[l - 2] != ':' && strchr("\\/", where[l - 1]))
                                where[l - 1] = 0;
                }
                dir_display_dir(&config, where, &config.global.files, 1);
        }
#else
        dir_display_dir(&config, ".", &config.global.files, 1);
#endif
        if (dir_glob_errno != 0) {
                size_t n = strlen(dir_glob_path);
                /* "A: No medium found", "A:\SUB: ..." */
                fprintf(stderr, "%s%s %s\n", dir_glob_path,
                        n && dir_glob_path[n - 1] == ':' ? "" : ":", strerror(dir_glob_errno));
                return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}

/* the rows of the terminal on stdout (25 when it is not one) */
static int dir_rows(void) {
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
        struct winsize w;
        if (ioctl(1, TIOCGWINSZ, &w) == 0 && w.ws_row > 2) {
                return w.ws_row;
        }
#endif
        return 25;
}

/* the bytes free on the volume of path, -1 when that is not known */
static long long dir_free(const char *path) {
#if (defined(_POSIX_C_SOURCE) || defined(__APPLE__)) && !defined(__GONWARE__)
        struct statvfs v;
        if (statvfs(path, &v) == 0) {
                return (long long)v.f_bavail * (long long)v.f_frsize;
        }
#else
        (void)path;
#endif
        return -1;
}

static void dir_display_dir(struct dir_config *config, const char *dir_name,
                            struct command_glob *files2, int depth) {
        /* then find all available files */
        struct file_entry files[MAX_DIR_ENTRY_FILES];
        size_t file_count = 0, requested_count = 0;
        size_t i, j;

        long long int total_bytes = 0;
        long long int free_bytes = dir_free(dir_name);
        int rows = dir_rows();
        int total_files = 0;
        int total_dirs = 0;
        int lines = 0;
        int cols = 0;

        /* I don't fully understand - but without initializing the array to 0,
         * valgrind is not happy. It complains about S_ISDIR() and st_mtime
         * Which is wierd - as those fields are used for display... and IMHO
         * the values I see are OK.
         */
        memset(files, 0, sizeof(files));
        for (i = 0; i < files2->count; i++) {
                glob_t globbuf = {0};
                dir_glob_errno = 0;
                if (glob(files2->file[i], GLOB_DOOFFS | GLOB_NOSORT, DIR_GLOB_ERRFUNC, &globbuf) ==
                        GLOB_ABORTED &&
                    dir_glob_errno != 0) {
                        globfree(&globbuf);
                        return;
                }
                for (j = 0; j != globbuf.gl_pathc; j++) {
                        const char *file_name = globbuf.gl_pathv[j];
                        requested_count++;
                        if (requested_count > MAX_DIR_ENTRY_FILES) {
                                continue;
                        }
                        if (!found(file_name, files, file_count)) {
                                files[file_count].file_name = strdup(globbuf.gl_pathv[j]);
                                if (stat(files[file_count].file_name,
                                         &files[file_count].file_details) != 0) {
                                        memset(&files[file_count].file_details, 0,
                                               sizeof(files[file_count].file_details));
                                }
                                file_count++;
                        }
                }
                globfree(&globbuf);
        }

        /* and sort if needed */
        if (config->sort_keys[0] != 0) {
                dir_sort_keys = config->sort_keys;
                qsort(files, file_count, sizeof(struct file_entry), dir_file_comperator);
        }

        if (requested_count != file_count) {
                fprintf(stderr, _("Warning: %d/%d files will not be shown\n"),
                        (int)requested_count - MAX_DIR_ENTRY_FILES, (int)requested_count);
        }
        if (!config->bare) {
                printf(_("\nDirectory of %s\n"), dir_name);
        }
        for (i = 0; i < file_count; i++) {
                char display[200];
                char *fname;

                total_bytes += files[i].file_details.st_size;
                fname = files[i].file_name;

#if defined(__GONWARE__)
                {
                        /* the name only, the directory is in the heading */
                        char *p;
                        for (p = fname; *p; p++)
                                if (*p == '\\' || *p == '/' || *p == ':')
                                        fname = p + 1;
                }
#else
                if (strcmp(dir_name, ".") != 0) {
                        if (str_is_prefix(fname, dir_name)) {
                                fname += strlen(dir_name) + 1;
                        }
                }
#endif
                if (S_ISDIR(files[i].file_details.st_mode)) {
                        total_dirs++;
                        if (config->wide) {
                                snprintf(display, 200, "[%s]", fname);
                        } else {
                                snprintf(display, 200, "%s%s", fname, DIRECTORY_DELIMITER);
                        }
                } else {
                        total_files++;
                        snprintf(display, 200, "%s", fname);
                }

                if (config->lower_case) {
                        str_to_lower(display);
                }

                if (config->wide) {
                        printf("%-18s ", display);
                        cols++;
                        if (cols == 4) {
                                cols = 0;
                                lines++;
                                putchar('\n');
                        }
                } else if (config->bare) {
                        puts(display);
                        lines++;
                } else {
                        char *ttime = ctime(&files[i].file_details.st_mtime);
                        if (S_ISDIR(files[i].file_details.st_mode)) {
                                printf("%-40s %10s %s", display, "<DIR>", ttime);
                        } else {
                                printf("%-40s %10lld %s", display,
                                       (long long)files[i].file_details.st_size, ttime);
                        }
                        lines++;
                }

                /* a screen full: a key goes on (only on a terminal) */
                if (config->pause && lines >= rows - 2 && isatty(1)) {
                        fputs(_("Press any key to continue . . ."), stdout);
                        fflush(stdout);
                        readline_key();
                        putchar('\n');
                        lines = 0;
                }

                if (config->subdirs && !S_ISDIR(files[i].file_details.st_mode)) {
                        /* file name is no longer needed */
                        free(files[i].file_name);
                }
        }
        if (cols != 0) {
                putchar('\n');
        }
        if (!config->bare) {
                printf(_("%10d files(s)\t %lld bytes\n"), total_files, total_bytes);
                if (free_bytes >= 0) {
                        printf(_("%10d dirs(s) \t %lld free\n"), total_dirs, free_bytes);
                } else {
                        printf(_("%10d dirs(s)\n"), total_dirs);
                }
        }
        for (i = 0; i < file_count; i++) {
                if (config->subdirs) {
                        struct command_glob f;
                        char *s;

                        if (!S_ISDIR(files[i].file_details.st_mode)) {
                                continue;
                        }
                        if (files[i].file_name == 0 || files[i].file_name[0] == '\0') {
                                continue;
                        }
                        s = (char *)malloc(strlen(files[i].file_name) + 5);
                        strcpy(s, files[i].file_name);
                        strcat(s, DIRECTORY_DELIMITER);
                        strcat(s, ALL_FILES_GLOB);
                        f.file[0] = s;
                        f.count = 1;
                        dir_display_dir(config, files[i].file_name, &f, depth + 1);
                        free(s);
                }
                free(files[i].file_name);
        }
}

const char *help_dir() { return _("Displays a list of files and subdirectories in a directory"); }

/****************************************************************************
 * internal functions
 ***************************************************************************/
static void dir_config_init(struct dir_config *config) {
        config->pause = false;
        config->wide = false;
        config->bare = false;
        config->sort_keys[0] = 0;
        config->lower_case = false;
        config->subdirs = false;
        config->show_help = false;
        command_config_init(&config->global);
}

static void dir_config_print(struct dir_config *config) {
        printf("\tpause=%s\n", str_bool(config->pause));
        printf("\twide_format=%s\n", str_bool(config->wide));
        printf("\tsubdirs=%s\n", str_bool(config->subdirs));
        printf("\tlower_case=%s\n", str_bool(config->lower_case));

        printf("\tsort=%s", config->sort_keys[0] ? config->sort_keys : "UNSORTED");
        putchar('\n');

        command_config_print(&config->global);
}

static bool dir_parse_config(int argc, char *argv[], struct dir_config *config) {

        int c1, c2;
        const char *argument;

        do {
                c1 = command_config_parse(argc, argv, &config->global);
                c2 = tolower(c1);
                switch (c2) {
                case 'p':
                        config->pause = true;
                        break;
                case 'w':
                        config->wide = true;
                        break;
                case 's':
                        config->subdirs = true;
                        break;
                case 'b':
                        config->bare = true;
                        break;
                case 'l':
                        config->lower_case = true;
                        break;

                case 'a':
                        /* TODO - attribute support is not supported yet */
                        argument = argv[config->global.state.current_argument - 1];
                        c2 = tolower(argument[2]);
                        switch (c2) {
                        case 'd':
                                break;
                        case 'h':
                                break;
                        case 's':
                                break;
                        case 'r':
                                break;
                        case 'a':
                                break;
                        default:
                                break;
                        }

                        break;
                case 'o':
                        argument = argv[config->global.state.current_argument - 1];
                        c2 = tolower(argument[2]);
                        if (!dir_parse_sort(argument + 2, config->sort_keys)) {
                                printf(_("invalid sort argument %s\n"), argument);
                                return false;
                        }
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

static void dir_print_extended_help() {
        printf("%s\n", help_dir());
        fputs(_("   dir [files] /p /w /s /b /l /O [files]\n"), stdout);

        fputs(_("   /p pause after each screen\n"), stdout);
        fputs(_("   /w use wide format, no details\n"), stdout);
        fputs(_("   /s display contents of sub directories as well\n"), stdout);
        fputs(_("   /b bare format - print no headers or summary\n"), stdout);
        fputs(_("   /l lower case - all names are displayed in lower case\n"), stdout);
        fputs(_("   /a[] filter files by attributes\n"), stdout);
        fputs(_("       /ad - directories    /ar - readonly   /ah - hidden\n"), stdout);
        fputs(_("       /as - system files\n"), stdout);
        fputs(_("   /o[-][nesdg]... sort files, the keys in order (- reverses the next)\n"),
              stdout);
        fputs(_("       /on - by name         /oe - extension  /od - date (oldest first)\n"),
              stdout);
        fputs(_("       /os - by size         /og - directories first; /o alone: /ogn\n"), stdout);
        fputs(_("Examples:\n"), stdout);
        fputs(_("> dir /w /os *.exe\n"), stdout);
        fputs(_("> dir /p /b *.txt *.com\n"), stdout);
}

static void dir_format_date_time(long ff_fdate, long ff_ftime, char *time, char *date) {
        int year, month, day;
        int hour, minute;
        bool longyear = true;

        year = (ff_fdate >> 9) + 80;
        if (longyear)
                year += 1900;
        else
                year %= 100;
        day = ff_fdate & 0x001f;
        month = (ff_fdate >> 5) & 0x000f;
        hour = ff_ftime >> 5 >> 6;
        minute = (ff_ftime >> 5) & 0x003f;

        snprintf(date, 10, "%d/%d/%d", day, month, year);
        snprintf(time, 10, "%02d:%0d", hour, minute);
}

static int dir_file_comperator(const void *a, const void *b) {
        const struct file_entry *file1 = (const struct file_entry *)(a);
        const struct file_entry *file2 = (const struct file_entry *)(b);
        bool dir1 = S_ISDIR(file1->file_details.st_mode);
        bool dir2 = S_ISDIR(file2->file_details.st_mode);
        const char *k;

        for (k = dir_sort_keys; *k; k++) {
                int v = 0;
                switch (toupper((unsigned char)*k)) {
                case 'N':
                        v = strcasecmp(file_base_name(file1->file_name),
                                       file_base_name(file2->file_name));
                        break;
                case 'E':
                        v = strcasecmp(file_get_extension(file1->file_name),
                                       file_get_extension(file2->file_name));
                        break;
                case 'S': {
                        /* a directory as 0 bytes, as DOS shows it */
                        long long s1 = dir1 ? 0 : (long long)file1->file_details.st_size;
                        long long s2 = dir2 ? 0 : (long long)file2->file_details.st_size;
                        v = s1 < s2 ? -1 : s1 > s2 ? 1 : 0;
                        break;
                }
                case 'D':
                        v = file1->file_details.st_mtime < file2->file_details.st_mtime   ? -1
                            : file1->file_details.st_mtime > file2->file_details.st_mtime ? 1
                                                                                          : 0;
                        break;
                case 'G':
                        v = dir1 == dir2 ? 0 : dir1 ? -1 : 1;
                        break;
                }
                if (v != 0) {
                        return islower((unsigned char)*k) ? -v : v;
                }
        }
        return 0;
}

/* the letters of /O (after the O): keys as dir_file_comperator takes
 * them; /O alone is /OGN, directories first and then by name */
static bool dir_parse_sort(const char *arg, char keys[SORT_KEYS]) {
        size_t n = 0;
        bool reverse = false;

        if (*arg == ':') {
                arg++;
        }
        if (*arg == 0) {
                strcpy(keys, "GN");
                return true;
        }
        for (; *arg; arg++) {
                char c = (char)toupper((unsigned char)*arg);
                if (c == '-') {
                        reverse = true;
                        continue;
                }
                if (!strchr("NESDG", c) || n + 1 >= SORT_KEYS) {
                        return false;
                }
                keys[n++] = reverse ? (char)tolower((unsigned char)c) : c;
                reverse = false;
        }
        keys[n] = 0;
        return n > 0;
}

static bool found(const char *file_name, const struct file_entry files[], size_t file_count) {
        size_t i;
        for (i = 0; i < file_count; i++) {
                if (strcmp(file_name, files[i].file_name) == 0) {
                        return true;
                }
        }
        return false;
}
